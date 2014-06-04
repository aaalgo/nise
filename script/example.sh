#!/bin/sh

# SET THE FOLLOWING THREE PARAMETERS BEFORE YOU RUN

IMAGE_DIR="$HOME/src/ida/neardupimages1"
HADOOP_WORK_DIR=nise-tmp
OUTPUT_DIR=.

PARTITIONS=10

if [ -z "$IMAGE_DIR" -o -z "$HADOOP_WORK_DIR" ]
then
    echo "Please edit the file to set parameters."
    exit
fi

if [ -z "$NISE_HOME" -o -z "$NISE_HADOOP_HOME" ]
then
    echo "Please export NISE_HOME and NISE_HADOOP_HOME to the environment."
    exit
fi

#TOTAL=4250

if false
then
hadoop -mkdir $HADOOP_WORK_DIR
find $IMAGE_DIR -type f | awk '{print $1, $1;}' | $NISE_HOME/bin/import --output $HADOOP_WORK_DIR/import
TOTAL=`$NISE_HOME/bin/number --partitions $PARTITIONS $HADOOP_WORK_DIR/import $HADOOP_WORK_DIR/number`
echo $TOTAL

if [ -z "$TOTAL" ]; then exit; fi

$NISE_HOME/bin/group --conf mapred.reduce.tasks=$PARTITIONS --total $TOTAL $HADOOP_WORK_DIR/number $HADOOP_WORK_DIR/group
$NISE_HOME/bin/graph-hash --conf mapred.reduce.tasks=$PARTITIONS $HADOOP_WORK_DIR/number $HADOOP_WORK_DIR/graph-hash


$NISE_HOME/bin/graph-join --conf mapred.reduce.tasks=$PARTITIONS $HADOOP_WORK_DIR/graph-hash $HADOOP_WORK_DIR/graph-join

$NISE_HOME/bin/download $HADOOP_WORK_DIR/group images
$NISE_HOME/bin/group-index < images > images.idx
$NISE_HOME/bin/download $HADOOP_WORK_DIR/graph-join graph

for off in 0 32 64 96
do

$NISE_HOME/bin/sketch-index --conf mapred.reduce.tasks=$PARTITIONS --offset $off $HADOOP_WORK_DIR/number $HADOOP_WORK_DIR/sketch.$off
$NISE_HOME/bin/download $HADOOP_WORK_DIR/sketch.$off sketch.$off
fbi-trie -I sketch.$off -O sketch.$off.trie -F $off

done

DIR=`pwd`

cat > db <<FOO
16
4
1000
$DIR
0 0 sketch.0 sketch.0.trie
32 0 sketch.32 sketch.32.trie
64 0 sketch.64 sketch.64.trie
96 0 sketch.96 sketch.96.trie
FOO

cat > server.xml <<FOO
<config>
    <nise>
        <sketch>
            <db>$DIR/db</db>
        </sketch>
        <expansion>
            <db>$DIR/graph</db>
        </expansion>
        <static>
            <root>$NISE_HOME/html</root>
            <preload>false</preload>
        </static>
        <image>
            <db>$DIR/images</db>
            <index>$DIR/images.idx</index>
        </image>
        <retrieval>
            <cache>10000</cache>
        </retrieval>
        <session>
            <cache>60000</cache>
        </session>
        <server>
            <port>80</port>
        </server>
    </nise>
</config>
FOO

echo If no error occurred, databases are built.  You can start server at this directory.

