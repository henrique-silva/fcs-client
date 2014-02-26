CMDSEP =;

CC = gcc
MAKE = make

CFLAGS = -Wall -Wextra
INCLUDE_DIRS = -I. -Ignuplot/src
LFLAGS = -lbsmp

ifeq ($(DEBUG),y)
	CFLAGS += -DDEBUG=1
endif

OUT = fcs_client

.SECONDEXPANSION:
fcs_client_OBJS = fcs_client.o debug.o

.PHONY: all clean

all: $(OUT)

$(OUT): $$($$@_OBJS)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -o $@ $^ $(LFLAGS)

%.o : %.c %.h
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

clean:
	$(foreach obj, $($(OUT)_OBJS),rm -f $(obj) $(CMDSEP))
	rm -f $(OUT)
