CC=gcc
CFLAGS=-Wall -pedantic -lrt -lpthread -std=c99 -lm
SOURCES=src/application.c src/utils/utils.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=tp1

all: linkedit slave

linkedit: compile $(OBJECTS)

	$(CC) $(OBJECTS) -o $(EXECUTABLE) $(CFLAGS)

slave: src/slave.o src/utils/utils.o
	$(CC) src/slave.o src/utils/utils.o -o src/slave $(CFLAGS)

compile: $(SOURCES)

src/application.o:
	$(CC) -c src/application.c -o src/application.o $(CFLAGS)

src/slave.o:
	$(CC) -c src/slave.c -o src/slave.o $(CFLAGS)

src/utils/utils.o:
	$(CC) -c src/utils/utils.c -o src/utils/utils.o $(CFLAGS)

clean:
	rm $(OBJECTS) src/slave.o src/slave ${EXECUTABLE}
