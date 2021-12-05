# Project2-Cole-Joseph
Repository for a TCP implementation via C Sockets, on top of a UDP transfer protocol.

### Running the Executables

- Use command 'make' to create executables. This will create a folder, tcp, where the executables are.
- Use command 'cd tcp' to enter directory with executables. Inside it will have the executable for the sender and a folder called Server, in which will be the executable for the receiver.
- To use client (sender), use command './sender [Host IP] [Port Num] [File Name/Directory]'.
- To use server (receiver), use command './Server/receiver [Port Num] [File Name]'.
- After finishing, call 'make clean' to remove 'tcp' folder.
