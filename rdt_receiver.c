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

    /* 
     * main loop: wait for a datagram, then echo it
     */
    VLOG(DEBUG, "epoch time, bytes received, sequence number");

    clientlen = sizeof(clientaddr);

    while (1) {
        //VLOG(DEBUG, "waiting from server \n");
        if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                (struct sockaddr *) &clientaddr, (socklen_t *)&clientlen) < 0) {
            error("ERROR in recvfrom");
        }
        recvpkt = (tcp_packet *) buffer;
        assert(get_data_size(recvpkt) <= DATA_SIZE);

        // If entire file has been received
        if ( recvpkt->hdr.data_size == 0) {
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
        /* 
         * sendto: ACK back to the client 
         */
        gettimeofday(&tp, NULL);
        VLOG(DEBUG, "%lu, %d, %d", tp.tv_sec, recvpkt->hdr.data_size, recvpkt->hdr.seqno);

        sndpkt = make_packet(0);
        printf("Seq received: %d | Looking for: %d.\n", recvpkt->hdr.seqno, lastrecvseqnum);

        // If not out of order, dont discard (sequence number isnt too large)
        if(recvpkt->hdr.seqno == lastrecvseqnum){
            fseek(fp, recvpkt->hdr.seqno, SEEK_SET);
            fwrite(recvpkt->data, 1, recvpkt->hdr.data_size, fp);
            lastrecvseqnum = recvpkt->hdr.seqno + recvpkt->hdr.data_size;
            
            // check buffer for any consecutive packets (entire array)
            // if one exists, write it and 
            
        }
        else{
            // attempt to buffer

            // no change to lastrecvseqnum
            printf("Out of order packet received.\n");
        }
        sndpkt->hdr.ackno = lastrecvseqnum;
        sndpkt->hdr.ctr_flags = ACK;
        if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, 
                (struct sockaddr *) &clientaddr, clientlen) < 0) {
            error("ERROR in sendto");
        }

        printf("\n\n\n");
    }

    return 0;
}
