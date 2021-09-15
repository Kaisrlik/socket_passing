.PHONY: all clean

TARGETS := srv cli nfq
CC := gcc
CFLAGS := -I/usr/include/libnl3/
LDFLAGS := -Wl,-lnl-3 -Wl,-lnl-nf-3

all: $(TARGETS)

nfq: nf-queue.c
	$(CC) -o $@ $< $(CFLAGS) -lmnl -lnetfilter_queue

%: %.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGETS)
