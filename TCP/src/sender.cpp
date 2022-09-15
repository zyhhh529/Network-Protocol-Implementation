#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <math.h>
#include <iostream>
#include <queue>

using namespace std;

#define DATA_SIZE 2000 //how many data each message could contain
#define BUFFER_SIZE 300
#define DATA 0
#define ACK 2
#define FIN 3
#define FIN_ACK 4
#define MAX_SEQ_NUMBER 100000
#define RTT 20000

//packet structure used for transfering
typedef struct{
    int 	data_size;
    int 	seq_num; // tcp sending
    int     ack_num; // tcp reply
    int     msg_type; //DATA 0 SYN 1 ACK 2 FIN 3 FINACK 4
    char    data[DATA_SIZE];
}packet;

FILE* fp;   // file pointer, pointing to a file

// variables to get sockets
struct addrinfo hints, *addr_list, *p;
int numbytes;

// Congestion Control related
int dupACK = 0;
double cwnd = 1.0;
queue<packet> wait_send;   // queue including pkts waiting for sending
queue<packet> wait_ack;    // queue including pkts sent but waiting for ack
int ssthread = 16;

unsigned long long int remainedBytes;

enum socket_state {SS, CA, FR, FIN_WAIT};
int trans_state = SS;

// slide window
long seq_number;
char pkt_buffer[sizeof(packet)]; 


int getSocket(char * hostname, int hostUDPport) {
    int rv, sockfd;
    // char portStr[10];
    char const *portStr = std::to_string(hostUDPport).c_str();
    cout<< "target udp port number is "<<portStr << endl;
    // set the hints for network config
    memset(&hints, 0, sizeof hints);
    memset(&addr_list,0,sizeof addr_list);

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(hostname, portStr, &hints, &addr_list)) != 0) {
        perror("cannot get link list of addresses");
        exit(0);
    }

    // loop through all the results and bind to the first we can
    for(p = addr_list; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            // current address cannot use, skip to the next one
            continue;
        }
        // find the first to use
        break;
    }
    if (p == NULL)  {
        perror("no available socket returned");
        exit(1);
    }

    return sockfd;
}

void addBuffer(int pkt_number) {
    if (pkt_number <= 0) return ;
    int byte_of_pkt;
    char data_buffer[DATA_SIZE];
//    int count = 0;
    // add pkt_number package into the buffer
    for (int i = 0; remainedBytes > 0 && i < pkt_number; i++) {
        packet pkt;
        if (remainedBytes < DATA_SIZE) {
            byte_of_pkt = remainedBytes;
        } else {
            byte_of_pkt = DATA_SIZE;
        }
        int file_size = fread(data_buffer, sizeof(char), byte_of_pkt, fp);
        if (file_size > 0) {
            pkt.data_size = file_size;
            pkt.msg_type = DATA;
            pkt.seq_num = seq_number;
            memcpy(pkt.data, &data_buffer,sizeof(char)*byte_of_pkt);
            wait_send.push(pkt);
            seq_number = (seq_number + 1) % MAX_SEQ_NUMBER;
        }
        remainedBytes -= file_size;
       // count = i;
    }
   // return count;
}

void sendPkts(int socket) {
    // wait_ack is the queue containing msg wating for ACKs
    // buffer contains msg waiting for sent

    int pending_pkts =(cwnd - wait_ack.size()) <= wait_send.size() ? cwnd - wait_ack.size() : wait_send.size();
    int sendingBytes;
    // resend
    if (cwnd - wait_ack.size() < 1) {
        // window size too large, therefore resend for waiting queue
        memcpy(pkt_buffer, &wait_ack.front(), sizeof(packet));
        if((numbytes = sendto(socket, pkt_buffer, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
            perror("resening errors");
            return;
        }
        return;
    }
    
    if (wait_send.empty()) {
        return;
    }

    int cur = 0;
    while(cur < pending_pkts){
        memcpy(pkt_buffer, &wait_send.front(), sizeof(packet));
        if ((numbytes = sendto(socket, pkt_buffer, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
            perror("Error: data sending");
            return;
            
        }
        // pkts sent adding into wait_ack queue
        wait_ack.push(wait_send.front());
        wait_send.pop();
        addBuffer(pending_pkts);
        cur++;
    }

}

//the fucntion of updating the window states
void stateUpdate(bool newACK, bool timeout) {
    switch (trans_state) {
        // congestion avoid
        case CA:
            if (timeout) {
                ssthread = cwnd/2.0;
                cwnd = 1;
                dupACK = 0;
                cout << "CA to SS, cwnd=" << cwnd <<endl;
                trans_state = SS;
                return;
            }
            if (newACK) {
                cwnd = (cwnd+ 1.0/cwnd >= BUFFER_SIZE) ? BUFFER_SIZE-1 : cwnd+ 1.0/cwnd;
                dupACK = 0;
            } else {
                dupACK++;
            }
            break;
        // slow start
        case SS:
            if (timeout) {
                ssthread = cwnd/2.0;
                cwnd = 1;
                dupACK = 0;
                return;
            }
            if (newACK) {
                dupACK = 0;
                cwnd = (cwnd+1 >= BUFFER_SIZE) ? BUFFER_SIZE-1 : cwnd+1;
            } else {
                dupACK++;
            }
            if (cwnd >= ssthread) {
                cout << "SS to CA, cwnd = " << cwnd <<endl;
                trans_state = CA;
            }
            break;
        // fast recovery
        case FR:
            if (timeout) {
                ssthread = cwnd/2.0;
                cwnd = 1;
                dupACK = 0;
                cout << "FR is SS, cwnd= " << cwnd <<endl;
                trans_state = SS;
                return;
            }
            if (newACK) {
                cwnd = ssthread;
                dupACK = 0;
                cout << "FR is CA, cwnd = " << cwnd << endl;
                trans_state = CA;
            } else {
                cwnd = (cwnd+1 >= BUFFER_SIZE) ? BUFFER_SIZE-1 : cwnd+1;
            }
            break;
        default:
            break;
    }
}

void reliablyTransfer(char* hostname, int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {

    int socket = getSocket(hostname, hostUDPport);

    //Open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("file does not exits or opening error");
        return;
    }
    remainedBytes = bytesToTransfer;

    // set time_clock
    struct timeval RTT_TO;
    RTT_TO.tv_sec = 0;
    RTT_TO.tv_usec = 2 * RTT;
    if (setsockopt(socket, SOL_SOCKET,SO_RCVTIMEO,&RTT_TO,sizeof(RTT_TO)) < 0) {
        fprintf(stderr, "Can not set timeout\n");
        return;
    }

    // load into the buffer
    addBuffer(BUFFER_SIZE);
    sendPkts(socket);

    while (!wait_send.empty() || !wait_ack.empty()) {
        // buffer or wait_ack not empty but cannot receive any acks, timeout
        if((numbytes = recvfrom(socket, pkt_buffer, sizeof(packet), 0, NULL, NULL)) == -1) {
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                return;
            }
            memcpy(pkt_buffer, &wait_ack.front(), sizeof(packet));
            if((numbytes = sendto(socket, pkt_buffer, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
                return;
            }
            stateUpdate(false, true);
        }
        else {
            packet pkt;
            memcpy(&pkt, pkt_buffer, sizeof(packet));
            if (pkt.msg_type == ACK) {
                if (pkt.ack_num > wait_ack.front().seq_num) {
                        while (!wait_ack.empty() && wait_ack.front().seq_num < pkt.ack_num) {
                            stateUpdate(true, false);
                            wait_ack.pop();
                        }
                        sendPkts(socket);
                    }
                else if (pkt.ack_num == wait_ack.front().seq_num) {
                    stateUpdate(false, false);
                    if (dupACK == 3) {
                        dupACK = 0;
                        ssthread = cwnd/2.0;
                        cwnd = ssthread + 3;
                        trans_state = FR;
                        memcpy(pkt_buffer, &wait_ack.front(), sizeof(packet));
                        if((numbytes = sendto(socket, pkt_buffer, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
                            perror("Error when fast resending");
                            exit(2);
                        }
                        
                    }
                }
            }
        }
    }
    fclose(fp);

    // send FIN to receiver
    packet fin_pkt;
    while (true) {
        // every time in loop, sending a fin msg
        fin_pkt.msg_type = FIN;
        fin_pkt.data_size = 0;
        memcpy(pkt_buffer, &fin_pkt, sizeof(packet));
        if((numbytes = sendto(socket, pkt_buffer, sizeof(packet), 0, p->ai_addr, p->ai_addrlen))== -1){
            perror("can not send FIN to sender");
            exit(2);
        }
        packet fin_ack;
        if ((numbytes = recvfrom(socket, pkt_buffer, sizeof(packet), 0, NULL, NULL)) == -1) {
            perror("can not receive from sender");
            exit(2);
        }
        memcpy(&fin_ack, pkt_buffer, sizeof(packet));
        // get 
        if (fin_ack.msg_type == FIN_ACK) {
            return;
        }
    }

}

int main(int argc, char** argv)
{

    if(argc != 5)
    {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    int udpPort = (unsigned short int)atoi(argv[2]);
    unsigned long long numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
    return 0;
}

