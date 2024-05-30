CC = gcc

CLIENT_DIR = client
SERVER_DIR = server

all: $(CLIENT_DIR)/client $(SERVER_DIR)/server

$(CLIENT_DIR)/client: $(CLIENT_DIR)/client.c
	$(CC) -o $@ $<

$(SERVER_DIR)/server: $(SERVER_DIR)/server.c
	$(CC) -o $@ $<

clean:
	rm -f $(CLIENT_DIR)/client $(SERVER_DIR)/server
