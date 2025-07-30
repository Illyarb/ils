CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGET = ils
SOURCES = main.c logo.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = config.h logo.h

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/
	chmod +x /usr/local/bin/$(TARGET)
	rm -rf $(HOME)/.local/share/ils/icons


uninstall:
	rm -f /usr/local/bin/$(TARGET)

debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)
