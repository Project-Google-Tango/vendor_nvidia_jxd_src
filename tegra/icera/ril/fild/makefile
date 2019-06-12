#################################################################################################
#
# Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
#
#
# Usage:
# make PLAT=linux    -> builds fild tool for linux
#
#
#########################################################

#################################################################################################
# name of the generated static libraries and shared objects
#################################################################################################
NAME = fild

#################################################################################################
# list of include paths
#################################################################################################

OBJ = $(NAME).o \
      fild_boot.o \
      fild_fs.o \
      fild_port.o \
      obex_core.o \
      obex_file.o \
      obex_port.o \
      obex_server.o \
      obex_transport.o

CFLAGS += -O2 -Wall
IFLAGS += -I../icera-util

ifeq ($(FILD_DIRECT_IPC),yes)
CFLAGS += -DFILD_DIRECT_IPC
endif

# Linux (by default)
CC = gcc -std=gnu99
CFLAGS += -g $(IFLAGS)
BIN = fild
LFLAGS += -lpthread

ifneq ($(FILD_DIRECT_IPC),yes)
# we don't use timers when testing boot from SHM in software model
LFLAGS += -lrt
endif

bin:$(BIN)

$(BIN): $(OBJ)
	@echo Build target $@
	rm -f $@
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LFLAGS)

clean:
	@echo Clean target
	rm -f *.o $(BIN)

