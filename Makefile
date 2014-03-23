CMDSEP =;

CC = gcc
MAKE = make

CFLAGS = -Wall -Wextra -Werror
INCLUDE_DIRS = -I.
LFLAGS = -L.
LDFLAGS = -lbsmp

USER=$(shell whoami)
INSTALL_DIR = /opt/fcs-client
METADATA_DIR=~/Desktop/metadata
EXEC_PATH=/usr/local/bin

ifeq ($(DEBUG),y)
	CFLAGS += -DDEBUG=1
endif

OUT = fcs_client
REVISION=$(shell git describe --dirty --always)

.SECONDEXPANSION:
fcs_client_OBJS = fcs_client.o debug.o revision.o transport/ethernet.o \
	transport/serial_rs232.o
aut_test_SCRIPTS = bpm_experiment metadata_parser
aut_test_USR_SCRIPTS = run_sweep run_single run_sweep_sausaging \
		   run_bursts

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
	ln -sf $(INSTALL_DIR)/fcs_client $(EXEC_PATH)
	$(foreach pyc, $(aut_test_SCRIPTS), \
		cp scripts/aut-tests/$(pyc).py $(INSTALL_DIR) $(CMDSEP))
	$(foreach pyc, $(aut_test_USR_SCRIPTS), \
		cp scripts/aut-tests/$(pyc).py $(INSTALL_DIR) $(CMDSEP))
	$(foreach pyc, $(aut_test_USR_SCRIPTS), \
		ln -sf $(INSTALL_DIR)/$(pyc).py \
		$(EXEC_PATH)/$(pyc) $(CMDSEP))
	mkdir -p $(METADATA_DIR)
	cp -r scripts/aut-tests/*.metadata $(METADATA_DIR)
	chown -R $(USER):$(USER) $(METADATA_DIR)
	chmod -R 444 $(METADATA_DIR)/*

uninstall:
	$(foreach pyc, $(aut_test_USR_SCRIPTS), \
		rm -f $(EXEC_PATH)/$(pyc) $(CMDSEP))
	$(foreach pyc, $(aut_test_USR_SCRIPTS), \
		rm -f $(INSTALL_DIR)/$(pyc).py $(CMDSEP))
	$(foreach pyc, $(aut_test_SCRIPTS), \
		rm -f $(INSTALL_DIR)/$(pyc).py $(CMDSEP))
	rm -f $(EXEC_PATH)/fcs_client
	rm -f $(INSTALL_DIR)/fcs_client
	rmdir $(INSTALL_DIR)

clean:
	$(foreach obj, $($(OUT)_OBJS),rm -f $(obj) $(CMDSEP))
	rm -f $(OUT)
