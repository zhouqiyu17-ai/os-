UNAME_S  := $(shell uname -s)
CC       = gcc
CFLAGS   = -Wall -Wextra -Iinc -g -O2
LDFLAGS  = -lpthread -lm
ifeq ($(UNAME_S),Linux)
LDFLAGS  += -lrt
endif

SRCDIR   = src
TESTDIR  = test
OBJDIR   = obj
BINDIR   = bin

SRCS     = $(wildcard $(SRCDIR)/*.c)
OBJS     = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

.PHONY: all clean test bench

all: $(BINDIR) $(BINDIR)/ipc_test $(BINDIR)/bench

$(BINDIR):
	mkdir -p $(BINDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/ipc_test: $(OBJS) $(TESTDIR)/ipc_test.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(TESTDIR)/ipc_test.c $(OBJS) $(LDFLAGS)

$(BINDIR)/bench: $(OBJS) $(TESTDIR)/bench.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(TESTDIR)/bench.c $(OBJS) $(LDFLAGS)

clean:
	rm -rf $(OBJDIR) $(BINDIR)
	rm -f /tmp/ipc_fifo_*
	rm -f /tmp/ipc_uds.sock
