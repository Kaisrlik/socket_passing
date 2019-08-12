all: srv cli

srv:
	gcc -o srv srv.c
cli:
	gcc -o cli cli.c
