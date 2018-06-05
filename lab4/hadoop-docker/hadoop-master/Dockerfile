FROM dynamicheart/hadoop-base:1.0.0-hadoop2.7.6-java8
MAINTAINER Jianbang Yang <yangjianbang112@gmail.com>

# Copy configuring data
COPY etc/hadoop/* $HADOOP_CONF_DIR/

RUN mkdir -p /hadoop/dfs/name
VOLUME /hadoop/dfs/name

RUN mkdir -p /hadoop/yarn/timeline
VOLUME /hadoop/yarn/timeline

ADD run.sh /run.sh
RUN chmod a+x /run.sh

EXPOSE 50070
EXPOSE 8088
EXPOSE 8188

ENTRYPOINT service ssh restart && ./run.sh && bash
