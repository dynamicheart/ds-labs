# Reliable Data Transport Protocol

### 设计策略
- **主要策略**
使用了go-back-n的方式，经测试，windows size设置为5，而time out设置为0.2，所用时间相对最少，发送包的数量相对最少。

-  **包的设计**

由发送方发往接收方的包的格式如下,包头的大小为7个byte

|checksum|packet seq|payload size|payload|
|-|-|-|-|-|
|2 byte|4 byte|1 byte|The rest|

其中对每条新的message，发送方发往接收方的第一个包的格式如下，多了4个byte来记录新的message的大小，好让接收方知道该为这个新的message分配多少空间。

|checksum|packet seq|payload size|message size |payload|
|-|-|-|-|-|
|2 byte|4 byte|1 byte|4 byte|The rest|

由接收方发往发送方的包的格式如下，主要用来传递ack信息。

|checksum|ack|nothing|
|-|-|-|
|2 byte|4 byte|the rest|

- **checksum校验算法**

使用16-bit因特网checksum的算法
```c
static short checksum(struct packet* pkt) {
    unsigned long sum = 0;
    for(int i = 2; i < RDT_PKTSIZE; i += 2) sum += *(short *)(&(pkt->data[i]));
    while(sum >> 16) sum = (sum >> 16) + (sum & 0xffff);
    return ~sum;
}
```

### 实现策略
- **Message Buffer** 由于UpperLayer和LowerLayer的发送速率可能不一致，所以设置了一个15000个message的数组buffer，循环利用。同时为了节省空间，message里面的data是动态分配的，有新的message到来，才会分配其data的空间。

- **Message的划分** 接收方需要知道哪些packet属于哪个message，这通过判断接收到的数据是否已经等于message的size来判定，而message的size可以通过存放在每个message的第一个包的payload前四个byte中，只需要传输一次，其它的packet不需要记录这个size。

- **将Message的size当成message的data来传输** 在sender收到一条要发送的message时，做一些处理，message的size变成size + 4，message的data变成size，然后再接上message的原始数据，因此，message的真实大小存放在message.data[0:3]中。
