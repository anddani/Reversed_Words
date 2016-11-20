SOURCES = reversed_words.c
PROGRAM = reversed_words
CC = gcc
CFLAGS = -Wall -lpthread

all: reversed_words

reversed_words: clean
	$(CC) $(CFLAGS) -o $(PROGRAM) reversed_words.c

threads: clean
	$(CC) $(CFLAGS) -DUSE_THREADS -o $(PROGRAM) reversed_words.c

clean:
	rm -rf $(PROGRAM)
