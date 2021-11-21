#Reference
#http://makepp.sourceforge.net/1.19/makepp_tutorial.html

CC = gcc -c
SHELL = /bin/bash

# compiling flags here
CFLAGS = -Wall -I.

LINKER = gcc -o
# linking flags here
LFLAGS   = -Wall

OBJDIR = ./tcp

CLIENT_OBJECTS := $(OBJDIR)/rdt_sender.o $(OBJDIR)/common.o $(OBJDIR)/packet.o
SERVER_OBJECTS := $(OBJDIR)/rdt_receiver.o $(OBJDIR)/common.o $(OBJDIR)/packet.o

#Program name
CLIENT := $(OBJDIR)/sender
SERVER := $(OBJDIR)/receiver

rm       = rm -f
rmdir    = rmdir 

TARGET:	$(OBJDIR) $(CLIENT)	$(SERVER)


$(CLIENT):	$(CLIENT_OBJECTS)
	$(LINKER)  $@  $(CLIENT_OBJECTS)
	@echo "Link complete!"

$(SERVER): $(SERVER_OBJECTS)
	$(LINKER)  $@  $(SERVER_OBJECTS) && mkdir $(OBJDIR)/Server && mv $(SERVER) $(OBJDIR)/Server
	@echo "Link complete!"

$(OBJDIR)/%.o:	%.c common.h packet.h
	$(CC) $(CFLAGS)  $< -o $@
	@echo "Compilation complete!"

clean:
	@if [ -a $(OBJDIR) ]; then rm -r $(OBJDIR); fi;
	@echo "Cleanup complete!"

$(OBJDIR):
	@[ -a $(OBJDIR) ]  || mkdir $(OBJDIR)


# CFLAGS = -O
# CC = gcc

# all: common.o packet.o rdt_receiver rdt_sender 

# # creates receiver and automatically places in folder Server
# rdt_receiver: rdt_receiver.o
# 	$(CC) $(CFLAGS) -o rdt_receiver rdt_receiver.o common.o packet.o && mv rdt_receiver Server/

# rdt_sender: rdt_sender.o
# 	$(CC) $(CFLAGS) -o rdt_sender rdt_sender.o common.o packet.o 

# common.o:
# 	$(CC) $(CFLAGS) -c common.c common.h

# packet.o:
# 	$(CC) $(CFLAGS) -c packet.c packet.h

# rdt_receiver.o:
# 	$(CC) $(CFLAGS) -c rdt_receiver.c

# rdt_sender.o:
# 	$(CC) $(CFLAGS) -c rdt_sender.c


 
# clean:
# 	rm -f *.o *.gch *~ rdt_sender rdt_receiver