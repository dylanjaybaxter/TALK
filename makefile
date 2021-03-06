CC = gcc
CFLAGS = -Wall -g
LD = gcc
LDFLAGS = -g

all: mytalk mytalk.o

mytalk: mytalk.o
	$(CC) $(CFLAGS) -L ~pn-cs357/Given/Talk/lib64 -o mytalk mytalk.o -ltalk \
	-l ncurses

mytalk.o: mytalk.c
	$(CC) $(CFLAGS) -I ~pn-cs357/Given/Talk/include -c -o mytalk.o mytalk.c

comp:
	$(CC) $(CFLAGS) -c -o mytalk.o mytalk.c
	$(CC) $(CFLAGS) -o mytalk mytalk.o
clean: mytalk
	rm mytalk.o
server: mytalk
	./mytalk hoster
client: mytalk
	./mytalk hoster 1234
valgrind: timit
	valgrind -s --leak-check=full --track-origins=yes --show-leak-kinds=all \
	./mytalk 1234