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

// =============================================================================
// =============================================================================
// Global Variables

// Sending Variables
int window_size = 1;        // cwnd size
int packets_in_flight = 0;  // # of packets in flight
int effective_window = 1;   // number of packets available to send (cwnd - # of packets in flight)

int next_seqno;             // seq # of next packet to be sent
int send_base = 0;          // seq # of next packet to be ACKed (window start)
int window_end = 0;         // seq # of last packet in window (window end)
 
int ssthresh = -1;       // -1 is the initial value that means infinity
int slow_start = 1;         // 1 if slow start, 0 if congestion avoidance (additive increase)
int acc_acks = 0;           // accumulated ACKs

// Timer variables
int timer_running = 0; //0 if timer is not running
int timedPacket = 0; //The ack the timer should be looking for to stop

int timeout = 120; //Timeout value in milliseconds for the timer
int retransmit = 0; //Flag to set if the timer is running on a retranmitted packet
float devRTT = 0;
int estimatedRTT = 100; //Estimated round trip time

int dupe_acks = 0;          // # of duplicate ACKs received

int acklen;                 // length of data for next packet to be ACKed packet (send base)

//  Server Variables
int sockfd, serverlen;
struct sockaddr_in serveraddr;
struct itimerval timer; 

// Packet & File Variables
tcp_packet *sndpkt; 
tcp_packet *timeoutpkt;
tcp_packet *recvpkt;
sigset_t sigmask;       
FILE *fp;

int done = 0; // Flag if done 

// =============================================================================
// =============================================================================
// Functions

// Get Maximum
int max(int a, int b){
    if(a > b){
        return a;
    }
    else{
        return b;
    }
}

// Resend Packets (on timeout)
void resend_packets(int sig)
{
    retransmit = 1;
    if (sig == SIGALRM)
    {
        // Set ssthresh and reset cwnd
        ssthresh = max(window_size/2,2);
        // packets_in_flight = 0;
        slow_start = 1;
        window_size = 1;

        // Resend packet
        VLOG(INFO, "Timeout happend: Resending %d", send_base);
        fseek(fp, send_base, SEEK_SET); // rewind fp to send base
        int len = 0;
        char buffer[DATA_SIZE];

        // Clear Buffer
        // bzero(&buffer, sizeof(buffer));

        len = fread(buffer, 1, DATA_SIZE, fp);

        // if EOF, send an empty packet to notify receiver of EOF
        if (len <= 0){
            VLOG(INFO, "End Of File has been reached");
            timeoutpkt = make_packet(0);
            timeoutpkt->hdr.ctr_flags = FIN;
            timeoutpkt->hdr.ackno = next_seqno;
            sendto(sockfd, timeoutpkt, TCP_HDR_SIZE, 0, (const struct sockaddr *)&serveraddr, serverlen);
        }
        // otherwise, create a packet with the data read
        else{
            timeoutpkt = make_packet(len);
            memcpy(timeoutpkt->data, buffer, len);
            timeoutpkt->hdr.seqno = send_base;
            printf("Base packet recreated.\n");
            // send packet
            VLOG(DEBUG, "Sending packet %d to %s", send_base, inet_ntoa(serveraddr.sin_addr));
            if(sendto(sockfd, timeoutpkt, TCP_HDR_SIZE + get_data_size(timeoutpkt), 0, (const struct sockaddr *)&serveraddr, serverlen) < 0){
                error("sendto");
            }
        }
        // set fp to back to next seq # to be read, decrement effective window
        fseek(fp, next_seqno, SEEK_SET);

        // free memory
        free(timeoutpkt);
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

// Recalcaulate the variance and timeout using the passed RTT
void recalcTimeout(int sampleRTT){
    printf("Recalculating time\n");
    estimatedRTT = (int)(((1.0-0.125) * estimatedRTT) + (0.125 * sampleRTT));
    devRTT = (1.0-0.25) * devRTT + 0.25 * abs(sampleRTT - estimatedRTT);
    timeout = (int) (estimatedRTT + 4 * devRTT);

    //Set the timer to have the new timeout
    timer.it_interval.tv_sec = timeout / 1000;    // sets an interval of the timer
    timer.it_interval.tv_usec = (timeout % 1000) * 1000;  
    timer.it_value.tv_sec = timeout / 1000;       // sets an initial value
    timer.it_value.tv_usec = (timeout % 1000) * 1000;
}

// Timer Initialization
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

    // debug (coinflip)
    srand(time(0));

    // Open File (Graphing)
    FILE *fpt;
    fpt = fopen("CWND.csv", "w+");
    fprintf(fpt,"Time, CWND, SlowStart?, ssthresh, send_base, packets_flying\n");
    struct timeval tp;

    // Main Execution
    init_timer(timeout, resend_packets);
    next_seqno = 0;
    while (1)
    {
        // Logging
        gettimeofday(&tp, NULL);
        fprintf(fpt,"%lu, %d\n", tp.tv_sec, window_size);

        // Clear Buffer
        // bzero(&buffer, sizeof(buffer));

        // Fast Retransmit (if applicable)
        if(dupe_acks >= 3){
            retransmit = 1;  // mark as duplicate for timer
            // reset dupe ACK counter
            dupe_acks = 0;
            ssthresh = max(window_size/2,2);
            slow_start = 1;
            window_size = 1;

            // Read Data & Create Packet
            fseek(fp, send_base, SEEK_SET); // rewind fp to send base
            len = fread(buffer, 1, DATA_SIZE, fp);

            // if EOF, send an empty packet to notify receiver of EOF
            if ( len <= 0){
                VLOG(INFO, "End Of File has been reached.");
                sndpkt = make_packet(0);
                sndpkt->hdr.ctr_flags = FIN;
                sndpkt->hdr.ackno = next_seqno;
                sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, (const struct sockaddr *)&serveraddr, serverlen);
            }
            // otherwise, make a packet and fill it
            else{
                sndpkt = make_packet(len);
                memcpy(sndpkt->data, buffer, len);
                sndpkt->hdr.seqno = send_base;
                // send packet
                if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, (const struct sockaddr *)&serveraddr, serverlen) < 0){
                    error("sendto");
                }
                // set fp to back to next seq # to be read, decrement effective window
                fseek(fp, next_seqno, SEEK_SET); 
            }
            printf("Fast retransmitting packet with seq %d.\n", sndpkt->hdr.seqno);

            // free memory
            free(sndpkt);
        }

        // Update Window End
        effective_window = (window_size - packets_in_flight);
        window_end = send_base + (DATA_SIZE * window_size);

        // Send as many packets in the control window as possible (prevent from going outside of it)
        printf("Window: %d, Packets in flight:%d\n", window_size, packets_in_flight);
        while( ( (window_size - packets_in_flight) > 0 && (next_seqno <= window_end) ) || timer_running == 0){
            
            // Read Data & Create Packet
            len = fread(buffer, 1, DATA_SIZE, fp);
            // if EOF, send an empty packet to notify receiver of EOF
            if ( len <= 0){
                VLOG(INFO, "End Of File has been reached.");
                sndpkt = make_packet(0);
                sndpkt->hdr.ctr_flags = FIN;
                sndpkt->hdr.ackno = next_seqno;
                sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, (const struct sockaddr *)&serveraddr, serverlen);
                packets_in_flight++;
                break;
            }
            // otherwise, create a packet with the data read
            sndpkt = make_packet(len);
            memcpy(sndpkt->data, buffer, len);
            sndpkt->hdr.seqno = next_seqno;

            // Send Packet
            // if((rand()%4 == 0 || rand()%4 == 1) && window_size > 3){}
            // else{
            //     VLOG(DEBUG, "Sending packet %d to %s", next_seqno, inet_ntoa(serveraddr.sin_addr));
            //     if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, (const struct sockaddr *)&serveraddr, serverlen) < 0){
            //         error("sendto");
            //     }
            // }
            VLOG(DEBUG, "Sending packet %d to %s", next_seqno, inet_ntoa(serveraddr.sin_addr));
            if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, (const struct sockaddr *)&serveraddr, serverlen) < 0){
                error("sendto");
            }
            
            // if first packet is sent, start timer
            if(timer_running == 0){
                start_timer();
                timer_running = 1;
                // acklen = len;
                retransmit = 0;
                timedPacket = next_seqno;
            }

            // increment next seq # to be sent increase packets in flight
            next_seqno = next_seqno + len;
            packets_in_flight++;
            
            // free memory
            free(sndpkt); 
        }

        // Receive ACK
        bzero(&buffer, sizeof(buffer));
        if(recvfrom(sockfd, buffer, MSS_SIZE, 0,(struct sockaddr *) &serveraddr, (socklen_t *)&serverlen) < 0){
            error("recvfrom");
        }
        // create packet
        recvpkt = (tcp_packet *)buffer;
        // display connection status
        printf("\e[1;1H\e[2J\n");
        if(slow_start){printf("-= Slow Start =-\n");}
        else{printf("-= Congestion Avoidance =-\n");}
        printf("Control Window: %d | Packets in Flight: %d | ssthresh: %d\n", window_size, packets_in_flight, ssthresh);
        printf("Packet ackno: %d \n", recvpkt->hdr.ackno);
        printf("Send base, len: %d %d | Next seq: %d\n", send_base, len, next_seqno);
        assert(get_data_size(recvpkt) <= DATA_SIZE);

        // if FIN (file fully received), stop execution
        if(recvpkt->hdr.ctr_flags == FIN){
            printf("File has been transferred! Exiting...\n");
            close(sockfd);
            exit(EXIT_SUCCESS);
        }
        // if ACK, check ACK number
        else if(recvpkt->hdr.ctr_flags == ACK){
            printf("Received ACK asking for: %d.\n", recvpkt->hdr.ackno);
            // if successful ACK (non-duplicate) arrives
            if(recvpkt->hdr.ackno > send_base){
                 // reset dupe ACK counter
                dupe_acks = 0;
                // printf("End timer sequence\n");
                int packets_acked = 1;
                // if acknum is greater than base + additional packet (2 or more packets ACKed)
                if(recvpkt->hdr.ackno > send_base + DATA_SIZE){
                    // get difference between ACK number and send base, divide and round up
                    int total_diff = recvpkt->hdr.ackno - (send_base);
                    int total_remainder = (total_diff)%(DATA_SIZE);
                    packets_acked = (total_diff)/(DATA_SIZE);
                    if(total_remainder > 0){
                        packets_acked++;
                    }
                    printf("Total Diff: %d | Total packets ACKed: %d | Total Accumulated ACKs: %d \n", total_diff, packets_acked, acc_acks);
                }
                printf("Total packets ACKed: %d | Total Accumulated ACKs: %d \n", packets_acked, acc_acks);
                
                // increment ACKs and decrement packets in flight
                acc_acks += packets_acked;
                packets_in_flight -= packets_acked;

                // if slow start, increase the window to send additional packet
                if(slow_start){
                    window_size++;
                    printf("Incrementing window size. Cwnd: %d\n", window_size);
                    // if ssthresh is reached, switch to congestion avoidance
                    if(window_size == ssthresh){
                        slow_start = 0;
                    }
                    send_base = recvpkt->hdr.ackno;
                }
                // if congestion avoidance, increment the accumulated ACK based on the size of data
                else{
                    send_base = recvpkt->hdr.ackno;
                    // if the entire cwnd size has been ACKed, increase window size by 1
                    if(acc_acks >= window_size){
                        acc_acks = 0;
                        window_size++;
                        // printf("Incrementing window size. Cwnd: %d\n", window_size);
                    }
                }
                // stop timer (restarts after iteration ends), check to see if rtt needs to be recalculated
                if(retransmit == 0 && timedPacket <= send_base){
                    struct itimerval timerVal;
                    getitimer(ITIMER_REAL, &timerVal);
                    int timerMilliseconds = timerVal.it_value.tv_sec*1000 + (timerVal.it_value.tv_usec/1000);
                    int fullTimer = timer.it_value.tv_sec*1000 + (timer.it_value.tv_usec/1000);
                    // printf("Recorded timer:%d fulltimer:%d\n", timerMilliseconds, fullTimer);
                    // recalcTimeout(fullTimer - timerMilliseconds);
                    timer_running = 0;
                    stop_timer();
                    // printf("timer stopped\n");
                }
                // Stop timer as a retransmitted packet has been recived
                else if(timedPacket <= send_base){
                    timer_running = 0;
                    stop_timer();
                    // printf("timer stopped\n");
                }
            }
            // if duplicate ACK, increment the dupe ACK count
            else if(recvpkt->hdr.ackno == send_base){
                dupe_acks++;
                printf("Duplicate ACKs: %d.\n", dupe_acks);
                // as dupe ACKs only are a possible symptom of loss, don't change out from slow start
            }   
        }
    }
    fclose(fpt);
    close(sockfd);
    return 0;

}



