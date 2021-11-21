// Project 2: TCP Implementation (Sender/Client)
// Cole and Joseph
// Description: sender-side code for the TCP implementation via C Sockets (UDP).
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
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>

#include"packet.h"
#include"common.h"

#define STDIN_FD    0
#define RETRY  120 //milliseconds before timeout

// =============================================================================
// =============================================================================
// Global Variables

int next_seqno;
int send_base = 0;
int window_size = 10;
int effective_window = 10;
int timer_running = 0; //0 if timer is not running

int sockfd, serverlen;
struct sockaddr_in serveraddr;
struct itimerval timer; 
tcp_packet *sndpkt; 
tcp_packet *lastackpkt;
tcp_packet *recvpkt;
sigset_t sigmask;       
FILE *fp;
int acklen;

// =============================================================================
// =============================================================================
// Functions

// Resend Packets (on timeout)
void resend_packets(int sig)
{
    if (sig == SIGALRM)
    {
        //Resend all packets range between 
        //sendBase and up to but not including nextSeqNum
        VLOG(INFO, "Timout happend: Resending from %d to %d", send_base, next_seqno);
        int resent_seq = send_base;
        fseek( fp, send_base, SEEK_SET); //Rewind fp to sendBase
        int len = 0;
        char buffer[DATA_SIZE];
        while(resent_seq < next_seqno){
            
            //Read in data
            len = fread(buffer, 1, DATA_SIZE, fp);
            //Check to make sure its not EOF
            if ( len <= 0)
            {
                VLOG(INFO, "End Of File has been reached");
                sndpkt = make_packet(0);
                sendto(sockfd, sndpkt, TCP_HDR_SIZE,  0,
                        (const struct sockaddr *)&serveraddr, serverlen);
                break;
            }
            sndpkt = make_packet(len);
            memcpy(sndpkt->data, buffer, len);
            sndpkt->hdr.seqno = resent_seq;

            //Send packet

            VLOG(DEBUG, "Resending packet %d to %s", 
                    resent_seq, inet_ntoa(serveraddr.sin_addr));
            resent_seq = resent_seq + len;
            /*
             * If the sendto is called for the first time, the system will
             * will assign a random port number so that server can send its
             * response to the src port.
             */
            if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                        ( const struct sockaddr *)&serveraddr, serverlen) < 0)
            {
                error("sendto");
            }
        }
    }
}

// Start Timer
void start_timer()
{
    sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
    setitimer(ITIMER_REAL, &timer, NULL);
}

// Stop Timer
void stop_timer()
{
    sigprocmask(SIG_BLOCK, &sigmask, NULL);
}

/*
 * init_timer: Initialize timeer
 * delay: delay in milli seconds
 * sig_handler: signal handler function for resending unacknoledge packets
 */
void init_timer(int delay, void (*sig_handler)(int)) 
{
    signal(SIGALRM, resend_packets);
    timer.it_interval.tv_sec = delay / 1000;    // sets an interval of the timer
    timer.it_interval.tv_usec = (delay % 1000) * 1000;  
    timer.it_value.tv_sec = delay / 1000;       // sets an initial value
    timer.it_value.tv_usec = (delay % 1000) * 1000;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGALRM);
}

// =============================================================================
// =============================================================================
// Execution
int main (int argc, char **argv)
{
    int portno, len;
    char *hostname;
    char buffer[DATA_SIZE];

    /* check command line arguments */
    if (argc != 4) {
        fprintf(stderr,"usage: %s <hostname> <port> <FILE>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    fp = fopen(argv[3], "r");
    if (fp == NULL) {
        error(argv[3]);
    }

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* initialize server server details */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serverlen = sizeof(serveraddr);

    /* covert host into network byte order */
    if (inet_aton(hostname, &serveraddr.sin_addr) == 0) {
        fprintf(stderr,"ERROR, invalid host %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(portno);

    assert(MSS_SIZE - TCP_HDR_SIZE > 0);

    // Go Back N Implementation
    init_timer(RETRY, resend_packets);
    next_seqno = 0;
    while (1)
    {
        // Send as many packets in effective window as doable
        while(effective_window > 0){
            //Read in data
            len = fread(buffer, 1, DATA_SIZE, fp);
            //Check to make sure its not EOF
            if ( len <= 0)
            {
                VLOG(INFO, "End Of File has been reached");
                sndpkt = make_packet(0);
                sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, (const struct sockaddr *)&serveraddr, serverlen);
                break;
            }
            sndpkt = make_packet(len);
            memcpy(sndpkt->data, buffer, len);
            sndpkt->hdr.seqno = next_seqno;

            // Send packet

            VLOG(DEBUG, "Sending packet %d to %s", 
                    next_seqno, inet_ntoa(serveraddr.sin_addr));
            next_seqno = next_seqno + len;
            effective_window--;
            /*
             * If the sendto is called for the first time, the system will
             * will assign a random port number so that server can send its
             * response to the src port.
             */
            if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                        ( const struct sockaddr *)&serveraddr, serverlen) < 0)
            {
                error("sendto");
            }

            // If first packet off, start timer
            if(timer_running == 0){
                start_timer();
                timer_running = 1;
                acklen = len;
            }
        }

        
        // Wait for ACK
        // do {
        //Discard any duplicate acks (below the next expected ack)
        do{
            if(recvfrom(sockfd, buffer, MSS_SIZE, 0,
                (struct sockaddr *) &serveraddr, (socklen_t *)&serverlen) < 0)
            {
                error("recvfrom");
            }
            
            recvpkt = (tcp_packet *)buffer;

            // If file has been fully received, stop execution
            if(recvpkt->hdr.ctr_flags == FIN){
                printf("File has been transferred. Exiting...\n");
                free(sndpkt);
                exit(EXIT_SUCCESS);
            }

            printf("%d \n", get_data_size(recvpkt));
            printf("Packet ackno: %d \n", recvpkt->hdr.ackno);
            printf("Send base, len: %d %d\n", send_base, acklen);
            assert(get_data_size(recvpkt) <= DATA_SIZE);
        } while(recvpkt->hdr.ackno < send_base+acklen);

        //The expected ack has been recieved
        effective_window++;
        send_base = send_base+acklen;
        stop_timer();
        timer_running = 0;
            /*resend pack if dont recv ack */
        // } while(recvpkt->hdr.ackno != send_base+acklen);

        free(sndpkt);
    }

    return 0;

}



