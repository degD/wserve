
# WSERVE: Static Web Server in Pure C

This is a static IPv4 Linux web server written in pure C. No external libraries used, except [bearSSL](https://www.bearssl.org/). Even the HTTP parser is written in pure C. It supports a subset of HTTP/1.1. GET, HEAD, POST requests are supported and custom paths can be configured for dynamic routing. HTTPS is supported through [bearSSL](https://www.bearssl.org/). But `Transfer-Encoding: chunked` support, extensive E2E testing and a stable release is still in progress. The project also includes a custom JSON parser specifically designed for the POST requests. 

## Features

- IPv4 supported.
- Pure C.
- HTTP/1.1 subset.
- GET and HEAD supported. POST is planned.
- Static serving.
- Extensive MIME types support.
- Automatic rejection of malformed requests.
- Prevention of static directory escape.
- Extensive error responses.
- Dynamic routes
- Functional `POST` handling
- Custom JSON parser
- HTTPS

## Not supported (yet)

- Keep-alive connections
- Chunked transfer encoding
- Production hardening or compatibility guarantees

## Roadmap

- [x] TCP Server
- [x] HTTP Parser
- [x] Static Routing
- [x] Dynamic Routing

For detailed version roadmap, see [PLAN.md](PLAN.md).

## Disclaimer

This is a learning project by a single developer. Use it only as a reference. It is highly discouraged to use it in production, or even if personal, in open web. It has a relatively small test set with quick code inspection using some LLMs. There are much better HTTP server implementations available in C out there. If you really want to use it in production, or in open web, that is entirely on you.

## Requirements

- Linux kernel 5.6 or later (`openat2()` is required)
- GCC
- Make
- BearSSL (For HTTPS)

## Installation

1. `git clone https://github.com/degD/wserve`
2. `cd wserve`
3. `make`
4. `./wserve`

To compile with BearSSL:

1. `cd wserve`
2. `git clone https://www.bearssl.org/git/BearSSL`
3. `cd BearSSL && make`
4. `cd .. && SECURE=1 make`

To run unit tests:

1. `make test.parser && ./test.parser`
2. `make test.static && ./test.static`

To test the JSON parser, [see detailed instructions](./test/json/setup.md). JSON parser is WIP. But the current test results are promising. Check `C wserve`.

![JSON test results](assets/json-parser-results-18-10-2026.png)

## Configuration

By default, the web server runs on port `6543` ([http://localhost:6543](http://localhost:6543)) and uses `test/static` as the root of the static assets. `test/static` includes a demo site. After compiling and running, visit this site to see the server in action. Server defaults are set by using the function `init_server_settings`. Modify the call of `init_server_settings` in `main.c` to change the configuration and compile again.

![wserve demo page](assets/wserve.png)

```c
init_server_settings(
    "test/static", 
    "6543", 
    100, 
    1024, 
    1024*1024
);
```

- **root**: The directory of static assets.
- **port**: Port the server will listen.
- **backlog**: Max number of requests to be kept on the queue. 
- **max_recv_size**: Max number of bytes to receive in a single step.
- **total_req_size**: Max size of a request.

By default, `wserve` uses self-signed test certificates in HTTPS mode. For details, [see certificate generation page](test/https/README.md).

## Source

The source code consists of a single file: `wserve.c`. Inside, the code is divided roughly into 5 sections. As the time of writing, `DYNAMIC ROUTING` section is in progress:

- `SERVER TCP FUNCTIONS`
- `HTTP HEADERS PARSER`
- `HTTP SERVER`
- `STATIC ROUTING`
- `DYNAMIC ROUTING`

## Credits

- [https://beej.us/guide/bgnet/html/split/](https://beej.us/guide/bgnet/html/split/)
- [https://www.rfc-editor.org/info/rfc7230](https://www.rfc-editor.org/info/rfc7230)

For detailed credits for of each development version, see [PLAN.md](PLAN.md).

## License

MIT
