CC = gcc
CFLAGS = -g -Wall -pthread
TARGET = jsanic
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

ALL = $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f $(OBJ) $(TARGET)
