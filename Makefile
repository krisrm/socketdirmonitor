CFLAGS=-std=c99 -Wall -lreadline -g -w

all: dirapp test

dirapp: server.o client.o host.o common.c dirdiff.c dirapp.c
	gcc -o dirapp server.o client.o host.o dirapp.c common.c dirdiff.c $(CFLAGS)

test: server.o client.o host.o common.c dirdiff.c test.c
	gcc -o test server.o client.o host.o test.c common.c dirdiff.c $(CFLAGS)

server.o: server.c dirapp.h
	gcc -c server.c $(CFLAGS)

client.o: client.c dirapp.h
	gcc -c client.c $(CFLAGS)

host.o: host.c host.h
	gcc -c host.c $(CFLAGS)

tar: clean
	tar -czvf A2.tar.gz *.c *.h *.pl README.pdf README.txt Makefile 

clean:
	rm -f *.o dirapp *~ Core

edit:
	xfig-pdf-viewer 379_assignment2.pdf &
	gvim -p *.c *.h README.txt Makefile

