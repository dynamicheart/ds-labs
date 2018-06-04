#!/bin/bash

function checkError() {
  if [ $? -ne 0 ]; then
    echo -e "\033[34m[Hadoop APP Controller]\033[0m: App failed"
    exit 1
  fi
}

if [ -z "$APP_PATH" ]; then
  echo -e "\033[34m[Hadoop APP Controller]\033[0m: App path not specified"
  exit 2
fi

if [ -z "$DATA_PATH" ]; then
  echo -e "\033[34m[Hadoop APP Controller]\033[0m: Data path not specified"
  exit 2
fi

if [ "`hdfs dfs -ls /`" == "" ]; then
  echo -e "\033[34m[Hadoop APP Controller]\033[0m: Upload data to hdfs"
  hdfs dfs -put $DATA_PATH/device.txt /
  checkError
  hdfs dfs -put $DATA_PATH/dvalues.txt /
  checkError
else
  echo -e "\033[34m[Hadoop APP Controller]\033[0m: Data has already been uploaded before"
fi

echo -e "\033[34m[Hadoop APP Controller]\033[0m: Running dvalue sum app"
time hadoop jar $APP_PATH/dvaluesum/build/libs/DValueSumApp.jar DValueSumApp /device.txt /dvalues.txt /out/dvaluesum
checkError

echo -e "\033[34m[Hadoop APP Controller]\033[0m: Dvalue sum app ends successfully"
echo -e "\033[34m[Hadoop APP Controller]\033[0m: Print the result of dvalue sum app"
hdfs dfs -cat /out/dvaluesum/part-r-00000
checkError

echo -e "\033[34m[Hadoop APP Controller]\033[0m: Running dvalue avg app"
time hadoop jar $APP_PATH/dvalueavg/build/libs/DValueAvgApp.jar DValueAvgApp /device.txt /dvalues.txt /out/dvalueavg
checkError

echo -e "\033[34m[Hadoop APP Controller]\033[0m: Dvalue avg app ends successfully"
echo -e "\033[34m[Hadoop APP Controller]\033[0m: Print the result of dvalue avg app"
hdfs dfs -cat /out/dvalueavg/part-r-00000
checkError

