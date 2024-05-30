#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <signal.h>
#include <sys/stat.h>

#define MAX_CREDENTIALS 100
#define BUFSIZE 1024
#define BUFSIZ 1024
#define LISTENQ  1024  /* second argument to listen() */
//const char* PATH = "/Users/master/Classes/Network_system/tutr6272_PA4";
char* PATH;

// Define a struct for credentials
struct Credential {
    char username[50];
    char password[50];
};

struct sigaction old_action;

// Global creadentials that will be assigned in main by parsing the config file, then being used in each thread for user's requests authentication
struct Credential* credentials;
int credentialCount;
int socket_list[10];
int socket_opened = 0;
char *server_folder;

void sigint_handler(int sig_no)
{
    printf("\nCTRL-C pressed, %s terminating...\n", server_folder);
    // Free the allocated memory for credentials before terminating
    free(credentials);
    // close all opened socket
    for(int i = 0; i<socket_opened; i++){
        printf("Closing socket: %d \n", socket_list[i]);
        close(socket_list[i]);
    }
    sigaction(SIGINT, &old_action, NULL);
    kill(0, SIGINT);
}

int folderExists(const char* folderName) {
    struct stat st;
    return stat(folderName, &st) == 0 && S_ISDIR(st.st_mode);
}

int createFolder(const char* folderName) {
    if (mkdir(folderName, 0777) == 0) {
        printf("Folder created successfully.\n");
        return 1;
    } else {
        perror("Error creating folder");
        return 0;
    }
}

bool extract_n_verify_usr_pwd(const char *input, char** username) {
    // Make a copy of the input string to avoid modifying the original
    char inputCopy[256];  // Adjust the size based on your input constraints
    strncpy(inputCopy, input, sizeof(inputCopy) - 1);
    inputCopy[sizeof(inputCopy) - 1] = '\0';  // Ensure null-termination

    // Initialize an array to store the substrings
    char *substrings[3];
    int count = 0;

    // Tokenize the string using space as a delimiter
    char *token = strtok(inputCopy, " ");
    while (token != NULL) {
        substrings[count++] = token;
        token = strtok(NULL, " ");
    }

    // Remove \n from the Password substring
    //substrings[count - 1][strlen(substrings[count - 1]) -1] = '\0';

    // Check if there are at least 2 substrings
    if (count >= 2) {
        // Print the last two substrings
        //printf("Last two substrings: %s, %s\n", substrings[count - 2], substrings[count - 1]);
        for (int i = 0; i < credentialCount; i++) {
            if (strcmp(substrings[count - 2], credentials[i].username) == 0){
                printf("User matched: %s \n", credentials[i].username);
                *username = credentials[i].username;
                if (strcmp(substrings[count - 1], credentials[i].password) == 0){
                    printf("Password matched: %s \n", credentials[i].password);
                    return true;
                }
            }
            //printf("(%s, %s)\n", credentials[i].username, credentials[i].password);
        }
        return false;
    } else {
        // Handle the case where there are fewer than 2 substrings
        printf("No username and password specified in the request\n");
        return false;
    }
}

int countTokens(const char *str, const char *delimiter) {
    int count = 0;
    char *token = strtok((char *)str, delimiter);

    while (token != NULL) {
        count++;
        token = strtok(NULL, delimiter);
    }

    return count;
}

char* readDir(const char *dirPath) {
    DIR *dir;
    struct dirent *entry;
    char *entries = (char*)malloc(BUFSIZ);
    bzero(entries, BUFSIZ);

    // Open the directory
    dir = opendir(dirPath);

    // Check if the directory was opened successfully
    if (dir == NULL) {
        printf("Error opening directory %s \n", dirPath);
    }

    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Exclude "." and ".." entries
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            strcat(entries, entry->d_name);
            strcat(entries, " ");
            strcat(entries, (entry->d_type == DT_DIR) ? "[directory], " : "[file], ");
        }
    }

    // Close the directory
    closedir(dir);

    return entries;
}

char* extract_subfolder(const char* input) {
    // Make a copy of the input string to avoid modifying the original
    char inputCopy[256];  // Adjust the size based on your input constraints
    strncpy(inputCopy, input, sizeof(inputCopy) - 1);
    inputCopy[sizeof(inputCopy) - 1] = '\0';  // Ensure null-termination

    int token_count = countTokens(input, " ");
    if (token_count < 4){
        return NULL;
    }
    // Tokenize the string using space as a delimiter
    char* token = strtok(inputCopy, " ");

    // Check if the first token is "MKDIR"
    if ((token != NULL && strcmp(token, "MKDIR")) == 0 || (token != NULL && strcmp(token, "LIST")) == 0 || (token != NULL && strcmp(token, "PUT")) == 0 || (token != NULL && strcmp(token, "GET")) == 0 ) {
        // Get the next token, which is the subfolder
        token = strtok(NULL, " ");

        // Check if the subfolder token is not NULL
        if (token != NULL) {
            // Return the subfolder
            return token;
        }
    }

    // Return NULL if "MKDIR" is not found or if the subfolder token is NULL
    return NULL;
}

// Function to parse the dfs.config file and return an array of credentials
struct Credential* parseCredentials(const char* filename, int* credentialCount) {
    // Construct the full file path
    char fullFilePath[100];
    snprintf(fullFilePath, sizeof(fullFilePath), "%s%s", PATH, filename);

    // Open the file for reading
    FILE *file = fopen(fullFilePath, "r");

    // Check if the file was opened successfully
    if (file == NULL) {
        perror("Error opening file");
        exit(1); // Exit the program with an error code
    }

    // Allocate memory for the array of credentials
    struct Credential* credentials = (struct Credential*)malloc(MAX_CREDENTIALS * sizeof(struct Credential));
    *credentialCount = 0;

    // Buffer to store each line read from the file
    char line[100];

    // Read the file line by line
    while (fgets(line, sizeof(line), file) != NULL) {
        // Check if the line contains "Username:"
        if (strstr(line, "Username:") != NULL) {
            // Extract the username
            sscanf(line, "Username: %s", credentials[*credentialCount].username);

            // Read the next line for the password
            fgets(line, sizeof(line), file);

            // Extract the password
            sscanf(line, "Password: %s", credentials[*credentialCount].password);

            // Increment the credential count
            (*credentialCount)++;

            // Check if we have reached the maximum number of credentials
            if (*credentialCount >= MAX_CREDENTIALS) {
                printf("Too many credentials. Increase MAX_CREDENTIALS.\n");
                break;
            }
        }
    }

    // Close the file
    fclose(file);

    // Return the array of credentials
    return credentials;
}

/*
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure
 */
int open_listenfd(int port)
{
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} /* end open_listenfd */



void resolve_request(int connfd){
    char *buf = (char *)malloc(BUFSIZE);
    bzero(buf, BUFSIZE);
    buf[0] = '\0';
    int n = 0;
    int socket_byte_count = 0;
    bool is_socket_close = false; // client would send a terminating message, so that server can close the socket and kill this thread
    const char* terminate_message = "terminate message from client";
    const char* invalid_message = "Invalid Username/Password. Please try again. \n";
    const char* valid_message = "Valid Username/Password.\n";
    const char* empty_folder_message = "Requested folder is empty\n";
    bool is_request_valid = false;
    char* username;
    char* user_folder = (char *)malloc(BUFSIZE);
    char* user_subfolder = (char *)malloc(BUFSIZE);

    while (!is_socket_close){
        bzero(buf, BUFSIZE);
        buf[0] = '\0';
        bzero(user_folder, BUFSIZE);
        user_folder[0] = '\0';
        bzero(user_subfolder, BUFSIZE);
        user_subfolder[0] = '\0';

        socket_byte_count = 0;
        while(socket_byte_count == 0) ioctl(connfd, FIONREAD, &socket_byte_count); // wait for further data

        n = read(connfd, buf, BUFSIZE); //waiting for new request, better with a timeout here
        if (strstr(buf, terminate_message) != NULL) {
            is_socket_close = true;
            printf("Received terminating message from connfd = %d \n", connfd);
        }
        else{
            printf("connfd = %d, n = %d \n",connfd, n);
            printf("data received: %s \n", buf);
            is_request_valid = extract_n_verify_usr_pwd(buf, &username);
            if (!is_request_valid){
                send(connfd, invalid_message, strlen(invalid_message), 0);
            }
            else{
                strcpy(user_folder,PATH);
                strcat(user_folder,server_folder);
                strcat(user_folder,"/");
                strcat(user_folder,username);

                // check if the user's folder exists, if not create one
                if (!folderExists(user_folder)) {
                    if (createFolder(user_folder)) {
                        printf("User folder created: %s\n", user_folder);
                    } else {
                        printf("Failed to create user folder: %s\n", user_folder);
                    }
                } else {
                    printf("User folder already exists: %s\n", user_folder);
                }

                // handle MKDIR request
                if(strstr(buf, "MKDIR") != NULL){
                    char* subfolder = extract_subfolder(buf);
                    if (subfolder){
                        strcpy(user_subfolder,user_folder);
                        strcat(user_subfolder,"/");
                        strcat(user_subfolder,subfolder);
                        char response[100];

                        if (!folderExists(user_subfolder)) {
                            if (createFolder(user_subfolder)) {
                                printf("User subfolder created: %s\n", user_subfolder);
                                sprintf(response, "User subfolder created: %s\n", user_subfolder);
                            } else {
                                printf("Failed to create user subfolder: %s\n", user_subfolder);
                                sprintf(response, "Failed to create user subfolder: %s, try again...\n", user_subfolder);
                            }
                        } else {
                            printf("User subfolder already exists: %s\n", user_subfolder);
                            sprintf(response, "User subfolder already exists: %s\n", user_subfolder);
                        }
                        send(connfd, response, strlen(response), 0);
                    }
                    else{
                        printf("MKDIR request doesn't specify subfolder name\n");
                        send(connfd, "MKDIR request doesn't specify subfolder name\n", strlen("MKDIR request doesn't specify subfolder name\n"), 0);
                    }
                }
                else if(strstr(buf, "LIST") != NULL){
                    char* subfolder = extract_subfolder(buf);
                    if (subfolder){
                        strcpy(user_subfolder,user_folder);
                        strcat(user_subfolder,"/");
                        strcat(user_subfolder,subfolder);

                        if (!folderExists(user_subfolder)) {
                            if (createFolder(user_subfolder)) {
                                printf("User subfolder created: %s\n", user_subfolder);
                                // since the folder just created, send back empty folder message
                                send(connfd, empty_folder_message, strlen(empty_folder_message), 0);
                            } else {
                                printf("Failed to create user user subfolder: %s\n", user_subfolder);
                            }
                        } else {
                            // user's requested folder exists, check for files and send back the list of file
                            char* file_list = readDir(user_subfolder);
                            if (file_list){
                                strcat(file_list,"\n");
                                send(connfd, file_list, strlen(file_list), 0);
                            }
                            else{
                                send(connfd, "Requested folder is empty\n", strlen("Requested folder is empty\n"), 0);
                            }
                            free(file_list);
                        }
                    }
                    else{
                        // LIST request doesn't contain subfolder, sending back files in the root user folder
                        char* file_list = readDir(user_folder);
                        if (file_list){
                            strcat(file_list,"\n");
                            send(connfd, file_list, strlen(file_list), 0);
                        }
                        else{
                            send(connfd, "Requested folder is empty\n", strlen("Requested folder is empty\n"), 0);
                        }
                        free(file_list);
                    }

                }
                else if(strstr(buf, "PUT") != NULL){ // PUT 1.txt subfolder/ username password
                    //char* file_name = extract_subfolder(buf);

                    char firstSubstring[256];
                    char secondSubstring[256];
                    char thirdSubstring[256];
                    char forthSubstring[256];
                    char fifthSubstring[256];

                    // Use sscanf to extract substrings
                    int num_substring = sscanf(buf, "%s %s %s %s %s", firstSubstring, secondSubstring, thirdSubstring, forthSubstring, fifthSubstring);
                    char *filepath = (char*)malloc(BUFSIZ);
                    strcpy(filepath, PATH);
                    strcat(filepath, server_folder);
                    strcat(filepath,"/");
                    strcat(filepath, username);
                    strcat(filepath, "/");
                    if (num_substring == 4){ // no specified subfolder, receive and store the file in root user directory
                        strcat(filepath, secondSubstring);
                    }
                    else if (num_substring == 5){ // there is specified subfolder, receive and store the file in subfolder
                        strcat(filepath, thirdSubstring);
                        strcat(filepath, secondSubstring);
                    }
                    FILE *receive_file = fopen(filepath, "wb");
                    bool is_transfer_done = false;
                    size_t bytesRead = 0;
                    char* receive_buffer = (char*)malloc(BUFSIZ);
                    printf("File path: %s \n", filepath);

                    //while ((bytesRead = recv(connfd, receive_buffer, sizeof(receive_buffer), 0)) > 0) {
                    while (true){
                        bytesRead = read(connfd, receive_buffer, BUFSIZE);
                        char *done_position = strstr(receive_buffer, "DONE\t\n\n");
                        if (done_position){
                            // If the substring is found, remove it
                            //size_t substringLength = strlen("DONE\t\n\n");
                            //size_t remainingLength = strlen(done_position + substringLength);
                            // Shift the remaining characters to fill the gap
                            //memmove(done_position, done_position + substringLength, remainingLength + 1);
                            //printf("Data received: %s\n", receive_buffer);
                            //fwrite(receive_buffer, bytesRead - substringLength, 1, receive_file);
                            printf("Received DONE message from client \n");
                            break;
                        }
                        else{
                            printf("Data received: %s\n", receive_buffer);
                            fwrite(receive_buffer, 1, bytesRead, receive_file);
                            bzero(receive_buffer, BUFSIZ);
                        }
                    }
                    fclose(receive_file);
                    free(receive_buffer);
                    printf("File received and written: %s \n", filepath);
                }
                else if(strstr(buf, "GET") != NULL){ // PUT 1.txt subfolder/ username password
                    char firstSubstring[256];
                    char secondSubstring[256];
                    char thirdSubstring[256];
                    char forthSubstring[256];
                    char fifthSubstring[256];

                    // Use sscanf to extract substrings
                    int num_substring = sscanf(buf, "%s %s %s %s %s", firstSubstring, secondSubstring, thirdSubstring, forthSubstring, fifthSubstring);
                    char *filepath = (char*)malloc(BUFSIZ);
                    strcpy(filepath, PATH);
                    strcat(filepath, server_folder);
                    strcat(filepath,"/");
                    strcat(filepath, username);
                    strcat(filepath, "/");
                    if (num_substring == 4){ // no specified subfolder, receive and store the file in root user directory
                        strcat(filepath, secondSubstring);
                    }
                    else if (num_substring == 5){ // there is specified subfolder, receive and store the file in subfolder
                        strcat(filepath, thirdSubstring);
                        strcat(filepath, secondSubstring);
                    }
                    // try to open the requested piece
                    FILE *send_file = fopen(filepath, "rb");
                    // if piece exists on this server -> response OK and start sending the piece
                    if (send_file){
                        send(connfd, "OK\t\n\n", sizeof("OK\t\n\n"), 0);
                        usleep(100000);

                        size_t byteSent = 0;
                        char *file_buffer = (char *) malloc(BUFSIZ);
                        bzero(file_buffer, BUFSIZ);
                        size_t bytesRead;

                        printf("Sending %s to connfd %d \n", filepath, connfd);

                        // read and send file content to client
                        while ((bytesRead = fread(file_buffer, 1, BUFSIZ, send_file)) > 0) {
                            if (connfd != -1) {
                                printf("Sending %lu bytes: %s\n", bytesRead, file_buffer);
                                send(connfd, file_buffer, bytesRead, 0);
                            }
                            byteSent += bytesRead;
                            bzero(file_buffer, BUFSIZ);
                        }
                        // send DONE message to wrap up sending file content
                        usleep(100000);
                        send(connfd, "DONE\t\n\n", BUFSIZ, 0);

                        printf("Done sending %s with %lu bytes to connfd %d \n", filepath, byteSent, connfd);
                        free(file_buffer);
                    }
                    // otherwise, sending a NOPE message to client so it moves on finding another server that may have the wanted piece
                    else{
                        send(connfd, "NOPE\t\n\n", strlen("NOPE\t\n\n"), 0);
                    }
                    free(filepath);
                    fclose(send_file);
                }
            }
        }
    }
    free(buf);
    free(user_folder);
    free(user_subfolder);
}

/* thread routine */
void * thread(void * vargp)
{
    int connfd = *((int *)vargp);
    printf("New thread is created: %lu\n", (unsigned long)pthread_self());
    pthread_detach(pthread_self());
    free(vargp);
    resolve_request(connfd);
    printf("Client closed socket: %d\n", connfd);
    printf("Thread killed: %lu\n", (unsigned long)pthread_self());
    close(connfd);
    return NULL;
}

int main(int argc, char *argv[]) {
    size_t path_size = pathconf(".", _PC_PATH_MAX);
    PATH = (char *)malloc((size_t)path_size);
    getcwd(PATH, path_size);
    printf("Current working directory: %s \n", PATH);
    // handle ctl + c
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sigint_handler;
    sigaction(SIGINT, &action, &old_action);


    // Check if there are enough command-line arguments
    if (argc < 3) {
        printf("Usage: ./dfs <folder_name> <port>\n");
        return 1; // Return an error code
    }

    // Access and use the command-line argument
    server_folder = argv[1];
    //int port = atoi(argv[2]);

    printf("Server folder name: %s \n", server_folder);
    //printf("Port: %d \n", port);

    // Call the function to parse credentials
    credentials = parseCredentials("/dfs.conf", &credentialCount);

    // Print the stored credentials
    printf("Stored Credentials:\n");
    for (int i = 0; i < credentialCount; i++) {
        printf("(%s, %s)\n", credentials[i].username, credentials[i].password);
    }

    int listenfd, *connfdp, port;
    unsigned int clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid;

    port = atoi(argv[2]);
    listenfd = open_listenfd(port);

    while (1) {
        connfdp = malloc(sizeof(int));
        *connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
        socket_list[socket_opened] = *connfdp;
        socket_opened = socket_opened + 1;
        pthread_create(&tid, NULL, thread, connfdp);
    }

    return 0; // Return 0 to indicate successful execution
}

