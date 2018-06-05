### Deployment Design

|Hostname    |Yarn           |HDFS    |
|------------|---------------|--------|
|hadoopmaster|ResourceManager|NameNode<br>Secondary NameNode<br>History Server<br>timeline server|
|hadoopslave1|NodeManager    |DataNode|
|hadoopslave2|NodeManager    |DataNode|
|hadoopslave3|NodeManager    |DataNode|


### Startup Sequence
- ***Logical startup sequence***
  
   NameNode ->> DataNode ->> ResourceManager and NodeManger

- ***Physical startup Sequence***

  All hadoopslaves ->> hadoopmaster
  
