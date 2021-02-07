CC = gcc
CFLAGS = -g -Wall -pthread
TARGET = jsanic
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

ALL = $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

debug: CFLAGS+=-fsanitize=address
debug: $(TARGET)

.PHONY: clean install uninstall
install: $(TARGET)
	install -o root -g root -m 555 $(TARGET) /opt/$(TARGET)
uninstall:
	rm /opt/$(TARGET)
clean:
	rm -f $(OBJ) $(TARGET)
