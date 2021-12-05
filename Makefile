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

SRVRDIR = ./tcp/Server

CLIENT_OBJECTS := $(OBJDIR)/rdt_sender.o $(OBJDIR)/common.o $(OBJDIR)/packet.o
SERVER_OBJECTS := $(OBJDIR)/rdt_receiver.o $(OBJDIR)/common.o $(OBJDIR)/packet.o

#Program name
CLIENT := $(OBJDIR)/rdt_sender
SERVER := $(OBJDIR)/rdt_receiver

rm       = rm -f
rmdir    = rmdir 

TARGET:	$(OBJDIR) $(SRVRDIR) $(CLIENT) $(SERVER)


$(CLIENT):	$(CLIENT_OBJECTS)
	$(LINKER)  $@  $(CLIENT_OBJECTS)
	@echo "Link complete!"

$(SERVER): $(SERVER_OBJECTS)
	$(LINKER)  $@  $(SERVER_OBJECTS) && mv $(SERVER) $(SRVRDIR)
	@echo "Link complete!"

$(OBJDIR)/%.o:	%.c common.h packet.h
	$(CC) $(CFLAGS)  $< -o $@
	@echo "Compilation complete!"

clean:
	@if [ -a $(OBJDIR) ]; then rm -r $(OBJDIR); fi;
	@echo "Cleanup complete!"

$(SRVRDIR):
	@[ -a $(SRVRDIR) ]  || mkdir $(SRVRDIR)

$(OBJDIR):
	@[ -a $(OBJDIR) ]  || mkdir $(OBJDIR)