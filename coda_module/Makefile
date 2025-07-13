CC = gcc
CFLAGS = -Wall -g -finput-charset=UTF-8 -fexec-charset=UTF-8
LDFLAGS_SERVER = -lpthread
LDFLAGS_CLIENT = -lncursesw

# Targets and sources
SERVER_TARGET = server
CLIENT_TARGET = client

SERVER_SOURCES = server.c davinci.c
CLIENT_SOURCES = client.c

# Default target: build both server and client
all: $(SERVER_TARGET) $(CLIENT_TARGET)

# Build server
$(SERVER_TARGET): $(SERVER_SOURCES)
	$(CC) $(CFLAGS) -o $(SERVER_TARGET) $(SERVER_SOURCES) $(LDFLAGS_SERVER)

# Build client
$(CLIENT_TARGET): $(CLIENT_SOURCES)
	$(CC) $(CFLAGS) -o $(CLIENT_TARGET) $(CLIENT_SOURCES) $(LDFLAGS_CLIENT)

# Clean build files
clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)

# Phony targets
.PHONY: all clean
