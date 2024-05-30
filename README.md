# Network-Programming
The Network programming work goes here. Written in a mix of C, C++, Bash

-----------
UDP Socket programming 
-------------
UDP (Use Datagram Protocol) is a connectionless transmission model with a minimum of protocol mechanism. It has no handshaking dialogues and no mechanism of acknowledgements (ACKs). Therefore, every packet delivered by using UDP protocol is not guaranteed to be received, but UDP avoids unnecessary delay, thanks to its simplicity.
* I built two programs in C, one for the client which will simply send a command and the other for a server which will send a suitable response back to the client

------------
HTTP/TFTP Server Programming
------------
To create a HTTP-based web server that handles multiple simultaneous requests from users. A web server is a program that receives requests from a client and sends the processed result back to the client as a response to the command. A client is usually a user trying to access the server using a web browser (Google Chrome, Firefox). This program with basics of client/server communication message structure will provide a step toward building high performance servers.

------
Web Proxy Server Programming
-------
HTTP is a client-server protocol; the client usually connects directly with the server. But oftentimes it is useful to introduce an intermediate entity called proxy. The proxy sits
between HTTP clients and HTTP servers. With the proxy, the HTTP client sends a HTTP request to the proxy. The proxy then forwards this request to the HTTP server, and then
receives the reply from the HTTP server. The proxy finally forwards this reply to the HTTP client.

Features 
* Multi-threaded Proxy
* Caching (Timeout setting, Page Cache, Expiration, Hostname IP address cache, Blacklist)
* Link Prefetch

--------
Distributed File System Programming
---------
To create a distributed file system for reliable and secure file storage.A Distributed File System is a client/server-based application that allows client to store and retrieve files on multiple servers. One of the features of Distributed file systems is that each file can be divided into pieces and stored on different servers and can be retrieved even if one server is not active. 

Features 
* User Handling
* Directory Handling
* File Pieces Handling
* Implement Sub Folder on DFS
* Multi-Threading and Multi-Processing
* Data Encryption

  


