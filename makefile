CC = gcc
CFLAGS = -Wall -g
LD = gcc
LDFLAGS = -g

all: ftalk ftalk.o

ftalk: ftalk.o
	$(CC) $(CFLAGS) -L ~pn-cs357/Given/Talk/lib64 -o ftalk ftalk.o -ltalk \
	-l ncurses

ftalk.o: ftalk.c
	$(CC) $(CFLAGS) -I ~pn-cs357/Given/Talk/include -c -o ftalk.o ftalk.c

clean: ftalk
	rm ftalk.o
server: ftalk
	./ftalk hoster
client: ftalk
	./ftalk hoster 1025
valgrind: timit
	valgrind -s --leak-check=full --track-origins=yes --show-leak-kinds=all \
	./timeit 3