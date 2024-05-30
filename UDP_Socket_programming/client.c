/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
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
    exit(0);
}

char *extract_filename (char *buf){
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

int retry(int sockfd, char *out_buf, char *in_buf,struct sockaddr_in serveraddr, int serverlen, int out_buf_len){
    int n = 0;
    int is_responsed = 0;
    for (int i=0; i<100; i=i+1){
        printf("Retry sending to server...\n");
        printf("Outbound buffer inside retry: %s \n", out_buf);
        n = sendto(sockfd, out_buf, out_buf_len, 0, &serveraddr, serverlen);
        if (n < 0) error("ERROR in sendto");
        n = recvfrom(sockfd, in_buf, BUFSIZE, 0, &serveraddr, &serverlen);
        if (n < 0){
            printf("No response from server.\n");
        }
        else{
            is_responsed = 1;
            break;
        }
    }

    return is_responsed;
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    //char *buf = (char *) malloc(BUFSIZE);

    /* check command line arguments */
    if (argc != 3) {
        fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    //set timeout for the receiving socket
    struct timeval timeout;
    //timeout.tv_sec = 10;
    timeout.tv_usec = 100000; //1ms for now -> will update according to RTT later
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&timeout,sizeof(timeout)) < 0) {
        perror("Error");
    }

    while (1){
        /* get a message from the user */
        bzero(buf, BUFSIZE);
	    buf[0] = '\0'; //make sure the first character in the damn buffer is a null character, bzero is stupid (there may some random leftover char in there).
        printf("Please enter msg: ");
        fgets(buf, BUFSIZE, stdin);

        //if command is put, let check if the file exist on client -> if no, then discard the command all together
        if (strstr(buf, "put") != NULL){

            char *file_name = extract_filename(buf); //get the file name from the user's command
            FILE* fd = fopen(file_name, "r");
            if (fd != NULL){
                printf("Uploading file to server: %s \n", file_name);
                n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
                if (n < 0) error("ERROR in sendto");
                //---------------------------------------
                int seq_num_server = 0;
                int seq_num_client = 0;
                int is_responsed = 1; //flag that indicate whether client response either instantly or during timeout retries
                int got_correct_ack = 1;

                char *outbound_buffer = (char*)malloc(BUFSIZE);
                char *inbound_buffer = (char*)malloc(BUFSIZE);
                char *content_buffer = (char*)malloc(BUFSIZE);

                //wait for OK\n from server, then begin the file uploading
                n = recvfrom(sockfd, inbound_buffer, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
                if (n<0){
		    printf("Waiting for OK...");
                    is_responsed = retry(sockfd, buf, inbound_buffer, serveraddr, serverlen, strlen(buf));
                }

                bzero(outbound_buffer, BUFSIZE);
                bzero(inbound_buffer, BUFSIZE);
                bzero(content_buffer, BUFSIZE);
		bzero(buf, BUFSIZE);
                //if receive OK\n from server -> begin file content transferring
                //if (is_responsed && strcmp(inbound_buffer, "OK\n") == 0){
		if (is_responsed){
                    printf("Got OK from server \n");
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
                        printf("Outbound_buffer before first send: %s \n", outbound_buffer + sizeof(int) + sizeof(size_t));
                        n = sendto(sockfd, outbound_buffer, sizeof(int) + sizeof(size_t) + bytesRead, 0, (struct sockaddr *) &serveraddr, serverlen);
                        if (n < 0) error("ERROR in sendto");

                        //wait for sequence number from server -> if timeout, retry sending data
                        n = recvfrom(sockfd, inbound_buffer, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
                        if (n<0){
                            printf("Outbound_buffer before retry: %s \n", outbound_buffer + sizeof(int));
                            is_responsed = retry(sockfd, outbound_buffer, inbound_buffer, serveraddr, serverlen, sizeof(int) + sizeof(size_t) + bytesRead);
                        }
                        if (is_responsed){
                            memcpy(&seq_num_client, inbound_buffer, sizeof(int));
                            if(seq_num_client == seq_num_server){
                                printf("Sequence number match: %d\n", seq_num_client);
                                seq_num_server = seq_num_server + 1; //if receive expected ack from server
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
                    //send DONE\n to the server, indicating that the client has finished sending all bytes of the requested file
                    bzero(outbound_buffer, BUFSIZE);
                    bzero(inbound_buffer, BUFSIZE);

                    //strcat(outbound_buffer, "DONE\n");
                    //n = sendto(sockfd, outbound_buffer, strlen(outbound_buffer), 0, (struct sockaddr *) &serveraddr, serverlen);
                    //if (n < 0) error("ERROR in sendto");
                    fclose(fd); //close file

                    //wait for DONE\n from client, retry if needed
                    //n = recvfrom(sockfd, inbound_buffer, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
                    //if (n<0){
                    //    printf("Outbound_buffer before retry: %s \n", outbound_buffer + sizeof(int));
                    //    is_responsed = retry(sockfd, outbound_buffer, inbound_buffer, serveraddr, serverlen, sizeof(int) + strlen(content_buffer));
                    //}
                    //printf("Got DONE from server \n");
		    printf("Done uploading file to server \n");

                }
                    //if client doesn't response to OK\n -> terminate the file transferring, wait for further commands
                else{
                    printf("No response from server, exiting file transferring...\n");
                }

                //free all allocated buffers -> otherwise, malloc issue
                free(inbound_buffer);
                free(outbound_buffer);
                free(content_buffer);
                free(file_name);

                //---------------------------------------
            }
            else{
                printf("File doesn't exist on client: %s \n", file_name);
            }

        }
        else{
            /* send the message to the server */
            serverlen = sizeof(serveraddr);
            n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
            if (n < 0) error("ERROR in sendto");

            // check the client's command, if it's get or put, enter UDP file transferring code
            if (strstr(buf, "get") != NULL){
                char *file_name = extract_filename(buf); //get the file name from the user's command
                printf("Requesting file from server: %s \n", file_name);
                //char inbound_buffer[BUFSIZE] = {'\0'}; // inbound buffer is used to store message receiving from server
                //char outbound_buffer[BUFSIZE] = {'\0'}; // outbound buffer is used to store message sending to the server
                //char content_buffer[BUFSIZ] = {'\0'};
                char *outbound_buffer = (char*)malloc(BUFSIZE);
                char *inbound_buffer = (char*)malloc(BUFSIZE);
                char *content_buffer = (char*)malloc(BUFSIZE);

                bzero(outbound_buffer, BUFSIZE);
                bzero(inbound_buffer, BUFSIZE);
                bzero(content_buffer, BUFSIZE);

                int is_responsed = 1;
                n = recvfrom(sockfd, inbound_buffer, BUFSIZE, 0, &serveraddr, &serverlen);
                if(n < 0){
                    is_responsed = retry(sockfd,buf,inbound_buffer,serveraddr,serverlen, strlen(buf));
                }
                if(is_responsed){ // begin file transfering if OK is sent from server
                    printf("Response from server: file downloading in progress: %s \n", file_name);
                    //send back OK\n message to server and enter file transferring loop
                    strcat(outbound_buffer, "OK\n");
                    n = sendto(sockfd, outbound_buffer, strlen(outbound_buffer), 0, (struct sockaddr *) &serveraddr, serverlen);
                    if (n < 0) error("ERROR in sendto");

                    int seq_num_client = 0;
                    int seq_num_server = 0;
		    size_t payload_bytes = 10000;

                    FILE *output_file;
                    output_file = fopen(file_name, "w+");
                    while (1){
                        //clear all buffers
                        bzero(inbound_buffer, BUFSIZE);
                        bzero(content_buffer, BUFSIZE);
                        //receive the message from server, if timeout, retry sending seq number n times
                        n = recvfrom(sockfd, inbound_buffer, BUFSIZE, 0, &serveraddr, &serverlen);
                        if (n<0){
                            is_responsed = retry(sockfd, outbound_buffer, inbound_buffer, serveraddr, serverlen, strlen(outbound_buffer));
                        }
                        //extract the sequence number from the first 4 bytes of inbound buffer, 1020 bytes are for data
                        memcpy(&seq_num_server, inbound_buffer, sizeof(int));
			            memcpy(&payload_bytes, inbound_buffer + sizeof(int), sizeof(size_t));
                        memcpy(content_buffer, inbound_buffer + sizeof(int)+ sizeof(size_t), payload_bytes);

                        //if data is DONE\n (meaning file transferring is finished) -> close output file and break the while loop
                        if (payload_bytes == 0){
                            fclose(output_file);
                            printf("File downloading is DONE: %s \n", file_name);
                            //echo DONE back to server
                            bzero(outbound_buffer, BUFSIZE);
                            //strcat(outbound_buffer, "DONE\n");
                            //n = sendto(sockfd, outbound_buffer, strlen(outbound_buffer), 0, (struct sockaddr *) &serveraddr, serverlen);
                            //if (n < 0) error("ERROR in sendto");
			    memcpy(outbound_buffer, &seq_num_client, sizeof(int));
                            n = sendto(sockfd, outbound_buffer, sizeof(int), 0, &serveraddr, serverlen); //send 4 bytes of sequence number to server
                            if (n < 0) error("ERROR in sendto");
                            break;
                        }
                        else if (is_responsed){ // otherwise write the data to file and send ack, if expected seq number received
                            if(seq_num_server == seq_num_client){//write to file if expected seq number received
                                //fprintf(output_file, content_buffer); //append inbound file content to local file
                                fwrite(content_buffer, sizeof(char), payload_bytes, output_file);
				                printf("Sequence number match: %d\n", seq_num_server);
                                printf("Received file content: %s \n", content_buffer);
                                // send ack containing seq num to server then clear outbound buffer
                                bzero(outbound_buffer, BUFSIZE);
                                //printf("Content buffer string length: %d \n", strlen(content_buffer));
                                memcpy(outbound_buffer, &seq_num_client, sizeof(int));
                                n = sendto(sockfd, outbound_buffer, sizeof(int), 0, &serveraddr, serverlen); //send 4 bytes of sequence number to server
                                if (n < 0) error("ERROR in sendto");
                                seq_num_client = seq_num_client + 1;
                            }
                            else{//seq number mismatch, simply discard the received message
                                printf("Sequence number mismatch: server %d; client %d \n", seq_num_server, seq_num_client);
                                //resend previous ACK
                                int prev_seq_num_client = seq_num_client - 1;
                                bzero(outbound_buffer, BUFSIZE);
                                memcpy(outbound_buffer, &prev_seq_num_client, sizeof(int));
                                n = sendto(sockfd, outbound_buffer, sizeof(int), 0, &serveraddr, serverlen); //send 4 bytes of sequence number to server
                                if (n < 0) error("ERROR in sendto");
                            }
                        }
                        else{ // if no response from server after 3 retries -> exit the file transferring code
                            fclose(output_file);
                            printf("Retried 3 times but no response from server, exiting file transferring...\n");
                        }

                    }
                }
                else if(is_responsed && strcmp(inbound_buffer, "FAIL\n") == 0){ // Server couldn't open the requested file
                    printf("Response from server: fail to open: %s \n", file_name);
                }
                else{
                    printf("Retried 3 times but no response from server, please try again later...\n");
                }
                free(inbound_buffer);
                free(outbound_buffer);
                free(content_buffer);
                free(file_name);

            }
            else{
                /* print the server's reply */
                n = recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);
                if (n < 0) { //if timeout and 0 bytes received from server -> retry sending the command to server for 3 times
                    printf("Retry sending: %s\n", buf);
		    int is_responsed = retry(sockfd,buf,buf,serveraddr,serverlen, strlen(buf));
                    if(is_responsed){
                        printf("Response from server: %s", buf);
                    }
                    else{
                        printf("Retried 3 times but no response from server, please try again later...\n");
                    }
                }
                else {
                    printf("Response from server: %s", buf);
                }

            }
        }
    }
    return 0;
}

