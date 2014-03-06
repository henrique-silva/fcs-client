CMDSEP =;

CC = gcc
MAKE = make

CFLAGS = -Wall -Wextra
INCLUDE_DIRS = -I.
LFLAGS = -L.
LDFLAGS = -lbsmp

INSTALL_DIR = /opt/fcs-client

ifeq ($(DEBUG),y)
	CFLAGS += -DDEBUG=1
endif

OUT = fcs_client
REVISION=$(shell git describe --dirty --always)

.SECONDEXPANSION:
fcs_client_OBJS = fcs_client.o debug.o revision.o

.PHONY: all clean

all: $(OUT)

$(OUT): $$($$@_OBJS)
	$(CC) $(CFLAGS) $(LFLAGS) $(INCLUDE_DIRS) -o $@ $^ $(LDFLAGS)

%.o : %.c %.h
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

revision.o: revision.c revision.h
	$(CC) $(CFLAGS) -DGIT_REVISION=\"$(REVISION)\" -c revision.c

install:
	mkdir -p $(INSTALL_DIR)
	cp fcs_client $(INSTALL_DIR)
	ln -s $(INSTALL_DIR)/fcs_client /usr/local/bin/

uninstall:
	rm -f $(INSTALL_DIR)/fcs-client
	rmdir $(INSTALL_DIR)
	rm /usr/local/bin/fcs-client

clean:
	$(foreach obj, $($(OUT)_OBJS),rm -f $(obj) $(CMDSEP))
	rm -f $(OUT)
