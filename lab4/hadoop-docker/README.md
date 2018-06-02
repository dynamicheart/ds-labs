### Deployment Design

|Hostname    |Yarn           | HDFS   |
|------------|---------------|--------|
|hadoopmaster|ResourceManager|NameNode|
|hadoopslave1|NodeManager    |DataNode|
|hadoopslave2|NodeManager    |DataNode|


### Startup Sequence
- ***Logical startup sequence***
  NameNode ->> DataNode ->> ResourceManager and NodeManger

- ***Physical startup Sequence***