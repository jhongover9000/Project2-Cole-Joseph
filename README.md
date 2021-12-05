# Project2-Cole-Joseph
Repository for a TCP implementation via C Sockets, on top of a UDP transfer protocol.

### Running the Executables

- Use command 'make' to create executables. This will create a folder, tcp, where the executables are.
- Use command 'cd tcp' to enter directory with executables. Inside it will have the executable for the sender and a folder called Server, in which will be the executable for the receiver.
- To use client (sender), use command './sender [Host IP] [Port Num] [File Name/Directory]'.
- To use server (receiver), use command './Server/receiver [Port Num] [File Name]'.
- After finishing, call 'make clean' to remove 'tcp' folder.

#### The Sender

The sender for our TCP implementation sends packets within the congestion window whenever there is space available, and this window slides with ACKs that are received. When a timeout occurs, it sends the send_base packet. When 3 dupe ACKs are received, a fast retransmit occurs. In both cases, the ssthreshold is either halved or is set to 2 (if less than 2). At the end of the file, it will send a FIN packet with the next seqno (the ending seqno) in the ackno section of the header, which can be used to compare if the file has been fully received.

#### The Receiver

The receiver accepts incoming packets and checks if they are either DATA or FIN. If DATA, it will check the seqno and either write it into the file or store it in a buffer (or drop it if there is no space available). If FIN, it will check to make sure that it has the final seqno of the sender's file, comparing it with whatever it has last received, then either sends the dupe ACK (if not matching) or exits successfully (if matching).

#### Testing

We've been sending all sorts of files under all sorts of conditions using Mahi Mahi. It works most of the time, but there are some rare occasions where there are corruptions that occur midway. We aren't sure where the issues are coming from, but it has to do with partial packets being sent or the receiver's buffer system not working properly.