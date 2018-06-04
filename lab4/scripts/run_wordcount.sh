#!/bin/bash

usage="Usage: run_wordcount.sh [intput data path] [output data path]"

if [ $# != 2 ] ; then
  echo $usage
  exit 1
fi

function checkError() {
  if [ $? -ne 0 ]; then
    echo -e "\033[34m[Hadoop APP Controller]\033[0m: App failed"
    exit 1
  fi
}

hdfs dfs -rm -r $2

hadoop jar $HADOOP_PREFIX/share/hadoop/mapreduce/sources/hadoop-mapreduce-examples-2.7.6-sources.jar org.apache.hadoop.examples.WordCount $1 $2
checkError

hdfs dfs -cat $2/part-r-00000

