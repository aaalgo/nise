#!/bin/sh

# SET THE FOLLOWING THREE PARAMETERS BEFORE YOU RUN

export NISE_HOME=$HOME/nise-x86_64/nise
export NISE_HADOOP_HOME=nise
IMAGE_DIRS="$HOME/images/neardupimages1 $HOME/images/neardupimages2"
DEMO_IMAGES=$HOME/demo


HADOOP_WORK_DIR=nise-tmp
WORK_DIR=tmp
OUTPUT_DIR=data

PARTITIONS=100
EXTRACT_BATCH=1000
EXTRACT_TASKS=2
#SAMPLE=10000

if [ "$1" = wipe ]
then
    rm -rf $WORK_DIR $OUTPUT_DIR
    hadoop fs -rmr $HADOOP_WORK_DIR
fi

if [ -z "$IMAGE_DIRS" -o -z "$HADOOP_WORK_DIR" ]
then
    echo "Please edit the file to set parameters."
    exit
fi

if [ -z "$NISE_HOME" -o -z "$NISE_HADOOP_HOME" ]
then
    echo "Please export NISE_HOME and NISE_HADOOP_HOME to the environment."
    exit
fi

hadoop fs -rmr $NISE_HADOOP_HOME
hadoop fs -put $NISE_HOME/bin $NISE_HADOOP_HOME/bin

if [ "$1" != cont ]
then

if [ -e $WORK_DIR ]
then
    echo "WORK_DIR $WORK_DIR exists."
    exit
fi

if [ -e $OUTPUT_DIR ]
then
    echo "OUTPUT_DIR $OUTPUT_DIR exists."
    exit
fi

mkdir -p $WORK_DIR
mkdir -p $OUTPUT_DIR

fi

if false
then
    true
fi



# find all images
T=`date +'%s'`
for DIR in $IMAGE_DIRS
do
    find $DIR -type f | awk '{print $1, $1;}' >> $WORK_DIR/list 
done

if [ -n "$SAMPLE" ]
then
    echo Only sampling $SAMPLE images
    shuf $WORK_DIR/list | head -n $SAMPLE > $WORK_DIR/listx
    mv $WORK_DIR/listx $WORK_DIR/list
fi

T2=`date +'%s'`
echo list $((T2-T)) >> $WORK_DIR/log

split -l $EXTRACT_BATCH $WORK_DIR/list $WORK_DIR/split

hadoop fs -mkdir $HADOOP_WORK_DIR/import

T=`date +'%s'`
ls $WORK_DIR/split* | parallel -j $EXTRACT_TASKS $NISE_HOME/bin/import --input {} --output $HADOOP_WORK_DIR/import/{/} 
T2=`date +'%s'`
echo import $((T2-T)) >> $WORK_DIR/log
T=$T2

TOTAL=`$NISE_HOME/bin/number --partitions $PARTITIONS $HADOOP_WORK_DIR/import $HADOOP_WORK_DIR/number`
T2=`date +'%s'`
echo total $((T2-T)) >> $WORK_DIR/log
echo "#images" $TOTAL >> $WORK_DIR/log
T=$T2

if [ -z "$TOTAL" ]; then exit; fi

$NISE_HOME/bin/group --conf mapred.reduce.tasks=$PARTITIONS --total $TOTAL $HADOOP_WORK_DIR/number $HADOOP_WORK_DIR/group
T2=`date +'%s'`
echo group $((T2-T)) >> $WORK_DIR/log
T=$T2
$NISE_HOME/bin/graph-hash --conf mapred.reduce.tasks=$PARTITIONS $HADOOP_WORK_DIR/number $HADOOP_WORK_DIR/graph-hash
T2=`date +'%s'`
echo group-hash $((T2-T)) >> $WORK_DIR/log
T=$T2
$NISE_HOME/bin/graph-join --conf mapred.reduce.tasks=$PARTITIONS $HADOOP_WORK_DIR/graph-hash $HADOOP_WORK_DIR/graph-join
T2=`date +'%s'`
echo group-join $((T2-T)) >> $WORK_DIR/log

$NISE_HOME/bin/download $HADOOP_WORK_DIR/group $OUTPUT_DIR/images
$NISE_HOME/bin/group-index < $OUTPUT_DIR/images > $OUTPUT_DIR/images.idx
$NISE_HOME/bin/download $HADOOP_WORK_DIR/graph-join $OUTPUT_DIR/graph

for off in 0 32 64 96
do
T=`date +'%s'`
$NISE_HOME/bin/sketch-index --conf mapred.reduce.tasks=$PARTITIONS --offset $off $HADOOP_WORK_DIR/number $HADOOP_WORK_DIR/sketch.$off
T2=`date +'%s'`
echo sketch-index $((T2-T)) >> $WORK_DIR/log
$NISE_HOME/bin/download $HADOOP_WORK_DIR/sketch.$off $OUTPUT_DIR/sketch.$off
$NISE_HOME/bin/fbi-trie -I $OUTPUT_DIR/sketch.$off -O $OUTPUT_DIR/sketch.$off.trie -F $off

done

cat > $OUTPUT_DIR/db <<FOO
16
4
1000
.
0 0 sketch.0 sketch.0.trie
32 0 sketch.32 sketch.32.trie
64 0 sketch.64 sketch.64.trie
96 0 sketch.96 sketch.96.trie
FOO

mkdir $OUTPUT_DIR/demo
mkdir -p $OUTPUT_DIR/log/record
find $DEMO_IMAGES -type f | shuf | head -n 100 | while read a; do cp $a $OUTPUT_DIR/demo ; done

cat > $OUTPUT_DIR/server.xml <<FOO
<config>
    <nise>
        <sketch>
            <db>db</db>
        </sketch>
        <expansion>
            <db>graph</db>
        </expansion>
        <static>
            <root>$NISE_HOME/html</root>
            <preload>false</preload>
        </static>
        <demo>
            <root>demo</root>
        </demo>
        <image>
            <db>images</db>
            <index>images.idx</index>
        </image>
        <log>
            <dir>log</dir>
        </log>
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

