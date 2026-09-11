
# WSERVE: Educational Web Server in Pure C

The goal of this project to build an education IPv4 web server in pure C. 
Educational means it will not have any production use, and therefore
code readability is more important than its security or efficiency.

The web server will be able parse a subset of HTTP/1.1 commands.
These commands are GET, HEAD, POST. So, there will also be a minimal
parser for this project. Transfer-Encoding, conflicting duplicate 
Content-Length values, malformed requests, oversized headers, and 
oversized bodies will be automaticly rejected. Response codes will be
implemented. For each connection, it will serve a single request and 
response pair and close the connection. 

There will be a routing table just like the way Flask works. Each route
will have a path (/about), method (GET), and a corresponding handler 
function. For example, the path (/about) could respond to GET requests 
and send about.html. All will be configurable in C. It will return 404
if route not found and 405 if route is not configured for the method.
There will be static mounts for assets, and directory will be 
configurable. Only serve files under project root. Reject empty, 
absolute, `.`, and `..` path components and malformed percent encodings.
Prevent symbolic-link escapes with descriptor-based traversal using 
`openat()` and `O_NOFOLLOW`. Return `404` for missing files and 
directories. Do not provide directory listings or index-file resolution.
It will initially support a small explicit MIME map: HTML, CSS, 
JavaScript, JSON, plain text, PNG, JPEG, SVG, and `application/octet-stream`.

## v0.1 [COMPLETE]

TCP server loop that accepts a connection, receives up to N bytes,
sends a response, and closes the connection. 

Compile the code with `make`. Run `./wserve` and send a message with
`echo 123 | nc localhost 6666`. It is 4 chars as `echo` appends a 
newline automatically.

* https://beej.us/guide/bgnet/html/split/system-calls-or-bust.html#sendrecv
* https://www.man7.org/linux/man-pages/man2/recv.2.html
* https://www.man7.org/linux/man-pages/man2/send.2.html
* https://www.man7.org/linux/man-pages/man7/signal.7.html

## v0.2 

Simple HTTP headers parser. Test set for the parser.


## v0.3 

Server that only responds to HTTP requests.
