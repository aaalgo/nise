#!/bin/sh

NISE_HOME=$1
NISE_HADOOP_HOME=$2

if [ -z "$NISE_HOME" -o -z "$NISE_HADOOP_HOME" ]
then
    echo "usage:  $0 <NISE_HOME> <NISE_HADOOP_HOME>"
    exit
fi


BIN_FILES="index/merge index/number index/graph-hash index/graph-join index/mapid index/sketch-index index/group index/group-index index/import index/import-nutch extract/extract index/download index/graph index/import-id index/cluster2id index/graph-index"

SBIN_FILES="server/server"
JAVA_FILES="java/*.jar"
HTML_FILES="html/*"

mkdir -p $NISE_HOME/bin $NISE_HOME/sbin $NISE_HOME/java $NISE_HOME/data $NISE_HOME/html

cp $BIN_FILES $NISE_HOME/bin
cp $SBIN_FILES $NISE_HOME/sbin
cp $JAVA_FILES $NISE_HOME/java
cp $HTML_FILES $NISE_HOME/html

hadoop fs -mkdir $NISE_HADOOP_HOME/bin

hadoop fs -D dfs.blocksize=2097152 -put $BIN_FILES $NISE_HADOOP_HOME/bin

echo "If no error messages occur, NISE should have been successfully installed."
echo "Now you should export NISE_HOME and NISE_HADOOP_HOME to your system environment."
