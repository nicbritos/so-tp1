CC=gcc
CFLAGS=-Wall -pedantic -lrt -lpthread -std=c99 -lm
SOURCES=src/application.c src/utils/utils.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=tp1

all: linkedit

linkedit: compile $(OBJECTS)
	$(CC) $(OBJECTS) -o $(EXECUTABLE) $(CFLAGS)

compile: $(SOURCES)

src/application.c:
	$(CC) -c src/application.c $(CFLAGS)

src/slave.c:
	$(CC) -c src/slave.c $(CFLAGS)

clean:
	rm $(OBJECTS) ${EXECUTABLE}
