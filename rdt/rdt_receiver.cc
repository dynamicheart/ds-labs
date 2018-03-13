/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: This implementation assumes there is no packet loss, corruption, or
 *       reordering.  You will need to enhance it to deal with all these
 *       situations.  In this implementation, the ack packet format is laid out as
 *       the following:
 *
 *       |<-  2 byte -->|<-  4 byte  ->|<-             the rest            ->|
 *       |   check sum  |     ack      |<-             nothing             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdt_struct.h"
#include "rdt_receiver.h"

#define HEADER_SIZE 7
#define MAX_PAYLOAD_SIZE (RDT_PKTSIZE - HEADER_SIZE)

//当前消息
static message* cur_message;
//当前message还未收到的数据的第一个byte的偏移量
static int cursor_receiver;

//应该收到的packet
static int packet_expected;

static short checksum(struct packet* pkt) {
    unsigned long sum = 0;
    for(int i = 2; i < RDT_PKTSIZE; i += 2) sum += *(short *)(&(pkt->data[i]));
    while(sum >> 16) sum = (sum >> 16) + (sum & 0xffff);
    return ~sum;
}

static void send_ack(int ack) {
    packet ack_packet;
    memcpy(ack_packet.data + sizeof(short), &ack, sizeof(int));
    short sum = checksum(&ack_packet);
    memcpy(ack_packet.data, &sum, sizeof(short));

    Receiver_ToLowerLayer(&ack_packet);
}

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    cur_message = (message *)malloc(sizeof(message));
    memset(cur_message, 0, sizeof(message));
    cursor_receiver = 0;

    packet_expected = 0;
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    free(cur_message);
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a packet is passed from the lower layer at the
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    //校验失败直接丢弃
    short sum;
    memcpy(&sum, pkt->data, sizeof(short));
    if(sum != checksum(pkt)) return;

    int packet_seq = 0, payload_size = 0;
    memcpy(&packet_seq, pkt->data + sizeof(short), sizeof(int));
    if(packet_seq != packet_expected) {
        send_ack(packet_expected - 1);
        return;
    }

    packet_expected++;
    payload_size = pkt->data[HEADER_SIZE - 1];

    //第一个包
    if(cursor_receiver == 0){
        if(cur_message->size != 0) free(cur_message->data);
        memcpy(&cur_message->size, pkt->data + HEADER_SIZE, sizeof(int));
        cur_message->data = (char*) malloc(cur_message->size);
        memcpy(cur_message->data, pkt->data + HEADER_SIZE + sizeof(int), payload_size - sizeof(int));
        cursor_receiver += payload_size - sizeof(int);
    } else {
        memcpy(cur_message->data + cursor_receiver, pkt->data + HEADER_SIZE, payload_size);
        cursor_receiver += payload_size;
    }

    if(cur_message->size == cursor_receiver) {
        Receiver_ToUpperLayer(cur_message);
        cursor_receiver = 0;
    }

    send_ack(packet_seq);
}
