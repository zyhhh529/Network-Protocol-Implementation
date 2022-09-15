
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <stdio.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <unistd.h>
	#include <pthread.h>
	#include <fcntl.h>
	#include "string"
	#include <cstring>
	#include <iostream>
	#include <netdb.h>
	
	#define D_SIZE 2000
	#define TOTAL_COUNT 300 
	#define BUFF_SIZE 600000 
	#define ACK 2
	#define FIN_ACK 4
	#define DATA 0
	#define FIN 3
	
	using namespace std;
	
	typedef struct {
	 int data_size;
	 int seq_num;
	 int ack_num;
	 int msg_type;
	 char data[D_SIZE];
	} Packet;
	
	struct sockaddr_in si_me, si_other;
	int s, slen, recvBytes;
	char buf[sizeof(Packet)];
	struct sockaddr_in senderAddr;
	socklen_t addrLen;
	
	void reliablyReceive(unsigned short int myUDPport, char *destinationFile) {
	
	 slen = sizeof(si_other);
	
	
	 if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
	 printf("Socket\n");
	 exit(1);
	 }
	
	 memset((char *) &si_me, 0, sizeof(si_me));
	 si_me.sin_family = AF_INET;
	 si_me.sin_port = htons(myUDPport);
	 si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	 printf("Now binding\n");
	 if (::bind(s, (struct sockaddr *) &si_me, sizeof(si_me)) <0) {
	 printf("Bind\n");
	 exit(1);
	 }

	
	 /* receive data & send acknowledgements */
	
	 FILE *file = fopen(destinationFile, "wb");
	 
	 char fBuffer[BUFF_SIZE]; 
	 int nextAck = 0;
	 int ifacked[TOTAL_COUNT]; 
	 int dSize[TOTAL_COUNT]; 
	 
	 for (int i = 0; i < TOTAL_COUNT; ++i) {
	 ifacked[i] = 0;
	 dSize[i] = D_SIZE;
	 }
	 
	 addrLen = sizeof(senderAddr);
	 
	 int curIndex = 0;
	 
	 while (true) {
	 recvBytes = recvfrom(s, buf, sizeof(Packet), 0, (struct sockaddr *) &senderAddr, &addrLen);
	 
	 // stop the  connection
	 if (recvBytes <= 0) {
	 printf("Connection stoped\n");
	 exit(2);
	 }
	 Packet packet;
	 memcpy(&packet, buf, sizeof(Packet));
	
	 if (packet.msg_type == DATA) {
	 
	 // when receiving the correct sequence number
	 if (packet.seq_num == nextAck) {
	 
	 memcpy(&fBuffer[curIndex * D_SIZE], &packet.data, packet.data_size);
	 
	 fwrite(&fBuffer[curIndex * D_SIZE], sizeof(char), packet.data_size, file);
	 
	 nextAck++;
	 
	 curIndex = (curIndex + 1) % TOTAL_COUNT;
	 
	 // when receiving the package with next seq num
	 while (ifacked[curIndex] == 1) {
	 
	 fwrite(&fBuffer[curIndex * D_SIZE], sizeof(char), packet.data_size, file);
	 ifacked[curIndex] = 0;
	 curIndex = (curIndex + 1) % TOTAL_COUNT;
	 nextAck++;
	 
	 }
	 
	 // get wrong sequence number
	 } else if (packet.seq_num > nextAck) {
	 
	 // the index of pkg
	 int headIndex = (curIndex + packet.seq_num - nextAck) % TOTAL_COUNT;
	 
	 // write data
	 for (int i = 0; i < packet.data_size; ++i) {
	 
	 fBuffer[headIndex * D_SIZE + i] = packet.data[i];
	 
	 }
	 ifacked[headIndex] = 1;
	 dSize[headIndex] = packet.data_size;
	 }
	 
	 // send ack 
	 Packet ackPackage;
	 ackPackage.msg_type = ACK;
	 ackPackage.ack_num = nextAck;
	 ackPackage.data_size = 0;
	 memcpy(buf, &ackPackage, sizeof(Packet));
	 sendto(s, buf, sizeof(Packet), 0, (struct sockaddr *) &senderAddr, addrLen);
	 
	 // stop connection
	 } else if (packet.msg_type == FIN) {
	 
	 Packet ackPackage;
	 ackPackage.msg_type = FIN_ACK;
	 ackPackage.ack_num = nextAck;
	 ackPackage.data_size = 0;
	 
	 memcpy(buf, &ackPackage, sizeof(Packet));
	 sendto(s, buf, sizeof(Packet), 0, (struct sockaddr *) &senderAddr, addrLen);
	 break;
	 
	 }
	 }
	 close(s);
	 printf("%s received.", destinationFile);
	 return;
	}
	
	

	int main(int argc, char **argv) {
	
	 unsigned short int udpPort;
	
	 if (argc != 3) {
	 fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
	 exit(1);
	 }
	
	 udpPort = (unsigned short int) atoi(argv[1]);
	
	 reliablyReceive(udpPort, argv[2]);
	}
