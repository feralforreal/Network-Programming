/*
 * udpserver.c - A simple UDP echo server
 * usage: udpserver <port>
 */

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

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(1);
}

char *extract_filename (char *buf){
    //allocate using malloc to avoid overriding by other function
    //https://stackoverflow.com/questions/26613235/printf-changes-the-value
    //char *file_name = malloc(100 * sizeof(char));
    char *file_name = (char*)malloc(50);
    for(int i = 0; i < strlen(buf); i=i+1){ // go through the received buffer one by one
        if (buf[i] == ' '){ // file name is after the white space
            i = i + 1; // skip the white space
            int j = 0; // counter of file_name buffer
            while (i < strlen(buf) - 1){ // copy the file name from received buffer to file_name buffer, exclude \n -> remove(file_name), file_name doesn't have \n
                file_name[j] = buf[i];
                i = i + 1;
                j = j + 1;
            }
            file_name[j] = '\0'; //add null pointer to indicate end of string
            break; // exit the for loop after extracting file name
        }
    }
    return file_name;
}

int retry(int sockfd, char *out_buf, char *in_buf,struct sockaddr_in clientaddr, int clientlen, int out_buf_len){
    int n = 0;
    int is_responsed = 0;
    for (int i=0; i<1000; i=i+1){
        printf("Retry sending to client...\n");
        if (strlen(out_buf) != out_buf_len) printf("Outbound buffer inside retry: %s \n", out_buf + sizeof(int) );
        else printf("Outbound buffer inside retry: %s \n", out_buf);
        printf("Number of byte sending: %d \n", out_buf_len);
        n = sendto(sockfd, out_buf, out_buf_len, 0, &clientaddr, clientlen);
        if (n < 0) error("ERROR in sendto");
        n = recvfrom(sockfd, in_buf, BUFSIZE, 0, &clientaddr, &clientlen);
        if (n < 0){
            printf("No response from client.\n");
        }
        else{
            is_responsed = 1;
            break;
        }
    }
    return is_responsed;
}

/*
 * source: https://www.tutorialspoint.com/how-can-i-get-the-list-of-files-in-a-directory-using-c-or-cplusplus#:~:text=Call%20opendir()%20function%20to,name%20using%20en%2D%3Ed_name.
 */
char* get_file_list (){
    //printf("file_list before get_file_list: %s \n", file_list);
    char *file_list = (char*)malloc(200);
    DIR *dr;
    struct dirent *entry;
    dr = opendir("."); //open current working directory
    if (dr) {
        while ((entry = readdir(dr)) != NULL) { // while not end of file list
            if (entry->d_type == DT_REG) { //make sure entry is the regular file
                strcat(file_list, entry->d_name); // add file name into file list
                strcat(file_list, "|"); // seperate by |
                //printf("file_list during get_file_list: %s \n", file_list);
            }
        }
        closedir(dr); //close directory
    }
    strcat(file_list, "\n"); // add \n at the end, to mark the end of string
    return file_list;
}

void test_read(){
    FILE* filePointer;
    char buffer[100];
    filePointer = fopen("test.txt", "r");
    while (!feof(filePointer)) {
        fread(buffer, sizeof(buffer), 1, filePointer);
        printf("%s", buffer);
    }
    fclose(filePointer);
}


int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp; /* client host info */
    char buf[BUFSIZE]; /* message buf */
    //char *buf = (char *) malloc(BUFSIZE);
    char *hostaddrp; /* dotted decimal host addr string */
    int optval; /* flag value for setsockopt */
    int n; /* message byte size */

    /*
     * check command line arguments
     */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    /*
     * socket: create the parent socket
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets
     * us rerun the server immediately after we kill it;
     * otherwise we have to wait about 20 secs.
     * Eliminates "ERROR on binding: Address already in use" error.
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /*
     * bind: associate the parent socket with a port
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr,
             sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    /*
     * main loop: wait for a datagram, then echo it
     */
    clientlen = sizeof(clientaddr);

    // to enable and disable timeout, I created 2 timeval instances
    // disable timeout in the main loop, enable timeout in file transferring code
    struct timeval en_timeout;
    //en_timeout.tv_sec = 10;
    en_timeout.tv_usec = 100000;

    struct timeval dis_timeout;
    dis_timeout.tv_sec = 0;
    dis_timeout.tv_usec = 0;

    while (1) {

        /*
         * recvfrom: receive a UDP datagram from a client
         */
        bzero(buf, BUFSIZE);
        buf[0] = '\0';
        n = recvfrom(sockfd, buf, BUFSIZE, 0,
                     (struct sockaddr *) &clientaddr, &clientlen);
        if (n < 0)
            error("ERROR in recvfrom");

        /*
         * gethostbyaddr: determine who sent the datagram
         */
	
        hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	
	if (hostp == NULL)
            error("ERROR on gethostbyaddr");
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if (hostaddrp == NULL)
            error("ERROR on inet_ntoa\n");
        printf("server received datagram from %s (%s)\n",
               hostp->h_name, hostaddrp);
        printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);

        // if the received message is exit\n => terminate the server
        if (strcmp(buf, "exit\n") == 0){ // strcmp returns 0 if the 2 strings are equal
            char exit_response[BUFSIZE] = "Server is exiting...\n"; // send back exit response to the client
            n = sendto(sockfd, exit_response, strlen(exit_response), 0, (struct sockaddr *) &clientaddr, clientlen);
            if (n < 0) error("ERROR in sendto");
            printf("Exit command received, terminating server...\n");
            break; // exit the main while loop
        }
            /*
             * get file list in current directory and send the file list back to the client
             */
        else if (strcmp(buf, "ls\n") == 0){
            char *file_list = get_file_list();
	        // send back the file list to client
            n = sendto(sockfd, file_list, strlen(file_list), 0, (struct sockaddr *) &clientaddr, clientlen);
	        if (n < 0) error("ERROR in sendto");
            free(file_list);
        }
        else if (strstr(buf, "delete") != NULL){ // detect delete substring in received buffer
            // extract the file name from buf
            char *file_name = extract_filename(buf);
            char response_message[BUFSIZE] = "";
            if (remove(file_name) == 0){ // construct the success response message
                strcat(response_message, "File deleted successfully: ");
                strcat(response_message, file_name);
                strcat(response_message, "\n");
            }
            else{ // send back the failed response to indicate the delete status
                strcat(response_message, "Failed to delete file: ");
                strcat(response_message, file_name);
                strcat(response_message, "\n");
            }
            //send back the response message
            n = sendto(sockfd, response_message, strlen(response_message), 0, (struct sockaddr *) &clientaddr, clientlen);
            if (n < 0) error("ERROR in sendto");
            free(file_name);
        }
        else if (strstr(buf, "get") != NULL){
            int seq_num_server = 0;
            int seq_num_client = 0;
            int is_responsed = 1; //flag that indicate whether client response either instantly or during timeout retries
            int got_correct_ack = 1; //flag indicating got the wrong/correct ack, go on and read the next bytes/resend the previous bytes

            char *outbound_buffer = (char*)malloc(BUFSIZE);
            char *inbound_buffer = (char*)malloc(BUFSIZE);
            char *content_buffer = (char*)malloc(BUFSIZE);
            bzero(outbound_buffer, BUFSIZE);
            bzero(inbound_buffer, BUFSIZE);
            bzero(content_buffer, BUFSIZE);

            char *file_list = get_file_list();
            //extract file name from client's command
            char *file_name = extract_filename(buf);
            printf("File to transfer: %s\n", file_name);
            FILE* fd = fopen(file_name, "rb"); //open file to read
            //if (strstr(file_list, file_name) != NULL) { //make sure the file can be opened -> send OK on success or FAIL response to client
            if (fd != NULL) {
                //enable timeout for file transferring
                if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&en_timeout,sizeof(en_timeout)) < 0) {
                    perror("Error");
                }
                //send OK\n to client, indicating requested file opened successfully
                strcat(outbound_buffer, "OK\n");
                printf("Outbound_buffer: %s \n", outbound_buffer);
                n = sendto(sockfd, outbound_buffer, strlen(outbound_buffer), 0, (struct sockaddr *) &clientaddr, clientlen);
                if (n < 0) error("ERROR in sendto");
                //waiting for OK\n response from client -> retransmit OK\n if there is no response from client
                n = recvfrom(sockfd, inbound_buffer, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
                if (n<0){
                    is_responsed = retry(sockfd, outbound_buffer, inbound_buffer, clientaddr, clientlen, strlen(outbound_buffer));
                }
                //if receive OK\n from user -> begin file content transferring
                //if(is_responsed && strcmp(inbound_buffer, "OK\n") == 0) {
                if (is_responsed && strcmp(inbound_buffer, "OK\n") == 0){
		        size_t bytesRead = 10000; //number of bytes read from file
                    while (bytesRead > 0){ //if not EOF, keep reading
                        bzero(inbound_buffer, BUFSIZE);
                        //only move on to read the next bytes if expected ack was received, otherwise, keep sending the previous file content
                        if(got_correct_ack){
                            bzero(outbound_buffer, BUFSIZE);
                            bzero(content_buffer, BUFSIZE);
                            //add seq number in the first 4 bytes and data in the rest 1020 bytes of outbound_buffer
                            bytesRead = fread(content_buffer, sizeof(char), BUFSIZE - sizeof(int) - sizeof(size_t), fd);
			                printf("Bytes read: %d\n", bytesRead);
                            memcpy(outbound_buffer, &seq_num_server, sizeof(int));
			                memcpy(outbound_buffer + sizeof(int), &bytesRead, sizeof(size_t));
                            memcpy(outbound_buffer + sizeof(int) + sizeof(size_t), content_buffer, bytesRead);
                        }
                        printf("Outbound_buffer before first send: %s \n", outbound_buffer);
                        n = sendto(sockfd, outbound_buffer, sizeof(int) + sizeof(size_t) + bytesRead, 0, (struct sockaddr *) &clientaddr, clientlen);
                        if (n < 0) error("ERROR in sendto");

                        //wait for sequence number from user -> if timeout, retry sending data
                        n = recvfrom(sockfd, inbound_buffer, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
                        if (n<0){
                            printf("Outbound_buffer before retry: %s \n", outbound_buffer + sizeof(int));
                            is_responsed = retry(sockfd, outbound_buffer, inbound_buffer, clientaddr, clientlen, sizeof(int) + sizeof(size_t) + bytesRead);
                        }
                        if (is_responsed){
                            memcpy(&seq_num_client, outbound_buffer, sizeof(int));
                            if(seq_num_client == seq_num_server){
                                printf("Sequence number match: %d\n", seq_num_client);
                                seq_num_server = seq_num_server + 1; //if receive expected ack from client
                                got_correct_ack = 1; //flag indicating got the correct ack, go on and read the next bytes
                            }
                            else{
                                printf("Sequence number mismatch: server %d; client %d \n", seq_num_server, seq_num_client);
                                got_correct_ack = 0;//indicating wrong ack received, resend the previous bytes
                            }
                        }
                        else{
                            printf("Retried 3 times but no response from client, exiting file transferring...\n");
                            break;
                        }


                    }
                    //send DONE\n to the client, indicating that the server has finished sending all bytes of the requested file
                    bzero(outbound_buffer, BUFSIZE);
                    bzero(inbound_buffer, BUFSIZE);

                    //strcat(outbound_buffer, "DONE\n");
                    //n = sendto(sockfd, outbound_buffer, strlen(outbound_buffer), 0, (struct sockaddr *) &clientaddr, clientlen);
                    //if (n < 0) error("ERROR in sendto");
                    fclose(fd); //close file

                    //wait for DONE\n from client, retry if needed
                    //n = recvfrom(sockfd, inbound_buffer, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
                    //if (n<0){
                    //    printf("Outbound_buffer before retry: %s \n", outbound_buffer + sizeof(int));
                    //    is_responsed = retry(sockfd, outbound_buffer, inbound_buffer, clientaddr, clientlen, sizeof(int) + strlen(content_buffer));
                    //}
                    //printf("Got DONE from client \n");
                }
                //if client doesn't response to OK\n -> terminate the file transferring, wait for further commands
                else{
                    printf("Retried 3 times but no response from client, exiting file transferring...\n");
                }
            }
            else{
                n = sendto(sockfd, "FAIL\n", strlen("FAIL\n"), 0, (struct sockaddr *) &clientaddr, clientlen);
                if (n < 0) error("ERROR in sendto");
            }
            //free all allocated buffers
            free(inbound_buffer);
            free(outbound_buffer);
            free(content_buffer);
            free(file_name);
            free(file_list);

            //disable timeout before going back to main loop (wait infinitely for client command)
            if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&dis_timeout,sizeof(dis_timeout)) < 0) {
                perror("Error");
            }
            printf("DONE sending file to client \n");
        }
        else if (strstr(buf, "put") != NULL) {
            if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&en_timeout,sizeof(en_timeout)) < 0) {
                perror("Error");
            }

            char *file_name = extract_filename(buf); //get the file name from the user's command
            printf("Receiving file from client: %s \n", file_name);
            //------------------------------------------------------------
            char *outbound_buffer = (char *) malloc(BUFSIZE);
            char *inbound_buffer = (char *) malloc(BUFSIZE);
            char *content_buffer = (char *) malloc(BUFSIZE);

            bzero(outbound_buffer, BUFSIZE);
            bzero(inbound_buffer, BUFSIZE);
            bzero(content_buffer, BUFSIZE);

            int is_responsed = 1;
            //send back OK to client and begin file transfering
            strcat(outbound_buffer, "OK\n");
            n = sendto(sockfd, outbound_buffer, strlen(outbound_buffer), 0, (struct sockaddr *) &clientaddr, clientlen);
            if (n < 0) error("ERROR in sendto");


            int seq_num_client = 0;
            int seq_num_server = 0;
            size_t payload_bytes = 0;


            FILE *output_file;
            output_file = fopen(file_name, "w+");
            while (1) {
                //clear all buffers
                bzero(inbound_buffer, BUFSIZE);
                bzero(content_buffer, BUFSIZE);
                //receive the message from server, if timeout, retry sending seq number n times
                n = recvfrom(sockfd, inbound_buffer, BUFSIZE, 0, &clientaddr, &clientlen);
                if (n < 0) {
                    is_responsed = retry(sockfd, outbound_buffer, inbound_buffer, clientaddr, clientlen,
                                         strlen(outbound_buffer));
                }
                //extract the sequence number from the first 4 bytes of inbound buffer, 1020 bytes are for data
                memcpy(&seq_num_server, inbound_buffer, sizeof(int));
                memcpy(&payload_bytes, inbound_buffer + sizeof(int), sizeof(size_t));
                memcpy(content_buffer, inbound_buffer + sizeof(int)+ sizeof(size_t), payload_bytes);
                printf("Sequence number received: %d\n", seq_num_server);
                printf("Number of bytes received: %d\n", payload_bytes);

                //if data is DONE\n (meaning file transferring is finished) -> close output file and break the while loop
                if (payload_bytes == 0) {
                    fclose(output_file);
                    printf("File downloading is DONE: %s \n", file_name);
                    //echo back DONE to client
                    bzero(outbound_buffer, BUFSIZE);
                    bzero(outbound_buffer, BUFSIZE);
                    memcpy(outbound_buffer, &seq_num_client, sizeof(int));
                    n = sendto(sockfd, outbound_buffer, sizeof(int), 0, &clientaddr, clientlen); //send 4 bytes of sequence number to server
                    if (n < 0) error("ERROR in sendto");
                    break;
                    
                } else if (is_responsed) { // otherwise write the data to file and send ack, if expected seq number received
                    if (seq_num_server == seq_num_client) {//write to file if expected seq number received
                        fwrite(content_buffer, sizeof(char), payload_bytes, output_file);
                        printf("Sequence number match: %d\n", seq_num_server);
                        printf("Received file content: %s \n", content_buffer);
                        // send ack containing seq num to server then clear outbound buffer
                        bzero(outbound_buffer, BUFSIZE);
                        //printf("Content buffer string length: %d \n", strlen(content_buffer));
                        memcpy(outbound_buffer, &seq_num_client, sizeof(int));
                        n = sendto(sockfd, outbound_buffer, sizeof(int), 0, &clientaddr, clientlen); //send 4 bytes of sequence number to server
                        if (n < 0) error("ERROR in sendto");
                        seq_num_client = seq_num_client + 1;
                    } else {//seq number mismatch, simply discard the received message
                        printf("Sequence number mismatch: server %d; client %d \n", seq_num_server, seq_num_client);
                        //resend previous ACK
                        int prev_seq_num_client = seq_num_client - 1;
                        bzero(outbound_buffer, BUFSIZE);
                        memcpy(outbound_buffer, &prev_seq_num_client, sizeof(int));
                        n = sendto(sockfd, outbound_buffer, sizeof(int), 0, &clientaddr,
                                   clientlen); //send 4 bytes of sequence number to server
                        if (n < 0) error("ERROR in sendto");
                    }

                }
            }
            free(inbound_buffer);
            free(outbound_buffer);
            free(content_buffer);
            free(file_name);

            //disable timeout before going back to main loop (wait infinitely for client command)
            if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&dis_timeout,sizeof(dis_timeout)) < 0) {
                perror("Error");
            }



            //------------------------------------------------------------


        }
        else{ // if command is not recognize -> send the command back with unknown message.
            char unknown_respose[BUFSIZE] = "Unknown command: ";
            strcat(unknown_respose, buf);
            n = sendto(sockfd, unknown_respose, strlen(unknown_respose), 0, (struct sockaddr *) &clientaddr, clientlen);
            if (n < 0) error("ERROR in sendto");
        }

    }

    return 0;
}
