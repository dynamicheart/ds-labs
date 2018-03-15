# Reliable Data Transport Protocol

### 设计策略
- **主要策略**
使用了go-back-n的方式，经测试，windows size设置为4，而time out设置为0.2，所用时间相对最少，发送包的数量相对最少。但有可能是因为退化成了stop-and-wait，因此还是讲windows size设置为10， timeout设置为0.3，来保证吞吐量。

- **对于GO-BACK-N方法的优化**

在接收方中设置一个等于WINDOWS_SIZE的缓存,用于存放发送方超前发出的包，这样大大降低了总耗时。

- **发包以及收包机制** 使用GO-BACK-N的方法。
  - 发送方每次收到从上层来的message，将message放入缓存。再检测timer有没有设置，如果没有设置，则说明当前windows为0，设置timer，此时填充windows，并且发包。
  - 发送方如果在timeout之前收到接收方正确的ack，windows“右移”，此时只会发送新填充进入windows的包，重新设置timer。
  - 发送方在timeout之后没有收到相应的ack，则将windows中的所有包全部发送，并重新设置timer。
  - 接收方按顺序接收包，按顺序发送ack，比 expcted < packet < expcted + windows_size 的包可以放入缓存中。

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
- **Message Buffer** 由于UpperLayer和LowerLayer的发送速率可能不一致，所以设置了一个15000个message的数组buffer，循环利用，所以上层发送消息的速率不能太高，不然可能会丢失message。同时为了节省空间，message里面的data是动态分配的，有新的message到来，才会分配其data的空间。


- **接收方对于Message的划分** 接收方需要知道哪些packet属于哪个message，这通过判断接收到的数据是否已经等于message的size来判定，而message的size可以通过存放在每个message的第一个包的payload前四个byte中，只需要传输一次，其它的packet不需要记录这个size。提高了有效空间。

- **将Message的size当成message的data来传输** 在sender收到一条要发送的message时，做一些处理，message的size变成size + 4，message的data变成size，然后再接上message的原始数据，因此，message的真实大小存放在message.data[0:3]中。


### 测试
相关测试结果如下，吞吐量正常，总耗时相对于参考值来说大大降低（这是由于在接收端设置buffer的原因）。

```shell
>./rdt_sim 1000 0.1 100 0.15 0.15 0.15 0
## Reliable data transfer simulation with:
	simulation time is 1000.000 seconds
	average message arrival interval is 0.100 seconds
	average message size is 100 bytes
	average out-of-order delivery rate is 15.00%
	average loss rate is 15.00%
	average corrupt rate is 15.00%
	tracing level is 0
Please review these inputs and press <enter> to proceed.

At 0.00s: sender initializing ...
At 0.00s: receiver initializing ...
At 1025.48s: sender finalizing ...
At 1025.48s: receiver finalizing ...

## Simulation completed at time 1025.48s with
	1004553 characters sent
	1004553 characters delivered
	49967 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.
```

```shell
>./rdt_sim 1000 0.1 100 0.3 0.3 0.3 0
## Reliable data transfer simulation with:
	simulation time is 1000.000 seconds
	average message arrival interval is 0.100 seconds
	average message size is 100 bytes
	average out-of-order delivery rate is 30.00%
	average loss rate is 30.00%
	average corrupt rate is 30.00%
	tracing level is 0
Please review these inputs and press <enter> to proceed.

At 0.00s: sender initializing ...
At 0.00s: receiver initializing ...
At 1806.43s: sender finalizing ...
At 1806.43s: receiver finalizing ...

## Simulation completed at time 1806.43s with
	1000185 characters sent
	1000185 characters delivered
	60593 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.
```
