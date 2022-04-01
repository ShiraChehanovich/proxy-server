# proxy-server

HTTP Proxy Server – Phase 2

==Program Files==
proxyServer.c- contains all the program- read filter file, connect to clients, genarate threadpool, connect to the web server(if needed), write a http request and get the http respond, return to the client the response or the error.
README.txt- current file
threadpool.c

==Description==
the program implements a HTTP proxy server, who receives requests from the client server, operates it:
1. if the request doesn't fit the format, a 400 error will be return.
2. if the host name appears in the filter file, a 403 error will be returned.
3. if the requested file already exists in the data base- the file is provided.
4. if not- then a HTTP request will be create and then send to the web server. the HTTP respond will be saved in the local directory, and return to the client.

==input==
You can enter any http request you want in the following format:
GET /<path> http/1.1
HOST: <host name>
<other data>

or- 
GET /<path> http/1.0
HOST: <host name>
<other data>

==output==
if the request you enterd was correct- 
if the file is already in the directory, the HTTP respond will be the following format:

 Content-Length: <file_size_in_bytes>
<file content>
File is given from local filesystem
Total response bytes: <total_respond_in_bytes>

if the file doesn't exist, the file will be saved in the directory from the request, and the HTTP respond will be returned in the following format:

HTTP request = 
<http request>
LEN = NUMBER 
HTTP/1.0 200 OK 
Content-Length: <file_size_in_bytes> 
<file content>
Total response bytes: <total_respond_in_bytes>

If the application does not fit the above format, the proxy will return:

HTTP/1.0 400 Bad Request
Content-Type: text/html
Content-Length: 113
Connection: close

<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>
<BODY><H4>400 Bad request</H4>
Bad Request.
</BODY></HTML>

If the request isn't a GET request, the proxy will return:

HTTP/1.0 501 Not supported
Content-Type: text/html
Content-Length: 129
Connection: close

<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>
<BODY><H4>501 Not supported</H4>
Method is not supported.
</BODY></HTML>

If the host in the request apears in the filter file:

HTTP/1.0 403 Forbidden
Content-Type: text/html
Content-Length: 111
Connection: close

<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>
<BODY><H4>403 Forbidden</H4>
Access denied.
</BODY></HTML>

For any internal error, will be returned:

HTTP/1.0 500 Internal Server Error
Content-Type: text/html
Content-Length: 144
Connection: close

<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>
<BODY><H4>500 Internal Server Error</H4>
Some server side error.
</BODY></HTML>

If the function Gethostbyname couldn't find the host:

HTTP/1.0 404 Not Found
Content-Type: text/html
Content-Length: 112
Connection: close

<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>
<BODY><H4>404 Not Found</H4>
File not found.
</BODY></HTML>
 

==How to compile?==
compile: gcc -Wall proxyServer.c -o proxyServer threadpool.c -lpthread

run: ./proxyServer <port> <pool-size> <max-number-of-request> <filter>
running exapmle: ./proxyServer 9014 1 3 /home/student/CLionProjects/proxyServer/filter.txt

==note==
This proxy support IPv4 connections and HTTP/1.0 and HTTP/1.1 requests only

  
