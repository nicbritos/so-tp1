CC=gcc
CFLAGS=-Wall -pedantic -lrt -lpthread -std=c99
SOURCES=application.c utils/utils.c utils/FileWalker.c
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=tp1

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(SOURCES)	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	
# application.c: $(OBJECTS)	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	
# utils/utils.c: $(OBJECTS)	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:	$(CC) $(CFLAGS) $< -o $@

clean:	rm -rf *.o ${EXECUTABLE}
