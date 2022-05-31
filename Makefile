CC = gcc
CFLAGS = -g -Wall

nee : nee.o

nee.o : nee.c
	cc -c nee.c
clean : 
	rm *.o
	
