# UDP_Socket_programming
UDP (Use Datagram Protocol) is a connectionless transmission model with a minimum of protocol mechanism. It has no handshaking dialogues and no mechanism of acknowledgements (ACKs). Therefore, every packet delivered by using UDP protocol is not guaranteed to be received, but UDP avoids unnecessary delay, thanks to its simplicity.

--------
Approach
---------

The client:
The client takes two command line arguments: an IP address of the machine on which the server application is running, and the port the server application is using. (The IP address can be obtained using hostname -i) For example:
* $ gcc uftp_client.c -o client
* $ ./client 192.168.1.101 5001

The server:
The server takes one command line argument: a port number for the server to use. example:
* $ gcc uftp_server.c -o server # Compile your c-program
* $ ./server 5001 # Running your server with port number 5001
And then it waits for a UDP connection after binding to port 5001
And accordingly client could send get, put, delete, ls, exit [file_name]”

And some of the functions used in the code are -
* o sockfd = socket(PF_INET,SOCK_DGRAM,0); o sockfd is a UDP socket.
* bind(sockfd, (struct sockaddr *)&my_addr, sizeof my_addr);
* ssize_t sendto( int sockfd, void *buff, size_t nbytes, int flags, const struct sockaddr* to, socklen_t addrlen);
* ssize_t recvfrom( int sockfd, void *buff, size_t nbytes, int flags, struct sockaddr* from, socklen_t *fromaddrlen);
 


--------------
Code Structure
--------------

Functionality common to both the client and server lives in the "common" directory under the "src" directory. There are
three files:

* utils.h: Some very basic primitive operations, such as logging, configurable global variables, and some functions for
           processing socket addresses, time values, and strings.
* proto.h: The meat of the of protocol implementation. This contains the relevant structs for messages and connections,
           functions for creating and tearing them down, and for sending and receiving messages. A large portion of
           this file is also the code needed to serialize and deserialize the protocol messages into and from byte
           arrays for sending and receiving.
* processorUtils.h: Some support code for running "processors" in the client and server. Processors are functions
                    invoked when messages are received to advance the state of the connection.

The server lives under "src/server" as "udp_server.c", and the client lives under "src/client" as "udp_client.c". Some
test code lives in the "test" directory. Both the client and server use the poll(2) syscall to either wait for
incoming datagrams or wait a specified amount of time until another event (like a resend or connection timeout) would
occur. Both are designed using an "event driven" model hinging around non-blocking sockets and poll(2); the server
is designed to be able to support many simultaneous clients.

-----------------------
Testing
------------------------

I have written a few libcheck tests (under test) to check some of the primitive functions I have written in the
"common" directory. Additionally, the programs are built with essentially all of the GCC warning flags turned on
(and set trigger errors in all but a couple of trivial cases). I have run the programs with valgrind using both
error checking and leak checking, exhaustively testing many permutations of commands to verify (with high probability)
that neither the client nor server leak memory.

The code features a number of logging statements (that can be optionally turned on) for ease of debugging, and has a
couple of convenience methods to dump messages to stderr, or dump the contents of a byte buffer in hex.

I have also tested the performance against a known good TFTP server and client using the same systems, and get roughly
the same performance on file transfers as TFTP (when the window size option is off), which is expected (and a good
sign) given the structure and underpinnings of the protocols are very similar.

I additionally tested the client and server using the built in traffic control tooling. The client and server have been
tested to reliably transfer files correctly even with the following significant impediments:

* Up to 20% packet loss (would probably work with even more).
* Large packet delays and jitters (tested up to 750 ms).
* A significant number (30%+) of reordered and duplicate IP packets (and thus duplicated datagrams).

including all of the above at the same time.

-----------------------
Building and Running
-----------------------

**NOTE**: I have only built and developed this against Debian Linux. The server and client may build and run on Mac
          OS X or a BSD. They WILL NOT BUILD on Windows (without something like cygwin or WSL) because Windows lacks
          support for poll(2) and generally has a very poor libc implementation (msvc).

**NOTE**: This may not build against compilers other than GCC. It's likely that it will build with clang/llvm.

To build the client and server, (a relatively recent copy) of gcc and make are required. To build both at the same time:

   make clean && make all

The client and server binaries are placed in the "bin" directory. The server may be started as

   ${PROJECT_PATH}/bin/server ${YOUR_PORT}

The client may be started as:

   ${PROJECT_PATH}/bin/client ${SERVER_ADDRESS} ${SERVER_PORT}

There are a handful of environment variables that alter the behavior of the client and server. To enable more
aggressive logging, set DEBUG to either 1 (info logs) or 2 (info and debug logs). An easy way to do is this is the
following:

   DEBUG=1 ${PROJECT_PATH}/bin/server ${YOUR_PORT}
   DEBUG=1 ${PROJECT_PATH}/bin/client ${SERVER_ADDRESS} ${SERVER_PORT}

The commands for the client should work as specified in the programming assignment document.
