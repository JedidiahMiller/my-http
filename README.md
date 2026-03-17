# My HTTP

This is a project to build an HTTP server from scratch that can server a website made up of static files. It supports HTTP/1.1 as specified in [RFC 2616](https://datatracker.ietf.org/doc/html/rfc2616)

## Running

To run this project, clone it and run `make run`. It will start serving files at 127.0.0.1:80. The port can be configured using the PORT constant in http.h

Files will be served from the `public` directory at the root of the server.

This server can be tested using curl

```bash
curl --ipv4 --http1.1 -v localhost:80/index.html
```

## Dev

The project can be built using `make`

All source and header files can be formatted using `make pretty`. This assumes clang-format is installed.
