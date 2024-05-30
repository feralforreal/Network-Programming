To create a distributed file system for reliable and secure file storage.

--------
A Distributed File System is a client/server-based application that allows client to store and retrieve files on multiple servers. One of the features of Distributed file systems is
that each file can be divided into pieces and stored on different servers and can be retrieved even if one server is not active.


---------
Objective 
---------
one distributed file system client (DFC) uploads and downloads to and from 4 distributed file servers (DFS1, DFS2, DFS3 and DFS4). DFS stands for distributed
file system. The DFS servers are all running locally on a single machine with different port numbers, for e.g. from 10001 to 10004. When a DFC wants to upload a file to the 4 DFS servers, it first splits the file into 4 equal
length pieces P1, P2, P3, P4 (a small length difference is acceptable if the total length cannot be divided by 4). Then the DFC groups the 4 pieces into 4 pairs such as (P1, P2),
(P2, P3), (P3, P4), (P4, P1). Finally, the DFC uploads them onto 4 DFS servers. So now the file has redundancy, 1 failed server will not affect the integrity of the file.

This depends on the MD5 hash value of the file. Let x = MD5HASH(file) % 4 

---------
Functions for DFC
---------

The client needs to run with the following command # dfc dfc.conf. The configuration file dfc.conf contains the list of DFS server addresses, username and
password. The username and password are used to authenticate its identity to DFS.

* Server DFS1 127.0.0.1:10001 
* Server DFS2 127.0.0.1:10002
* Server DFS3 127.0.0.1:10003
*  Server DFS4 127.0.0.1:10004
*  Username: X
*  Password: XyX5aghB&

  And should be run by giving # dfs /DFS1 <Port Number> &

--------------------------
Traffic optimization :

In the default GET command it gets all available pieces from all available servers it actually consumes twice of the actual data needed. An upgraded GET command can reduce traffic consumption.

