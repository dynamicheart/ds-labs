/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or
 *       reordering.  You will need to enhance it to deal with all these
 *       situations.  In this implementation, the packet format is laid out as
 *       the following:
 *
 *   |<- 2 byte -> |<- 4 byte  ->|<-  1 byte  ->|<-  1 byte  ->|><-  The rest   ->|
 *   |  checksum   |  packet seq |  message seq | payload size |<-   payload    ->|
 *
 *
 *   The first packet of a new message:
 *   |<- 2 byte -> |<- 4 byte  ->|<-  1 byte  ->|<-  1 byte  ->|<-  4 byte  ->|<-  The rest   ->|
 *   |  checksum   |  packet seq |  message seq | payload size | message size |<-   payload   ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdt_struct.h"
#include "rdt_sender.h"

#define MAX_BUFFER_SIZE 15000
#define MAX_WINDOW_SIZE 2
#define TIMEOUT 0.2

#define HEADER_SIZE 8
#define MAX_PAYLOAD_SIZE (RDT_PKTSIZE - HEADER_SIZE)

static message* buffer;
//消息序号
static int next_message_seq;
//当前buffer中message的数量
static int nmessage;
//当前还未完全发送完成的message在buffer中的位置
static int cur_message_seq;
//当前message还未进入windows的第一个byte的偏移量
static int cursor_sender;

static packet* windows;
//包序号
static int next_packet_seq;
//下一次要发的包的序列
static int next_packet_to_send;
//应该收到的ack
static int ack_expected;
//当前windows中packet的数目
static int npacket;

static short checksum(struct packet* pkt) {
    unsigned long sum = 0;
    for(int i = 2; i < RDT_PKTSIZE; i += 2) sum += *(short *)(&(pkt->data[i]));
    while(sum >> 16) sum = (sum >> 16) + (sum & 0xffff);
    return ~sum;
}

static void fill_windows() {
    message msg = buffer[cur_message_seq % MAX_BUFFER_SIZE];

    //复用
    packet pkt;
    short sum;

    while (npacket < MAX_WINDOW_SIZE && cur_message_seq < next_message_seq) {
        if(msg.size - cursor_sender > MAX_PAYLOAD_SIZE){
            memcpy(pkt.data + sizeof(short), &next_packet_seq, sizeof(int));
            pkt.data[sizeof(short) + sizeof(int)] = cur_message_seq % 256;

            pkt.data[HEADER_SIZE - 1] = MAX_PAYLOAD_SIZE;

            memcpy(pkt.data + HEADER_SIZE, msg.data + cursor_sender, MAX_PAYLOAD_SIZE);

            sum = checksum(&pkt);
            memcpy(pkt.data, &sum, sizeof(short));

            memcpy(&(windows[next_packet_seq % MAX_WINDOW_SIZE]), &pkt, sizeof(packet));

            cursor_sender += MAX_PAYLOAD_SIZE;
            next_packet_seq++;
            npacket++;
        } else if (msg.size > cursor_sender) {
            memcpy(pkt.data + sizeof(short), &next_packet_seq, sizeof(int));
            pkt.data[sizeof(short) + sizeof(int)] = cur_message_seq % 256;
            pkt.data[HEADER_SIZE - 1] = msg.size - cursor_sender;

            memcpy(pkt.data + HEADER_SIZE, msg.data + cursor_sender, msg.size - cursor_sender);

            sum = checksum(&pkt);
            memcpy(pkt.data, &sum, sizeof(short));

            memcpy(&(windows[next_packet_seq % MAX_WINDOW_SIZE]), &pkt, sizeof(packet));

            //已经将某个message全部发送并且放入窗口
            nmessage--;
            cur_message_seq++;
            if(cur_message_seq < next_message_seq){
                msg = buffer[cur_message_seq % MAX_BUFFER_SIZE];
            }

            cursor_sender = 0;

            next_packet_seq++;
            npacket++;
        }
    }

}

static void send_packets() {
    //复用
    packet pkt;

    while(next_packet_to_send < next_packet_seq) {
        memcpy(&pkt, &(windows[next_packet_to_send % MAX_WINDOW_SIZE]), sizeof(packet));
        Sender_ToLowerLayer(&pkt);
        next_packet_to_send++;
    }
}

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    buffer = (message*) malloc((MAX_BUFFER_SIZE) * sizeof(message));
    memset(buffer, 0, (MAX_BUFFER_SIZE) * sizeof(message));
    next_message_seq = 0;
    nmessage = 0;
    cur_message_seq = 0;
    cursor_sender = 0;

    windows = (packet*) malloc((MAX_WINDOW_SIZE) * sizeof(packet));
    next_packet_seq = 0;
    next_packet_to_send = 0;
    ack_expected = 0;
    npacket = 0;

    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    for(int i = 0; i < MAX_BUFFER_SIZE; i++) {
        if(buffer[i].size != 0) free(buffer[i].data);
    }
    free(buffer);
    free(windows);
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a message is passed from the upper layer at the
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    //使用GO-BACK-N策略。
    if(nmessage >= MAX_BUFFER_SIZE) {
        ASSERT(0);
    }

    if(buffer[next_message_seq % MAX_BUFFER_SIZE].size != 0) free(buffer[next_message_seq % MAX_BUFFER_SIZE].data);
    buffer[next_message_seq % MAX_BUFFER_SIZE].size = msg->size + sizeof(int);
    buffer[next_message_seq % MAX_BUFFER_SIZE].data = (char *) malloc(msg->size + sizeof(int));
    memcpy(buffer[next_message_seq % MAX_BUFFER_SIZE].data, &msg->size, sizeof(int));
    memcpy(buffer[next_message_seq % MAX_BUFFER_SIZE].data + sizeof(int), msg->data, msg->size);
    next_message_seq++;
    nmessage++;

    //还未超时
    if(Sender_isTimerSet()) return;

    Sender_StartTimer(TIMEOUT);

    fill_windows();
    send_packets();
}

/* event handler, called when a packet is passed from the lower layer at the
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    //checksum失败直接丢掉
    short sum;
    memcpy(&sum, pkt->data, sizeof(short));
    if(sum != checksum(pkt)) return;

    int ack;
    memcpy(&ack, pkt->data + sizeof(short), sizeof(int));
    //收到以前的ack
    if(ack_expected <= ack && ack < next_packet_seq){
        //收到某一个包的ack，向后移动窗口
        Sender_StartTimer(TIMEOUT);
        npacket -= (ack - ack_expected + 1);
        ack_expected = ack + 1;
        fill_windows();
        send_packets();
    }

    if(ack == next_packet_seq - 1) Sender_StopTimer();

}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    Sender_StartTimer(TIMEOUT);
    next_packet_to_send = ack_expected;
    fill_windows();
    send_packets();
}
