A HTTP-based web server that handles multiple simultaneous requests from users.

A web server is a program that receives requests from a client and sends the processed result back to the client as a response to the command. A client is usually a user trying to access the server using a web browser (Google Chrome, Firefox).

The HTTP request consists of three substrings – request method, request URL, and request version. All three parts should be separated by one or more white spaces.
The request method should be capital letters like “GET”, “HEAD”, and “POST”.
The request URL is a set of words which are case insensitive and separated by “/” and the server should treat the URL as a relative path of the current document root directory.

If I type an URL like http://www.w3.org/Protocols/rfc1945/rfc1945 in a web browser, the web browser will send an HTTP GET command to the server http://www.w3.org/ after establishing a TCP connection with port 80 on the server

TO COMPILE/RUN
In the root directory
$: make
$: ./server 8888
SERVED FILES:
The root directory of the served files is {project root}/www. I.E., replace the files in /www with your own if you want to serve different content. 

TO KILL:
Ctrl-C, which sends a kill signal to the main thread and all child threads. The main thread then joins any threads that may still be active and then closes itself. 

--------
Getting Files
----------
I interpret '/' and '/index.html' as index.html in a GET req. All other symbols I treat as a file to look for in the root (www) directory, and I return a 500 if that file does not exist. There sure is a functionality to automatically pull the mime types from the system, but I hardcoded for every file extension in the directory because the main difference between HTTP/1.1 and /1.0 is that /1.0 does not support persistant connections, where 1.1 does. In that case, I should not get a "Connection" line in a well-formed /1.0 req and I will not send a "Connection" line back in a /1.0 response. 

--------
Multiple Connections 
--------
When the client receives a web page that contains a lot of embedded pictures, it repeatedly requests an HTTP GET message for each object to the server. In such a case the server should be capable of serving multiple requests at same point of time. This can be done using select(), fork(), or pthread() with following approaches:
1. A multi-threaded approach that will spawn a new thread for each incoming connection. That is, once the server accepts a connection, it will spawn a thread to parse the request, transmit the file, etc.
2. A multi-process approach that maintains a worker pool of active processes to hand requests off to and from the main server.

Every time my listening socket gets a new connection, a POSIX thread is created. These run as long as necessary and their process IDs are stored in a set, which is run through when the process ends and each thread is "joined" before the server exits fully supported multiple connections (Technically done using pthreads)

-------
Error Handling
--------
When the HTTP request results in an error then the web-server should respond to the client with an error code. The “500 Internet Server Error” indicating that the server experiences unexpected system errors

-----------
Pipelining
-----------
I set the socket file descriptors for my threads to non-blocking, so each thread runs through a loop continuously even if there is no data to be read from the socket. This way, I can almost continuously check the current time against the stored time of the last message and then kill the thread when the timer expires. 
The server must send its responses to those requests in the same order that the requests were received. So, if there is a “Connection: Keep-alive” header in the request then a timer has to start pertaining to the
HTTP/1.1 500 Internal Server Error. These messages should be sent back to the client if any of the above error occurs.

* src/common/workQueue.{c,h}
    * A work queue based on pthreads that i have written but did not use

If there was no “Connection: Keep-alive” in the request header, then the socket will be closed immediately; the timer will never start for this case

For example, if the server received this client request and the message is something like this:
GET /index.html HTTP/1.1 

Host: localhost

Connection: Keep-alive

The response for this request should be something like the following:
HTTP/1.1 200 

OK

Content-Type: <>

Content-Length: <> 

Connection: Keep-alive <file contents>


 ----------
 Telnet 
 ----------
 Telnet by default was sending on "\r\0\r\n" for every "\r\n" echoed into it, and this is fixed by editing .telnetrc file to run the "toggle crlf" command on startup.
 
 using Telnet
 * (echo -en "GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: Keep-alive\r\n\r\n"; sleep 10; echo -en "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n") | telnet 127.0.0.1 8888
 
 using nc
 * (echo -en "GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: Keep-alive\r\n\r\n"; sleep 10; echo -en "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n") | nc 127.0.0.1 8888


-----------
POST method
-----------
When a POST request is sent, it should be handled in the same way as the GET and return the same web page as returned for the GET request.

Sample post request made to webserver while testing through telnet:

* (echo -en "POST /index.html HTTP/1.1\r\n Host: localhost\r\nConnection: Keep-alive\r\n\r\nPOSTDATA") | telnet 127.0.0.1 8888
  
  POST /www/sha2/index.html HTTP/1.1
  
  Host: localhost
  
  Content-Length: 9
  
  <blank line>
