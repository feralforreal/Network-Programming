
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
#include <signal.h>
#include <set>
#include <fcntl.h>
#include <errno.h>
#include <string>
#include <sys/stat.h>
#include <sys/sendfile.h>


#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */


#define KEEPTIME 10.0

using namespace std;

enum Req {GET, POST, ERR};

struct ReqStruct {
    bool keepAlive;
    string req;
    string reqVersion;
    Req reqType;
    string postdata;
};

int open_listenfd(int port);
int resp(int connfd, ReqStruct reqdeets, bool keepAlive);
void *thread(void *vargp);

string fileRoot = "www";

int kilt = 0;
set<pthread_t> threadset;

ReqStruct parseReq(char * req, int connfd)
{
    ReqStruct returner;

    returner.keepAlive = false;

    char * reqLine = strtok(req, "\r\n");
    if(reqLine == NULL)
    {
        returner.reqType = ERR;
        return returner;
    }
    char  * reqLinePersist = (char *)malloc(sizeof(char) * MAXLINE);
    strcpy(reqLinePersist, reqLine);
    int firstLineLen = strlen(reqLinePersist);
    //printf("First line: %s\n", reqLinePersist);

    // parse first line 
    char lineDelim[] = {' '};
    char * tok = strtok(reqLinePersist, lineDelim);
    if(tok == NULL)
    {
        returner.reqType = ERR;
        return returner;
    }
    else if(strncmp(tok, "GET", 3) == 0)
    {
        returner.reqType = GET;
        returner.postdata = "";
    }
    else if(strncmp(tok, "POST", 4) == 0)
    {
        returner.reqType = POST;
    }
    else
    {
        returner.reqType = ERR;
        return returner;
    }
    // get req file
    tok = strtok(NULL, lineDelim);
    if(tok == NULL)
    {
        returner.reqType = ERR;
        return returner;
    }
    returner.req = tok;
    tok = strtok(NULL, lineDelim);
    if(tok == NULL)
    {
        returner.reqType = ERR;
        return returner;
    }
    // get the HTML version 
    // (for this implementation, doesn't change much)
    returner.reqVersion = tok;

    reqLine = strtok(req+firstLineLen+1, "\r\n");
    long postDataLen = 0;
    while(reqLine != NULL)
    {
        //printf("Loop line: %s\n", reqLine);
        if(strcmp(reqLine, "Connection: keep-alive") == 0 
                || strcmp(reqLine, "Connection: Keep-alive") == 0)
        {
            printf("Keepalive set to true!\n");
            returner.keepAlive = true;
        }
        else if(returner.reqType == POST && 
            strncmp(reqLine, "Content-Length: ", 16) == 0)
        {
            postDataLen = strtol(reqLine + 16, NULL, 10);
        }
        // keep the last line around, in case we need postdata
        strcpy(reqLinePersist, reqLine);
        reqLine = strtok(NULL, "\r\n");
    }

    if(postDataLen != 0)
    {
        returner.postdata = reqLinePersist;
        if(returner.postdata.length() > postDataLen)
        {
            returner.postdata = returner.postdata.substr(0, postDataLen);
        }
        postDataLen -= returner.postdata.length();
        if(postDataLen > 0)
        {
            char buf[postDataLen];
            recv(connfd, buf, postDataLen, 0);
            returner.postdata += buf;
        }
    }

    free(reqLinePersist);
    return returner;
}

/* thread routine */
void * thread(void * vargp) 
{  
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self()); 
    size_t n;
    // don't die on broken pipe
    signal(SIGPIPE, SIG_IGN);
    bool keepAlive = true;
    time_t lastTime = time(NULL);

    while(!kilt && keepAlive)
    {
        if(keepAlive && difftime(time(NULL), lastTime) > KEEPTIME)
        {
            printf("KEEPALIVE ELAPSED!!\n");
            break;
        }

        char buf[MAXLINE];
        n = recv(connfd, buf, MAXLINE, 0);
        if(n == -1 && errno == EAGAIN)
        {
        //    printf("Thread not blocking");
        }
        else if (n == 0)
        {
            printf("Other side closed conn for thread!\n");
            break;
        }
        else
        {
            printf("server received the following request:\n%s\n",buf);

            ReqStruct parsedReq = parseReq(buf, connfd);
            if(parsedReq.keepAlive)
            {
                keepAlive = true;
                lastTime = time(NULL);
            }
            else
            {
                keepAlive = false;
            }

            int respStat = resp(connfd, parsedReq, keepAlive) ;
            if(respStat == -1 && errno == EPIPE)
            {
                printf("Thread broke pipe!");
                break;
            }
            else if(respStat == -1)
            {
                printf("Error Sending!");
                break;
            }
        }
    }
    printf("THREAD DUN BIN KILT!!!!\n");
    close(connfd);
    free(vargp);
    pthread_exit(NULL);
}

// send an error message
int sendErr(int connfd)
{
    char buf[MAXLINE]; 
    // Chrome is unhappy with this, but the directions say 
    // "These messages in the exact format as shown above should be sent back to the client if any of the above
    // error occurs."
    string httpmsg="HTTP/1.1 500 Internal Server Error";

    strcpy(buf,httpmsg.c_str());
    printf("server returning a http message with the following content.\n%s\n",buf);
    return write(connfd, buf, httpmsg.length());
}

// get file extension 
const char* getExtension(const char * req)
{
    const char * extLoc = strrchr(req, '.');

    if(!extLoc || extLoc == req)
    {
        return NULL;
    }

    return extLoc + 1;
}

/*
 * Respond to client req
 */
int resp(int connfd, ReqStruct reqdeets, bool keepAlive) 
{
    printf("Req file: %s\n", (fileRoot + reqdeets.req).c_str());

    size_t n;

    if(reqdeets.reqType == ERR)
    { 
        return sendErr(connfd);
    }
    else
    {
        string resp = reqdeets.reqVersion + " 200 OK\r\n";

        if(keepAlive)
        {
            resp += "Connection: keep-alive\r\n";
        }
        else if(reqdeets.reqVersion.compare("HTTP/1.1") == 0)
        {
            resp += "Connection: close\r\n";
        }

        if(reqdeets.req.compare("/") == 0)
        {
            reqdeets.req = "/index.html";
        }

        // get content type:
        // (not elegant, sorry--the way to do this would be to check
        // /etc/mime.types automatically)
        resp += "Content-Type: ";
        const char * ext = getExtension(reqdeets.req.c_str());
        if(ext == NULL)
        {
            printf("No file extension found for |%s|\n", reqdeets.req.c_str());
            return sendErr(connfd);
        }
        else if(strcmp(ext, "html") == 0)
        {
            resp += "text/html\r\n";
        }
        else if(strcmp(ext, "js") == 0)
        {
            resp += "application/javascript\r\n";   
        }
        else if(strcmp(ext,"css") == 0)
        {
            resp += "text/css\r\n";
        }
        else if(strcmp(ext, "png") == 0)
        {
            resp += "img/png\r\n";
        }
        else if(strcmp(ext, "txt") == 0)
        {
            resp += "text/plain\r\n";
        }
        else if(strcmp(ext, "gif") == 0)
        {
            resp += "image/gif\r\n";
        }
        else if(strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0)
        {
            resp += "image/jpeg\r\n";
        }
        // help from https://stackoverflow.com/questions/13827325/correct-mime-type-for-favicon-ico 
        // for mime type
        else if(strcmp(ext, "ico") == 0)
        {
            resp += "image/x-icon\r\n";
        }
        // Apparently, missing content type can be a problem, so I'm calling it an error!
        // https://www.iothreat.com/blog/content-type-header-missing
        else
        {
            return sendErr(connfd);
        }

        // Get file deets
        struct stat filestat;
        int fd = open( (fileRoot + reqdeets.req).c_str(), O_RDONLY );
        if (fd < 0) 
        {
            printf("can't open file! %s\n", reqdeets.req.c_str());
            return sendErr(connfd);
        }
        if(fstat(fd, &filestat) < 0)
        {
            printf("can't get file deets! %s\n", reqdeets.req.c_str());
            return sendErr(connfd);
        }


        // deal with file length stuff:
        int fileLen = filestat.st_size;
        string postExtra = "";
        if(reqdeets.postdata.length() > 0)
        {
            // Using a VERY LITERAL interpretation of the example. 
            postExtra = "<html><body><pre><h1>" + reqdeets.postdata + "</h1></pre>";
            fileLen += postExtra.length();
        }

        resp += "Content-Length: " + to_string(fileLen) + "\r\n\r\n" + postExtra;

        printf("Responding with \n%s", resp.c_str());

        // write HTTP bit to client
        int sent_data_header = write(connfd, resp.c_str(), resp.length());
        if(sent_data_header == -1)
        {
            return sent_data_header;
        }
        // write file to client
        int data_left = filestat.st_size;
        int sent_data = 0;
        off_t off = 0;
        while (((sent_data = sendfile(connfd, fd, &off, MAXBUF)) > 0) && (data_left > 0))
        {
            // signal something broke if it broke. 
            if(sent_data == -1)
            {
                return -1;
            }
            data_left -= sent_data;
            fprintf(stdout, "Sent %d bytes, offset: %ld remaining data: %d\n", sent_data, off, data_left);
        }
        return filestat.st_size + sent_data_header;
    }
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

    /*
   * Set socket to non-blocking 
   */
    int nonBlockSuc = fcntl(listenfd, F_SETFL, fcntl(listenfd, F_GETFL, 0) | O_NONBLOCK);
    if (nonBlockSuc == -1){
        perror("Error setting listen socket to non-blocking");
        return -1;
    }

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

// With help from https://www.geeksforgeeks.org/signals-c-language/#
// (not an OS course, don't feel bad)
// handles signals for graceful exit! 
void killIt(int sig) 
{ 
    printf("KILLING!\n"); 
    kilt = 1;
} 

int main(int argc, char **argv) 
{
    signal(SIGINT, killIt);
    
    int listenfd, *connfdp = NULL, port;
    socklen_t clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid = -1; 

    if (argc != 2) 
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    port = atoi(argv[1]);

    listenfd = open_listenfd(port);
    while (listenfd != -1 && !kilt) {
        connfdp = (int*)malloc(sizeof(int));


        *connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);

        // handle continuous loop
        if(*connfdp == -1 && (errno == EWOULDBLOCK || errno == EAGAIN))
        {
            // I hate that I'm leaving this, but embrace the pain...
            //printf("Not kilt, not blocking: %d\n", kilt);
            free(connfdp);
            connfdp = NULL;
        }
        else
        {
            int nonBlockSuc = fcntl(*connfdp, F_SETFL, fcntl(listenfd, F_GETFL, 0) | O_NONBLOCK);
            if (nonBlockSuc == -1){
                perror("Error setting connection to non-blocking");
                break;
            }
            printf("Making thread\n");
            pthread_create(&tid, NULL, thread, connfdp);
            threadset.insert(tid);
        }
    }

    if (kilt)
    {
        for (set<pthread_t>::iterator it=threadset.begin(); it!=threadset.end(); ++it)
        {
            pthread_join(*it, NULL);
        }
        printf("DUN BIN KILT\n");
    }
    else printf("Not kilt, but dead anyhow: %d\n", listenfd);

    if(connfdp != NULL)
        free(connfdp);

    return 0;
}
