CC = gcc

CFLAGS = -Wall -Wextra
INCLUDE_DIRS = -I.
LFLAGS = -lbsmp

OUT = fcs_client

all: $(OUT)

fcs_client: fcs_client.c fcs_client.h
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -o $@ $< $(LFLAGS)

clean:
	rm $(OUT)
