#!/bin/bash

echo "Hadoop Master start up"

namedir="/hadoop/dfs/name"

if [ "`ls -A $namedir`" == "" ]; then
  echo "Formatting namenode name directory: $namedir"
  $HADOOP_PREFIX/bin/hdfs --config $HADOOP_CONF_DIR namenode -format $CLUSTER_NAME 
fi

$HADOOP_PREFIX/sbin/start-dfs.sh
$HADOOP_PREFIX/sbin/start-yarn.sh
$HADOOP_PREFIX/sbin/yarn-daemon.sh start timelineserver
$HADOOP_PREFIX/sbin/mr-jobhistory-daemon.sh start historyserver
