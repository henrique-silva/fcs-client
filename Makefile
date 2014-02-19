CMDSEP =;

CC = gcc
MAKE = make

CFLAGS = -Wall -Wextra
INCLUDE_DIRS = -I. -Ignuplot/src
LFLAGS = -lbsmp

GNUPLOT_DIR = gnuplot
# weird,  I know... just to keep things simple
GNUPLOT_INCLUDE_DIR = $(GNUPLOT_DIR)/src

OUT = fcs_client

.SECONDEXPANSION:
fcs_client_OBJS = fcs_client.o debug.o gnuplot_i.o

.PHONY: all clean

all: gnuplot $(OUT)

$(OUT): $$($$@_OBJS)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -o $@ $^ $(LFLAGS)

gnuplot_i.o:
	$(MAKE) -C $(GNUPLOT_DIR)
	cp $(GNUPLOT_DIR)/gnuplot_i.o .
	ln -s $(GNUPLOT_INCLUDE_DIR)/gnuplot_i.h gnuplot_i.h

%.o : %.c %.h
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

clean:
	$(foreach obj, $($(OUT)_OBJS),rm -f $(obj) $(CMDSEP))
	$(MAKE) -C $(GNUPLOT_DIR) clean
	rm -f gnuplot_i.h
	rm -f $(OUT)
