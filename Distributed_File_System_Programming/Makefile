CC = gcc
CFLAGS = -Wall -Wextra -std=c99

CLIENT_DIRS = Client_1 Client_2 Client_3
CLIENT_EXES = $(CLIENT_DIRS:%=%/dfc)

SERVER_DIR = Servers
SERVER = $(SERVER_DIR)/dfs.c
SERVER_EXE = $(SERVER_DIR)/dfs

all: $(CLIENT_EXES) $(SERVER_EXE)

$(CLIENT_EXES): %/dfc: %/dfc.c
	$(CC) $(CFLAGS) -o $@ $<

$(SERVER_EXE): $(SERVER)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(CLIENT_EXES) $(SERVER_EXE)

