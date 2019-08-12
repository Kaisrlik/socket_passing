all: srv cli nf

srv: srv.c
	gcc -o srv srv.c -I/usr/include/libnl3/ -lnl-3 -lnl-nf-3
cli: cli.c
	gcc -o cli cli.c -I/usr/include/libnl3/ -lnl-3 -lnl-nf-3

nf:
	gcc -o nfq nf-queue.c -lmnl -lnetfilter_queue
