// Project 2: TCP Implementation (Receiver/Server)
// Cole and Joseph
// Description: receiver-side code for the TCP implementation via C Sockets (UDP).
//              Modified starter code from class.
// =============================================================================
// =============================================================================
// Includes and Definitions
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <assert.h>

#include "common.h"
#include "packet.h"
// =============================================================================
// =============================================================================
// Global Variables

tcp_packet *recvpkt;
tcp_packet *sndpkt;

// =============================================================================
// =============================================================================
// Execution
int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int optval; /* flag value for setsockopt */
    FILE *fp;
    char buffer[MSS_SIZE];
    struct timeval tp;
    int lastrecvseqnum = 0;

    // Out of order buffer
    int buffer_size = 10;
    int out_of_order_num[buffer_size];
    int out_of_order_size[buffer_size];
    char out_of_order_data[buffer_size][DATA_SIZE];
    memset(out_of_order_num, 0, buffer_size*sizeof(out_of_order_num[0]));
    memset(out_of_order_size, 0, buffer_size*sizeof(out_of_order_size[0]));

    int head = 0; //Where is the next out of order packet
    int tail = 0; //Where to put the next out of order packet
    int lastBuffered = 0; //Used to make sure packets are not buffered out of order in buffer

    // Check command line arguments
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> FILE_RECVD\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    fp  = fopen(argv[2], "w");
    if (fp == NULL) {
        error(argv[2]);
    }

    // socket: create the parent socket 
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
            (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
     * bind: associate the parent socket with a port 
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr, 
                sizeof(serveraddr)) < 0) 
        error("ERROR on binding");

    VLOG(DEBUG, "epoch time, bytes received, sequence number");

    clientlen = sizeof(clientaddr);

    // Main Execution
    while (1) {
        // VLOG(DEBUG, "waiting from server \n");

        // Clear Buffer
        bzero(&buffer, sizeof(buffer));

        // Receive Packet
        if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                (struct sockaddr *) &clientaddr, (socklen_t *)&clientlen) < 0) {
            error("ERROR in recvfrom");
        }
        recvpkt = (tcp_packet *) buffer;
        assert(get_data_size(recvpkt) <= DATA_SIZE);

        gettimeofday(&tp, NULL);
        VLOG(DEBUG, "%lu, %d, %d", tp.tv_sec, recvpkt->hdr.data_size, recvpkt->hdr.seqno);
        
        // Create ACK packet
        sndpkt = make_packet(0);
        printf("\e[1;1H\e[2J\n");
        if(recvpkt->hdr.ctr_flags == DATA){
            printf("Seq received: %d | Looking for: %d.\n", recvpkt->hdr.seqno, lastrecvseqnum);
        }
        else if(recvpkt->hdr.ctr_flags == FIN){
            printf("FIN packet received. End seq no: %d | Looking for: %d.\n", recvpkt->hdr.ackno, lastrecvseqnum);
        }
        printf("Seq No. of buffer: ");
        for(int i = 0; i < buffer_size; i++){
            if(i > 0){
                printf(", %d", out_of_order_num[i]);
            }
            else{
                printf("%d", out_of_order_num[i]);
            }
        }
        printf("\n");

        // If DATA Packet
        if(recvpkt->hdr.ctr_flags == DATA){
            // If not out of order, dont discard (sequence number isnt too large)
            if(recvpkt->hdr.seqno == lastrecvseqnum){
                fseek(fp, lastrecvseqnum, SEEK_SET);
                fwrite(recvpkt->data, 1, recvpkt->hdr.data_size, fp);
                lastrecvseqnum = recvpkt->hdr.seqno + recvpkt->hdr.data_size;

                // If packets can be written to file from buffer do so now
                if(tail != head){
                    printf("Buffer has %d at %d %d\n", out_of_order_num[head], head, tail);
                }
                while(tail != head && out_of_order_num[head] == lastrecvseqnum){
                    printf("Using buffer. Writing %d of size %d, with content:\n", out_of_order_num[head], out_of_order_size[head]);
                    printf("%s\n\n", out_of_order_data[head]);
                    fseek(fp, out_of_order_num[head], SEEK_SET);
                    fwrite(out_of_order_data[head], 1, out_of_order_size[head], fp);
                    lastrecvseqnum = out_of_order_num[head] + out_of_order_size[head];
                    head = (head + 1) % buffer_size;
                    printf("New Buffer: %d at %d %d\n", out_of_order_num[head], head, tail);
                }
            }
            // if out of order, attempt to buffer
            else if (recvpkt->hdr.seqno > lastrecvseqnum){
                // See if there is space
                if((tail + 1) % buffer_size != head){
                    // Make sure the packet being written to buffer is not smaller than the last packet in buffer
                    if(tail == head || recvpkt->hdr.seqno >= lastBuffered){
                        // Dont buffer ones with no size
                        // if(recvpkt->hdr.data_size > 0){
                            printf("Writing %d to buffer\n", recvpkt->hdr.seqno);
                            // printf("Current:%s\n\n", recvpkt->data);
                            out_of_order_num[tail] = recvpkt->hdr.seqno;
                            out_of_order_size[tail] = recvpkt->hdr.data_size;
                            memcpy(out_of_order_data[tail], recvpkt->data, recvpkt->hdr.data_size);
                            printf("%s\n\n\n", recvpkt->data);
                            printf("%s\n", out_of_order_data[tail]);
                            // printf("Saved:%s\n\n", out_of_order_data[tail]);
                            tail = (tail + 1) % buffer_size;
                            lastBuffered = recvpkt->hdr.seqno;
                        // }
                    }
                }
                // no change to lastrecvseqnum
                printf("Out of order packet received.\n");
            }
        }
        // If FIN Packet
        else if (recvpkt->hdr.ctr_flags == FIN) {
            if(recvpkt->hdr.ackno == lastrecvseqnum){
                //VLOG(INFO, "End Of File has been reached");
                fclose(fp);
                sndpkt = make_packet(0);

                // Mark packet as finish acknowledgement
                sndpkt->hdr.ctr_flags = FIN;
                if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, (struct sockaddr *) &clientaddr, clientlen) < 0) {
                    error("ERROR in sendto");
                }
                printf("File has been received! Exiting...\n");
                close(sockfd);
                break;
            }
        }
        // Set ACK and Send
        sndpkt->hdr.ackno = lastrecvseqnum;
        sndpkt->hdr.ctr_flags = ACK;
        if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, 
                (struct sockaddr *) &clientaddr, clientlen) < 0) {
            error("ERROR in sendto");
        }
        // free memory
        free(sndpkt);
    }

    close(sockfd);
    return 0;
}
