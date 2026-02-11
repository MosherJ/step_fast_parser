CC = gcc
CFLAGS = -Wall -O3 -pthread -D_GNU_SOURCE
TARGET = step_fast_parser
SOURCES = step_fast_parser.c
HEADERS = step_protocol.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET) *.csv

run: $(TARGET)
	./$(TARGET) input_data.bin output_data 8

debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

.PHONY: all clean run debug