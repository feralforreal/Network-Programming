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


#define BUFFER_SIZE 1024
#define NUM_SERVERS 4

#define MAX_LINE_LENGTH 1024
//const char* PATH = "/Users/master/Classes/Network_system/PA4_client";
char* PATH;
const char* terminate_message = "terminate message from client";
struct sigaction old_action;
int clientSocket[4];
struct sockaddr_in DFS[4];

typedef struct {
    char username[MAX_LINE_LENGTH];
    char password[MAX_LINE_LENGTH];
    char dfsAddresses[4][MAX_LINE_LENGTH];
    int dfsPorts[4];
    bool is_server_online[4];
} ConfigData;

void sigint_handler(int sig_no)
{
    printf("\nCTRL-C pressed, sending terminating message to servers...\n");

    for (int i = 0; i < 4; i++){
        send(clientSocket[i], terminate_message, strlen(terminate_message), 0);
        close(clientSocket[i]);
    }
    // Free the allocated memory for credentials before terminating
    sigaction(SIGINT, &old_action, NULL);
    kill(0, SIGINT);
}

void extractConfigData(const char *filename, ConfigData *config) {
    char fullFilePath[100];
    snprintf(fullFilePath, sizeof(fullFilePath), "%s/%s", PATH, filename);

    FILE *file = fopen(fullFilePath, "r");

    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, "Server", 6) == 0) {
            // Parse Server line
            char dfsName[MAX_LINE_LENGTH];
            char dfsAddress[MAX_LINE_LENGTH];
            int dfsPort;

            sscanf(line, "Server %s %s:%d", dfsName, dfsAddress, &dfsPort);

            // Store information in the config struct
            int dfsIndex = dfsName[3] - '1'; // Assumes DFS names are DFS1, DFS2, DFS3, DFS4
            strcpy(config->dfsAddresses[dfsIndex], dfsAddress);
            config->dfsPorts[dfsIndex] = dfsPort;
        } else if (strncmp(line, "Username:", 9) == 0) {
            // Parse Username line
            sscanf(line, "Username: %s", config->username);
        } else if (strncmp(line, "Password:", 9) == 0) {
            // Parse Password line
            sscanf(line, "Password: %s", config->password);
        }
    }

    fclose(file);
}

char* removeDuplicates(char* input) {
    char* result = (char*)malloc(BUFFER_SIZE);
    bzero(result, BUFFER_SIZE);
    result[0] = '\0';

    char* token = NULL;
    char* context = NULL;

    // Tokenize the input string using ',' as a delimiter
    token = strtok_r(input, ",", &context);

    while (token != NULL) {
        // Check if the current token is not already in the result string
        if (strstr(result, token) == NULL) {
            strcat(result, token);
            strcat(result, ",");
        }

        // Move to the next token
        token = strtok_r(NULL, ",", &context);
    }
    return result;
}

char* extractDir(char (*buffer)[BUFFER_SIZE], int bufferCount) {
    char *dir_tmp = (char*) malloc(BUFFER_SIZE);
    for (int i = 0; i < bufferCount; ++i) {
        // Create a copy of the buffer to avoid modifying the original string
        char *bufferCopy = strdup(buffer[i]);

        if (bufferCopy == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(EXIT_FAILURE);
        }

        char *token = strtok(bufferCopy, ",");
        while (token != NULL) {
            // Trim leading and trailing whitespaces
            while (*token == ' ' || *token == '\t') {
                ++token;
            }
            size_t len = strlen(token);
            while (len > 0 && (token[len - 1] == ' ' || token[len - 1] == '\t')) {
                --len;
            }

            // Check if the trimmed token ends with "[directory]"
            if (len > 11 && strcmp(token + len - 11, "[directory]") == 0) {
                //printf("Directory entry: %.*s\n", (int)(len - 11), token);
                strcat(dir_tmp, token);
                strcat(dir_tmp, ",");
            }

            // Move to the next token
            token = strtok(NULL, ",");
        }
        // Free the allocated memory
        free(bufferCopy);
        //free(dir_tmp);
    }
    if (dir_tmp != NULL && strlen(dir_tmp) > 0) {
        dir_tmp[strlen(dir_tmp) - 1] = '\0';  // Replace the last character with null terminator
    }
    return removeDuplicates(dir_tmp);
}

char* extractFile(char (*buffer)[BUFFER_SIZE], int bufferCount) {
    char *dir_tmp = (char*) malloc(BUFFER_SIZE);
    for (int i = 0; i < bufferCount; ++i) {
        // Create a copy of the buffer to avoid modifying the original string
        char *bufferCopy = strdup(buffer[i]);

        if (bufferCopy == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(EXIT_FAILURE);
        }

        char *token = strtok(bufferCopy, ",");
        while (token != NULL) {
            // Trim leading and trailing whitespaces
            while (*token == ' ' || *token == '\t') {
                ++token;
            }
            size_t len = strlen(token);
            while (len > 0 && (token[len - 1] == ' ' || token[len - 1] == '\t')) {
                --len;
            }

            // Check if the trimmed token ends with "[directory]"
            if (len > 6 && strcmp(token + len - 6, "[file]") == 0) {
                //printf("Directory entry: %.*s\n", (int)(len - 11), token);
                strcat(dir_tmp, token);
                strcat(dir_tmp, ",");
            }

            // Move to the next token
            token = strtok(NULL, ",");
        }
        // Free the allocated memory
        free(bufferCopy);
        //free(dir_tmp);
    }
    if (dir_tmp != NULL && strlen(dir_tmp) > 0) {
        dir_tmp[strlen(dir_tmp) - 1] = '\0';  // Replace the last character with null terminator
    }
    return removeDuplicates(dir_tmp);
}

void removeLast9Characters(char *inputString) {
    // Find the length of the input string
    size_t length = strlen(inputString);

    // Check if the length is at least 9 characters, as we want to remove the last 9 characters
    if (length >= 9) {
        // Calculate the new length after removing the last 9 characters
        size_t newLength = length - 9;

        // Set the new length as the null-terminator position to truncate the string
        inputString[newLength] = '\0';
    }
}

int count_pieces(const char *inputString, const char *substring) {
    int count = 0;
    size_t substringLength = strlen(substring);

    const char *position = inputString;

    while ((position = strstr(position, substring)) != NULL) {
        count++;
        position += substringLength;
    }

    return count;
}
char* removeFirstCharacter(const char* inputString) {
    // Check if the input string is empty or already one character long
    if (inputString == NULL || inputString[0] == '\0' || inputString[1] == '\0') {
        // Return an empty string or a copy of the original string
        return strdup("");
    }

    // Calculate the length of the input string
    size_t length = strlen(inputString);

    // Allocate memory for the new string
    char* result = (char*)malloc((length) * sizeof(char));

    // Check if memory allocation was successful
    if (result == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Copy the original string starting from the second character
    strncpy(result, inputString + 1, length);

    // Null-terminate the new string
    result[length - 1] = '\0';

    return result;
}

char* check_file_pieces(char *inputString) {
    int piece_count = 0;
    char* tmp_string = (char*)malloc(BUFFER_SIZE);
    char* output_string = (char*)malloc(BUFFER_SIZE);
    char* original_string = (char*)malloc(BUFFER_SIZE);
    bzero(tmp_string, BUFFER_SIZE);
    bzero(output_string, BUFFER_SIZE);
    bzero(original_string, BUFFER_SIZE);
    strcpy(original_string, inputString);
    char *token = strtok(inputString, ",");

    while (token != NULL) {
        // Remove the last 9 characters from each token
        removeLast9Characters(token);

        // Print the modified token
        //printf("%s\n", token);
        strcat(tmp_string,token);
        strcat(tmp_string, ",");

        // Get the next token
        token = strtok(NULL, ",");
    }
    tmp_string = removeDuplicates(tmp_string);
    token = strtok(tmp_string, ",");
    while (token != NULL) {
        piece_count = count_pieces(original_string, token);
        if (piece_count == 4){
            char* remove_first_dot = removeFirstCharacter(token);
            strcat(output_string, remove_first_dot);
            strcat(output_string, "[complete], ");
        }
        else{
            char* remove_first_dot = removeFirstCharacter(token);
            strcat(output_string, remove_first_dot);
            strcat(output_string, "[incomplete], ");
        }
        // Get the next token
        token = strtok(NULL, ",");
    }
    free(tmp_string);
    free(original_string);
    return output_string;
}


/*
int main() {
    char inputString[] = ".1.txt.1 [file],.2.txt.1 [file],.1.txt.2 [file],.2.txt.2 [file],.1.txt.3 [file],.1.txt.4 [file],";
    char *outputString = check_file_pieces(inputString);
    printf("Output: %s\n", outputString);
    free(outputString);
}
 */

unsigned long hash(char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

void remove_put(char *str) {
    char *src = str;
    char *dst = str;

    while (*src) {
        if (strstr(src, "PUT") == src) {
            src += 4; // Move past "PUT "
        } else {
            *dst++ = *src++;
        }
    }

    *dst = '\0'; // Null-terminate the resulting string
}

long get_file_size(char *filename, char* fullFilePath) {
    //remove_put(filename);
    //snprintf(fullFilePath, sizeof(fullFilePath), "%s/%s", PATH, filename);
    strcpy(fullFilePath, PATH);
    strcat(fullFilePath, "/");
    strcat(fullFilePath, filename);

    FILE *file = fopen(fullFilePath, "rb"); // Open the file in binary mode

    if (file == NULL) {
        printf("File doesn't exist: %s\n", fullFilePath);
        return -1; // Return -1 to indicate an error
    }

    fseek(file, 0, SEEK_END); // Move the file pointer to the end of the file
    long size = ftell(file);  // Get the current position of the file pointer

    fclose(file); // Close the file

    return size;
}

void XOR_EncryptDecrypt(char *data, size_t size, char *xorKey) {
    // Calculate length of XOR key
    int keyLen = strlen(xorKey);

    // Perform XOR operation of key
    // with every byte in data
    for (size_t i = 0; i < size; i++) {
        data[i] = data[i] ^ xorKey[i % keyLen];
    }
}

size_t send_each_piece(int socket_1, int socket_2, long num_buf_fill, long remaining_byte, ConfigData *config, FILE *file, char* message){
    size_t byteSent = 0;
    char *file_buffer = (char *) malloc(BUFFER_SIZE);
    size_t bytesRead;

    printf("Sending %s\n", message);

    // if each file piece is larger than the buffer => send full buffers first
    for (int i = 0; i < num_buf_fill; i++) {
        bzero(file_buffer, BUFFER_SIZE);
        bytesRead = fread(file_buffer, 1, BUFFER_SIZE, file);
        printf("Before encryption file content: %s\n", file_buffer);
        //encrypt file_buffer with password before sending to servers
        XOR_EncryptDecrypt(file_buffer, BUFFER_SIZE, config->password);
        if (socket_1 != -1) {
            printf("Sending encrypted file content: %s\n", file_buffer);
            send(socket_1, file_buffer, bytesRead, 0);
        }
        if (socket_2 != -1) {
            send(socket_2, file_buffer, bytesRead, 0);
        }
        byteSent += bytesRead;
    }
    // send the remaining byte of the file piece
    if (remaining_byte > 0) {
        bzero(file_buffer, BUFFER_SIZE);
        bytesRead = fread(file_buffer, 1, remaining_byte, file);
        printf("Before encryption file content: %s\n", file_buffer);
        //encrypt file_buffer with password before sending to servers
        XOR_EncryptDecrypt(file_buffer, remaining_byte, config->password);
        if (socket_1 != -1) {
            printf("Sending encrypted file content: %s\n", file_buffer);
            write(socket_1, file_buffer, bytesRead);
        }
        if (socket_2 != -1) {
            write(socket_2, file_buffer, bytesRead);
        }
        byteSent += bytesRead;
    }

    usleep(100000);

    // send end message to servers to close the file piece
    if (socket_1 != -1) {
        send(socket_1, "DONE\t\n\n", BUFSIZ, 0);
    }
    if (socket_2 != -1) {
        send(socket_2, "DONE\t\n\n", BUFSIZ, 0);
    }
    printf("Bytes Sent - %s: %zu\n", message,byteSent);

    free(file_buffer);

    return byteSent;
}

void check_online_servers(ConfigData * config){
    char *userInput = (char*)malloc(BUFFER_SIZE);
    char *buffer =  (char*)malloc(BUFFER_SIZE);
    bzero(userInput, BUFFER_SIZE);
    bzero(buffer, BUFFER_SIZE);

    strcpy(userInput, "LIST ");
    strcat(userInput, config->username);
    strcat(userInput, " ");
    strcat(userInput, config->password);
    for (int i = 0; i < 4; i++){
        if (config->is_server_online[i]){
            if (send(clientSocket[i], userInput, BUFSIZ, 0) == -1){
                config->is_server_online[i] = false;
            }
        }
    }
    for (int i = 0; i < 4; i++){
        size_t bytesRead = 0;
        bzero(buffer, BUFFER_SIZE);
        buffer[0] = '\0';
        if(config->is_server_online[i]){
            bytesRead = recv(clientSocket[i], buffer, BUFFER_SIZE, 0);
            if(bytesRead == 0){
                config->is_server_online[i] = false;
                printf("Server is offline: %s\n", config->dfsAddresses[i]);
            }
        }
    }

    free(buffer);
    free(userInput);
}

void retry_connect(ConfigData *config){
    for (int i = 0; i < 4; ++i) {
        char dfsAddress[MAX_LINE_LENGTH];
        int dfsPort;
        sscanf(config->dfsAddresses[i], "%[^:]:%d", dfsAddress, &dfsPort);

        if(config->is_server_online[i] == false){
            clientSocket[i] = socket(AF_INET, SOCK_STREAM, 0);
            DFS[i].sin_family = AF_INET;
            DFS[i].sin_addr.s_addr = inet_addr(dfsAddress);
            DFS[i].sin_port = htons(dfsPort);
            if (connect(clientSocket[i], (struct sockaddr*)&DFS[i], sizeof(DFS[i])) != -1) {
                printf("Reconnected to server at %s:%d\n", dfsAddress, dfsPort);
                config->is_server_online[i] = true;
            }
        }
    }
}

void send_file_pieces(long scheme_value, long file_size, char* fullFilePath, char*secondSubstring, char* thirdSubstring, int num_substring, ConfigData *config){
    check_online_servers(config);
    retry_connect(config);
    long file_size_divided = file_size/4;
    printf("Piece size: %lu \n", file_size_divided);

    char request_1[100];
    char request_2[100];
    char request_3[100];
    char request_4[100];
    //remove_put(userInput); // remove PUT from the user input, getting just the file name

    if (scheme_value == 0) {

        FILE *file = fopen(fullFilePath, "rb");
        int socket_1=-1, socket_2=-1;
        size_t totalBytesSent = 0;
        long num_buf_fill = file_size_divided / BUFFER_SIZE;
        long remaining_byte = file_size_divided - (BUFFER_SIZE * num_buf_fill);
        size_t byteSent = 0;

        // send PUT requests to servers
        if (num_substring == 2) {
            sprintf(request_1, "PUT .%s.1 %s %s", secondSubstring, config->username, config->password);
            sprintf(request_2, "PUT .%s.2 %s %s", secondSubstring, config->username, config->password);
            sprintf(request_3, "PUT .%s.3 %s %s", secondSubstring, config->username, config->password);
            sprintf(request_4, "PUT .%s.4 %s %s", secondSubstring, config->username, config->password);
        } else if (num_substring == 3) {
            sprintf(request_1, "PUT .%s.1 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
            sprintf(request_2, "PUT .%s.2 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
            sprintf(request_3, "PUT .%s.3 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
            sprintf(request_4, "PUT .%s.4 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
        }

        // send PUT request to server 1 and 4, then read and send the first piece
        if (config->is_server_online[0]) {
            if (send(clientSocket[0], request_1, BUFSIZ, 0) == -1) {
                config->is_server_online[0] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[0];
            }
        }
        if (config->is_server_online[3]) {
            if (send(clientSocket[3], request_1, BUFSIZ, 0) == -1) {
                config->is_server_online[3] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[3];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 1 to DFS1 and/or DFS4");
        totalBytesSent += byteSent;
        socket_1 = socket_2 = -1;

        usleep(100000);

        printf("Total Bytes Sent: %zu\n", totalBytesSent);


        // send PUT request to server 1 and 2, then read and send the second piece
        if (config->is_server_online[0]) {
            if (send(clientSocket[0], request_2, BUFSIZ, 0) == -1) {
                config->is_server_online[0] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[0];
            }
        }
        if (config->is_server_online[1]) {
            if (send(clientSocket[1], request_2, BUFSIZ, 0) == -1) {
                config->is_server_online[1] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[1];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 2 to DFS1 and/or DFS2");
        totalBytesSent += byteSent;
        socket_1 = socket_2 = -1;

        usleep(100000);

        printf("Total Bytes Sent: %zu\n", totalBytesSent);

        // TODO: config piece to send -> the last piece should cover the remaining bytes, not based on piece size -> then try reconnection to each server if send request fail for all type of commands
        // send PUT request to server 2 and 3, then read and send the third piece
        if (config->is_server_online[1]) {
            if (send(clientSocket[1], request_3, BUFSIZ, 0) == -1) {
                config->is_server_online[1] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[1];
            }
        }
        if (config->is_server_online[2]) {
            if (send(clientSocket[2], request_3, BUFSIZ, 0) == -1) {
                config->is_server_online[2] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[2];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 3 to DFS2 and/or DFS3");
        totalBytesSent += byteSent;

        printf("Total Bytes Sent: %zu\n", totalBytesSent);
        socket_1 = socket_2 = -1;

        usleep(100000);

        // send PUT request to server 3 and 4, then read and send the forth piece
        // recalculate the remaining byte -> go to last piece
        num_buf_fill = (file_size - totalBytesSent) / BUFFER_SIZE;
        remaining_byte = (file_size - totalBytesSent) - (BUFFER_SIZE * num_buf_fill);

        if (config->is_server_online[2]) {
            if (send(clientSocket[2], request_4, BUFSIZ, 0) == -1) {
                config->is_server_online[2] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[2];
            }
        }
        if (config->is_server_online[3]) {
            if (send(clientSocket[3], request_4, BUFSIZ, 0) == -1) {
                config->is_server_online[3] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[3];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 4 to DFS3 and/or DFS4");
        totalBytesSent += byteSent;

        printf("Total Bytes Sent: %zu\n", totalBytesSent);
        socket_1 = socket_2 = -1;

        usleep(100000);


        // Close the file and socket
        fclose(file);
    }

    if (scheme_value == 1) {
        FILE *file = fopen(fullFilePath, "rb");
        int socket_1=-1, socket_2=-1;
        size_t totalBytesSent = 0;
        long num_buf_fill = file_size_divided / BUFFER_SIZE;
        long remaining_byte = file_size_divided - (BUFFER_SIZE * num_buf_fill);
        size_t byteSent = 0;

        // send PUT requests to servers
        if (num_substring == 2) {
            sprintf(request_1, "PUT .%s.1 %s %s", secondSubstring, config->username, config->password);
            sprintf(request_2, "PUT .%s.2 %s %s", secondSubstring, config->username, config->password);
            sprintf(request_3, "PUT .%s.3 %s %s", secondSubstring, config->username, config->password);
            sprintf(request_4, "PUT .%s.4 %s %s", secondSubstring, config->username, config->password);
        } else if (num_substring == 3) {
            sprintf(request_1, "PUT .%s.1 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
            sprintf(request_2, "PUT .%s.2 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
            sprintf(request_3, "PUT .%s.3 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
            sprintf(request_4, "PUT .%s.4 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
        }

        // send PUT request to server 1 and 2, then read and send the first piece
        if (config->is_server_online[0]) {
            if (send(clientSocket[0], request_1, BUFSIZ, 0) == -1) {
                config->is_server_online[0] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[0];
            }
        }
        if (config->is_server_online[1]) {
            if (send(clientSocket[1], request_1, BUFSIZ, 0) == -1) {
                config->is_server_online[1] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[1];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 1 to DFS1 and/or DFS2");
        totalBytesSent += byteSent;
        socket_1 = socket_2 = -1;

        usleep(100000);

        printf("Total Bytes Sent: %zu\n", totalBytesSent);


        // send PUT request to server 2 and 3, then read and send the second piece
        if (config->is_server_online[1]) {
            if (send(clientSocket[1], request_2, BUFSIZ, 0) == -1) {
                config->is_server_online[1] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[1];
            }
        }
        if (config->is_server_online[2]) {
            if (send(clientSocket[2], request_2, BUFSIZ, 0) == -1) {
                config->is_server_online[2] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[2];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 2 to DFS2 and/or DFS3");
        totalBytesSent += byteSent;
        socket_1 = socket_2 = -1;

        usleep(100000);

        printf("Total Bytes Sent: %zu\n", totalBytesSent);

        // TODO: config piece to send -> the last piece should cover the remaining bytes, not based on piece size -> then try reconnection to each server if send request fail for all type of commands
        // send PUT request to server 3 and 4, then read and send the third piece
        if (config->is_server_online[2]) {
            if (send(clientSocket[2], request_3, BUFSIZ, 0) == -1) {
                config->is_server_online[2] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[2];
            }
        }
        if (config->is_server_online[3]) {
            if (send(clientSocket[3], request_3, BUFSIZ, 0) == -1) {
                config->is_server_online[3] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[3];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 3 to DFS3 and/or DFS4");
        totalBytesSent += byteSent;

        printf("Total Bytes Sent: %zu\n", totalBytesSent);
        socket_1 = socket_2 = -1;

        usleep(100000);

        // send PUT request to server 1 and 4, then read and send the forth piece
        // recalculate the remaining byte -> go to last piece
        num_buf_fill = (file_size - totalBytesSent) / BUFFER_SIZE;
        remaining_byte = (file_size - totalBytesSent) - (BUFFER_SIZE * num_buf_fill);

        if (config->is_server_online[0]) {
            if (send(clientSocket[0], request_4, BUFSIZ, 0) == -1) {
                config->is_server_online[0] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[0];
            }
        }
        if (config->is_server_online[3]) {
            if (send(clientSocket[3], request_4, BUFSIZ, 0) == -1) {
                config->is_server_online[3] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[3];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 4 to DFS1 and/or DFS4");
        totalBytesSent += byteSent;

        printf("Total Bytes Sent: %zu\n", totalBytesSent);
        socket_1 = socket_2 = -1;

        usleep(100000);


        // Close the file and socket
        fclose(file);
    }

    if (scheme_value == 2) {
        FILE *file = fopen(fullFilePath, "rb");
        int socket_1=-1, socket_2=-1;
        size_t totalBytesSent = 0;
        long num_buf_fill = file_size_divided / BUFFER_SIZE;
        long remaining_byte = file_size_divided - (BUFFER_SIZE * num_buf_fill);
        size_t byteSent = 0;

        // send PUT requests to servers
        if (num_substring == 2) {
            sprintf(request_1, "PUT .%s.1 %s %s", secondSubstring, config->username, config->password);
            sprintf(request_2, "PUT .%s.2 %s %s", secondSubstring, config->username, config->password);
            sprintf(request_3, "PUT .%s.3 %s %s", secondSubstring, config->username, config->password);
            sprintf(request_4, "PUT .%s.4 %s %s", secondSubstring, config->username, config->password);
        } else if (num_substring == 3) {
            sprintf(request_1, "PUT .%s.1 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
            sprintf(request_2, "PUT .%s.2 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
            sprintf(request_3, "PUT .%s.3 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
            sprintf(request_4, "PUT .%s.4 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
        }

        // send PUT request to server 2 and 3, then read and send the first piece
        if (config->is_server_online[1]) {
            if (send(clientSocket[1], request_1, BUFSIZ, 0) == -1) {
                config->is_server_online[1] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[1];
            }
        }
        if (config->is_server_online[2]) {
            if (send(clientSocket[2], request_1, BUFSIZ, 0) == -1) {
                config->is_server_online[2] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[2];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 1 to DFS2 and/or DFS3");
        totalBytesSent += byteSent;
        socket_1 = socket_2 = -1;

        usleep(100000);

        printf("Total Bytes Sent: %zu\n", totalBytesSent);


        // send PUT request to server 3 and 4, then read and send the second piece
        if (config->is_server_online[2]) {
            if (send(clientSocket[2], request_2, BUFSIZ, 0) == -1) {
                config->is_server_online[2] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[2];
            }
        }
        if (config->is_server_online[3]) {
            if (send(clientSocket[3], request_2, BUFSIZ, 0) == -1) {
                config->is_server_online[3] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[3];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 2 to DFS3 and/or DFS4");
        totalBytesSent += byteSent;
        socket_1 = socket_2 = -1;

        usleep(100000);

        printf("Total Bytes Sent: %zu\n", totalBytesSent);

        // TODO: config piece to send -> the last piece should cover the remaining bytes, not based on piece size -> then try reconnection to each server if send request fail for all type of commands
        // send PUT request to server 1 and 4, then read and send the third piece
        if (config->is_server_online[0]) {
            if (send(clientSocket[0], request_3, BUFSIZ, 0) == -1) {
                config->is_server_online[0] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[0];
            }
        }
        if (config->is_server_online[3]) {
            if (send(clientSocket[3], request_3, BUFSIZ, 0) == -1) {
                config->is_server_online[3] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[3];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 3 to DFS1 and/or DFS4");
        totalBytesSent += byteSent;

        printf("Total Bytes Sent: %zu\n", totalBytesSent);
        socket_1 = socket_2 = -1;

        usleep(100000);

        // send PUT request to server 1 and 2, then read and send the forth piece
        // recalculate the remaining byte -> go to last piece
        num_buf_fill = (file_size - totalBytesSent) / BUFFER_SIZE;
        remaining_byte = (file_size - totalBytesSent) - (BUFFER_SIZE * num_buf_fill);

        if (config->is_server_online[0]) {
            if (send(clientSocket[0], request_4, BUFSIZ, 0) == -1) {
                config->is_server_online[0] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[0];
            }
        }
        if (config->is_server_online[1]) {
            if (send(clientSocket[1], request_4, BUFSIZ, 0) == -1) {
                config->is_server_online[1] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[1];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 4 to DFS1 and/or DFS2");
        totalBytesSent += byteSent;

        printf("Total Bytes Sent: %zu\n", totalBytesSent);
        socket_1 = socket_2 = -1;

        usleep(100000);


        // Close the file and socket
        fclose(file);
    }

    if (scheme_value == 3) {
        FILE *file = fopen(fullFilePath, "rb");
        int socket_1=-1, socket_2=-1;
        size_t totalBytesSent = 0;
        long num_buf_fill = file_size_divided / BUFFER_SIZE;
        long remaining_byte = file_size_divided - (BUFFER_SIZE * num_buf_fill);
        size_t byteSent = 0;

        // send PUT requests to servers
        if (num_substring == 2) {
            sprintf(request_1, "PUT .%s.1 %s %s", secondSubstring, config->username, config->password);
            sprintf(request_2, "PUT .%s.2 %s %s", secondSubstring, config->username, config->password);
            sprintf(request_3, "PUT .%s.3 %s %s", secondSubstring, config->username, config->password);
            sprintf(request_4, "PUT .%s.4 %s %s", secondSubstring, config->username, config->password);
        } else if (num_substring == 3) {
            sprintf(request_1, "PUT .%s.1 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
            sprintf(request_2, "PUT .%s.2 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
            sprintf(request_3, "PUT .%s.3 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
            sprintf(request_4, "PUT .%s.4 %s %s %s", secondSubstring, thirdSubstring, config->username,
                    config->password);
        }

        // send PUT request to server 3 and 4, then read and send the first piece
        if (config->is_server_online[2]) {
            if (write(clientSocket[2], request_1, BUFSIZ) == -1) {
                config->is_server_online[2] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[2];
            }
        }
        if (config->is_server_online[3]) {
            if (write(clientSocket[3], request_1, BUFSIZ) == -1) {
                config->is_server_online[3] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[3];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 1 to DFS3 and/or DFS4");
        totalBytesSent += byteSent;
        socket_1 = socket_2 = -1;

        usleep(100000);

        printf("Total Bytes Sent: %zu\n", totalBytesSent);


        // send PUT request to server 1 and 4, then read and send the second piece
        if (config->is_server_online[0]) {
            if (write(clientSocket[0], request_2, BUFSIZ) == -1) {
                config->is_server_online[0] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[0];
            }
        }
        if (config->is_server_online[3]) {
            if (write(clientSocket[3], request_2, BUFSIZ) == -1) {
                config->is_server_online[3] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[3];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 2 to DFS1 and/or DFS4");
        totalBytesSent += byteSent;
        socket_1 = socket_2 = -1;

        usleep(100000);

        printf("Total Bytes Sent: %zu\n", totalBytesSent);

        // TODO: config piece to send -> the last piece should cover the remaining bytes, not based on piece size -> then try reconnection to each server if send request fail for all type of commands
        // send PUT request to server 1 and 2, then read and send the third piece
        if (config->is_server_online[0]) {
            if (write(clientSocket[0], request_3, BUFSIZ) == -1) {
                config->is_server_online[0] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[0];
            }
        }
        if (config->is_server_online[1]) {
            if (write(clientSocket[1], request_3, BUFSIZ) == -1) {
                config->is_server_online[1] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[1];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 3 to DFS1 and/or DFS2");
        totalBytesSent += byteSent;

        printf("Total Bytes Sent: %zu\n", totalBytesSent);
        socket_1 = socket_2 = -1;

        usleep(100000);

        // send PUT request to server 2 and 3, then read and send the forth piece
        // recalculate the remaining byte -> go to last piece
        num_buf_fill = (file_size - totalBytesSent) / BUFFER_SIZE;
        remaining_byte = (file_size - totalBytesSent) - (BUFFER_SIZE * num_buf_fill);

        if (config->is_server_online[1]) {
            if (write(clientSocket[1], request_4, BUFSIZ) == -1) {
                config->is_server_online[1] = false;
                socket_1 = -1;
            }
            else{
                socket_1 = clientSocket[1];
            }
        }
        if (config->is_server_online[2]) {
            if (write(clientSocket[2], request_4, BUFSIZ) == -1) {
                config->is_server_online[2] = false;
                socket_2 = -1;
            }
            else{
                socket_2 = clientSocket[2];
            }
        }

        usleep(100000);

        byteSent = send_each_piece(socket_1, socket_2, num_buf_fill, remaining_byte, config, file, "piece 4 to DFS2 and/or DFS3");
        totalBytesSent += byteSent;

        printf("Total Bytes Sent: %zu\n", totalBytesSent);
        socket_1 = socket_2 = -1;

        usleep(100000);


        // Close the file and socket
        fclose(file);
    }
}

int retrieve_file_piece(ConfigData *config, char* request, char* filepath, FILE* receive_file, char* message){
    size_t bytesRecv = 0;
    size_t totalByteRecv = 0;
    char* receive_buffer = (char*)malloc(BUFSIZ);
    bzero(receive_buffer, BUFSIZ);

    for (int i=0; i < 4; i++){
        if(config->is_server_online[i]){
            send(clientSocket[i], request, BUFSIZ, 0);
            //usleep(100000);
            bytesRecv = recv(clientSocket[i], receive_buffer, BUFSIZ, 0);
            char *OK = strstr(receive_buffer, "OK\t\n\n");
            // server responses with OK message piece found on this server -> sending piece now
            if(OK){
                bzero(receive_buffer, BUFSIZ);
                while (true){
                    bytesRecv = recv(clientSocket[i], receive_buffer, BUFSIZ, 0);
                    char *done_position = strstr(receive_buffer, "DONE\t\n\n");
                    if (done_position){
                        bzero(receive_buffer, BUFSIZ);
                        printf("Received DONE message from server \n");
                        free(receive_buffer);
                        break;
                    }
                    else{
                        printf("[Before decryption] Data received %lu bytes: %s\n", bytesRecv, receive_buffer);
                        //decrypt receive_buffer with password before writing to file
                        XOR_EncryptDecrypt(receive_buffer, bytesRecv, config->password);
                        printf("[After decryption] Data received %lu bytes: %s\n", bytesRecv, receive_buffer);
                        fwrite(receive_buffer, bytesRecv, 1, receive_file);
                        bzero(receive_buffer, BUFSIZ);
                    }
                    totalByteRecv += bytesRecv;
                }
                printf("Piece %s received and written to %s \n", message, filepath);
                return 1;
                break;
            }

        }
    }
    free(receive_buffer);
    return -1;
}


int main(int argc, char *argv[]) {
    size_t path_size = pathconf(".", _PC_PATH_MAX);
    PATH = (char *)malloc((size_t)path_size);
    getcwd(PATH, path_size);
    printf("Current working directory: %s \n", PATH);
    
    if (argc < 2) {
        printf("Usage: ./dfc <config_file>\n", argv[0]);
        return 1;
    }

    // handle ctl + c
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sigint_handler;
    sigaction(SIGINT, &action, &old_action);

    char *filename = argv[1];
    char *userInput = (char*)malloc(BUFSIZ);
    bzero(userInput, BUFSIZ);
    ConfigData config;

    // Initialize the config struct
    memset(&config, 0, sizeof(ConfigData));

    extractConfigData(filename, &config);

    // Print the extracted information
    for (int i = 0; i < 4; ++i) {
        // Split DFS address and port
        char dfsAddress[MAX_LINE_LENGTH];
        int dfsPort;
        sscanf(config.dfsAddresses[i], "%[^:]:%d", dfsAddress, &dfsPort);

        printf("DFS%d_address = %s\n", i + 1, dfsAddress);
        printf("DFS%d_port = %d\n", i + 1, dfsPort);
    }
    printf("username = %s\n", config.username);
    printf("password = %s\n", config.password);




    //char buffer[BUFFER_SIZE];
    char *buffer = (char *)malloc(BUFFER_SIZE);
    char bufferArray[4][BUFFER_SIZE];
    int bytesRead;

    // try to connect to servers in 1 second
    int num_online_server = 0;
    for (int i = 0; i < 4; ++i) {
        char dfsAddress[MAX_LINE_LENGTH];
        int dfsPort;
        sscanf(config.dfsAddresses[i], "%[^:]:%d", dfsAddress, &dfsPort);

        // Create socket
        if ((clientSocket[i] = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        // Configure server address
        DFS[i].sin_family = AF_INET;
        DFS[i].sin_addr.s_addr = inet_addr(dfsAddress);
        DFS[i].sin_port = htons(dfsPort);

        // Connect to the server
        if (connect(clientSocket[i], (struct sockaddr*)&DFS[i], sizeof(DFS[i])) == -1) {
            printf("Server offline %s:%d\n", dfsAddress,dfsPort);
            config.is_server_online[i] = false;
        }
        else{
            printf("Connected to server at %s:%d\n", dfsAddress, dfsPort);
            config.is_server_online[i] = true;
            num_online_server++;
        }
    }
    sleep(1);
    if (num_online_server < 4){
        printf("Retry connecting to servers \n");
        retry_connect(&config);
    }

    while (1) {
        bzero(buffer, BUFSIZ);
        bzero(userInput, BUFSIZ);
        buffer[0] = '\0';
        userInput[0] = '\0';

        printf("Enter command: ");

        // Read input as a string
        fgets(userInput, BUFSIZ, stdin);

        // Remove the newline character if present
        size_t length = strlen(userInput);
        if (length > 0 && userInput[length - 1] == '\n') {
            userInput[length - 1] = '\0';
        }

        // Check for the exit condition
        if (strcmp(userInput, "exit") == 0) {
            printf("Exiting the program.\n");
            break;
        }

        if (strstr(userInput, "LIST") != NULL){
            check_online_servers(&config);
            retry_connect(&config);
            strcat(userInput, " ");
            strcat(userInput, config.username);
            strcat(userInput, " ");
            strcat(userInput, config.password);
            for (int i = 0; i < 4; i++){
                if (config.is_server_online[i]){
                    if (send(clientSocket[i], userInput, BUFSIZ, 0) == -1){
                        config.is_server_online[i] = false;
                    }
                }
                else{
                    if(connect(clientSocket[i], (struct sockaddr*)&DFS[i], sizeof(DFS[i])) == -1){
                        config.is_server_online[i] = false;
                    }
                    else{
                        config.is_server_online[i] = true;
                        send(clientSocket[i], userInput, BUFSIZ, 0);
                    }
                }
            }
            for (int i = 0; i < 4; i++){
                bytesRead = 0;
                bzero(buffer, BUFFER_SIZE);
                buffer[0] = '\0';
                bzero(bufferArray[i], BUFFER_SIZE);
                bufferArray[i][0] = '\0';
                if(config.is_server_online[i]){
                    bytesRead = recv(clientSocket[i], buffer, BUFFER_SIZE, 0);
                    if(bytesRead == 0){
                        config.is_server_online[i] = false;
                        printf("Server is offline: %s\n", config.dfsAddresses[i]);
                    }
                    else{
                        printf("Response from server: %s\n", buffer);
                        strcpy(bufferArray[i], buffer);
                    }
                }
            }
            char* dir = extractDir(bufferArray, 4);
            //printf("%s \n", dir);
            char* file = extractFile(bufferArray, 4);
            printf("%s \n", file);
            char* after_checked_pieces = check_file_pieces(file);
            printf("%s \n", after_checked_pieces);

            free(dir);
            free(file);
            free(after_checked_pieces);
        }
        else if (strstr(userInput, "MKDIR") != NULL){
            strcat(userInput, " ");
            strcat(userInput, config.username);
            strcat(userInput, " ");
            strcat(userInput, config.password);
            for (int i = 0; i < 4; i++){
                if(config.is_server_online[i]){
                    //send(clientSocket[i], userInput, strlen(userInput), 0);
                    send(clientSocket[i], userInput, BUFSIZ, 0);
                }
            }
            for (int i = 0; i < 4; i++){
                bytesRead = 0;
                bzero(buffer, BUFFER_SIZE);
                buffer[0] = '\0';
                if(config.is_server_online[i]){
                    bytesRead = recv(clientSocket[i], buffer, BUFFER_SIZE, 0);
                    if(bytesRead == 0){
                        config.is_server_online[i] = false;
                        printf("Server is offline: %s\n", config.dfsAddresses[i]);
                    }
                    else{
                        printf("Response from server: %s\n", buffer);
                    }
                }
            }
        }
        else if (strstr(userInput, "PUT") != NULL){
            // hash the file name to decide which server gets which file
            long scheme_value = hash(userInput) %4;
            printf("Scheme value: %lu\n", scheme_value);
            char *fullFilePath = (char*)malloc(BUFFER_SIZE);
            char firstSubstring[256]; // PUT
            char secondSubstring[256]; // filename
            char thirdSubstring[256]; // subfolder name: optional
            // Use sscanf to extract substrings
            int num_substring = sscanf(userInput, "%s %s %s", firstSubstring, secondSubstring, thirdSubstring);
            long file_size = get_file_size(secondSubstring, fullFilePath); // -1 if file doesn't exist
            printf("File size: %lu\n", file_size);

            if (file_size > 0){
                send_file_pieces(scheme_value, file_size, fullFilePath, secondSubstring, thirdSubstring, num_substring, &config);
            }
            free (fullFilePath);
        }
        else if (strstr(userInput, "GET") != NULL){
            //TODO: check number of online server, < 3 => there won't be enough file pieces => return cannot download file
            // Otherwise, parse request and send LIST request to servers -> get back the file pieces list
            // Go from piece 1 to 4, send GET request to each server to retrieve the pieces -> writting them into output file along the way

            //check which server is offline and retry to connect to those offline
            check_online_servers(&config);
            retry_connect(&config);

            //count number of online servers -> smaller than 3 -> cannot have complete file pieces
            int num_online_servers = 0;
            for(int i = 0; i < 4 ; i++){
                if (config.is_server_online[i] == true){
                    num_online_servers++;
                }
            }

            if (num_online_servers < 3){
                printf("There is currently less than 3 servers being online, cannot retrieve all file pieces. Try again later... \n");
            }
            // parse PUT request -> get file path -> loop through all online servers and request piece 1 to 4
            // -> if any piece cannot retrieve after trying all online server -> printf cannot retrieve
            // Otherwise receive all pieces and write to same output file
            else{

                // parse the PUT request, with and without subfolder
                char firstSubstring[256];
                char secondSubstring[256];
                char thirdSubstring[256];

                // Use sscanf to extract substrings
                int num_substring = sscanf(userInput, "%s %s %s", firstSubstring, secondSubstring, thirdSubstring);
                char request_1[100];
                char request_2[100];
                char request_3[100];
                char request_4[100];

                if (num_substring == 2) {
                    sprintf(request_1, "GET .%s.1 %s %s", secondSubstring, config.username, config.password);
                    sprintf(request_2, "GET .%s.2 %s %s", secondSubstring, config.username, config.password);
                    sprintf(request_3, "GET .%s.3 %s %s", secondSubstring, config.username, config.password);
                    sprintf(request_4, "GET .%s.4 %s %s", secondSubstring, config.username, config.password);
                }
                else if (num_substring == 3) {
                    sprintf(request_1, "GET .%s.1 %s %s %s", secondSubstring, thirdSubstring, config.username, config.password);
                    sprintf(request_2, "GET .%s.2 %s %s %s", secondSubstring, thirdSubstring, config.username, config.password);
                    sprintf(request_3, "GET .%s.3 %s %s %s", secondSubstring, thirdSubstring, config.username, config.password);
                    sprintf(request_4, "GET .%s.4 %s %s %s", secondSubstring, thirdSubstring, config.username, config.password);
                }
                char* filepath = (char*) malloc(MAX_LINE_LENGTH);
                bzero(filepath, MAX_LINE_LENGTH);
                strcpy(filepath, PATH);
                strcat(filepath, "/");
                strcat(filepath, secondSubstring);

                FILE *receive_file = fopen(filepath, "wb");
                printf("File path: %s \n", filepath);

                // trying to retrieve the first piece -> sending first piece retrieve request to all online servers

                int success = retrieve_file_piece(&config, request_1, filepath, receive_file, "1");
                if (success == 1){
                    success = retrieve_file_piece(&config, request_2, filepath, receive_file, "2");
                    if (success == 1){
                        success = retrieve_file_piece(&config, request_3, filepath, receive_file, "3");
                        if (success == 1){
                            success = retrieve_file_piece(&config, request_4, filepath, receive_file, "4");
                            if (success != 1){
                                printf("Fail to retrieve piece 4 \n");
                            }
                        }
                        else{
                            printf("Fail to retrieve piece 3 \n");
                        }
                    }
                    else{
                        printf("Fail to retrieve piece 2 \n");
                    }
                }
                else{
                    printf("Fail to retrieve piece 1 \n");
                }

                if (success == 1){
                    printf("File retrieved successfully: %s \n", filepath);
                }
                else{
                    printf("Fail to retrieve file: %s \n", filepath);
                }

                fclose(receive_file);
                free(filepath);
            }
        }
        else{
            printf("Unsupported Command, try again...\n");
        }
    }

    printf("\nExit command entered, sending terminating message to servers...\n");

    for (int i = 0; i < 4; i++){
        send(clientSocket[i], terminate_message, strlen(terminate_message), 0);
        close(clientSocket[i]);
    }

    free(buffer);
    free(userInput);

    return 0;
}
