FROM ubuntu:16.04
MAINTAINER Jianbang Yang <yangjianbang112@gmail.com>

# Install openjdk
RUN apt-get update && apt-get install -y software-properties-common && add-apt-repository ppa:openjdk-r/ppa
RUN apt-get update && apt-get install -y openjdk-8-jdk
ENV JAVA_HOME /usr/lib/jvm/java-8-openjdk-amd64

RUN apt-get update && apt-get -y install wget

# Download hadoop. For fast downloading, you can use tsinghua mirror.
# https://mirrors.tuna.tsinghua.edu.cn/apache/hadoop/common/
ENV HADOOP_VERSION 2.7.6
RUN wget -P /tmp/ https://www.apache.org/dist/hadoop/common/hadoop-$HADOOP_VERSION/hadoop-$HADOOP_VERSION.tar.gz \
    && tar -xvf /tmp/hadoop-$HADOOP_VERSION.tar.gz -C /opt/ \
    && rm /tmp/hadoop-$HADOOP_VERSION.tar.gz*

RUN ln -s /opt/hadoop-$HADOOP_VERSION/etc/hadoop /etc/hadoop
RUN mkdir /opt/hadoop-$HADOOP_VERSION/logs

ENV HADOOP_PREFIX=/opt/hadoop-$HADOOP_VERSION
ENV HADOOP_CONF_DIR=/etc/hadoop
ENV YARN_CONF_DIR=/etc/hadoop

ENV PATH $HADOOP_PREFIX/bin/:$PATH

# SSH connect without key
RUN apt-get update && apt-get install -y openssh-server \
    && ssh-keygen -t rsa -f ~/.ssh/id_rsa -P '' \
    && cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys \
    && touch ~/.ssh/config
ADD etc/ssh/config /root/.ssh/config
