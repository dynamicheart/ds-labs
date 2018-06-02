FROM dynamicheart/hadoop-base:1.0.0-hadoop2.7.6-java8
MAINTAINER Jianbang Yang <yangjianbang112@gmail.com>

# Copy configuring data
COPY etc/hadoop/* $HADOOP_CONF_DIR/

RUN mkdir -p /hadoop/dfs/name

ADD run.sh /run.sh
RUN chmod a+x /run.sh

ENTRYPOINT service ssh restart && ./run.sh && bash
