CC=gcc
CFLAGS= -std=c99 -pedantic -Wall -g3


.PHONY: lzw

lzw: encode decode

encode: lzw.o StringTable.o code.o
	${CC} ${CFLAGS} -o $@ $^

decode: lzw.o StringTable.o code.o
	${CC} ${CFLAGS} -o $@ $^

lzw.o: lzw.c
	${CC} ${CFLAGS} -c $^

StringTable.o: StringTable.c
	${CC} ${CFLAGS} -c $^


