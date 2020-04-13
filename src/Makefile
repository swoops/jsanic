CC = gcc
CFLAGS = -g -Wall
TARGET = jspurty
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

ALL = $(TARGET)

$(TARGET): $(OBJ) tokens.h
	$(CC) $(CFLAGS) -o $@ $^

tokenizer.o: tokenizer.c tokenizer.h tokens.h
	$(CC) $(CFLAGS) -o $@ -c $<

tokens.h: compile_tokens.py
	python3 compile_tokens.py

.PHONY: clean
clean:
	rm -f $(OBJ) $(TARGET) tokens.h
