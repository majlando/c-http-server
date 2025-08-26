# c-http-server

![CI](https://github.com/majlando/c-http-server/actions/workflows/ci.yml/badge.svg)

Minimal educational HTTP server in C. This project is a compact scaffold that demonstrates non-blocking sockets with epoll and a tiny incremental HTTP request parser.

Highlights
- Non-blocking I/O using epoll (Linux)
- Small incremental parser that tolerates fragmented input
- Static file serving from `www/`
- Lightweight unit tests for the parser

Quick start
```sh
make
./bin/c-http-server
# then in another terminal
curl -i http://127.0.0.1:8080/
```

Repository layout
- `src/` — server and parser sources
- `tests/` — small test binaries for the parser and utilities
- `www/` — static files served by the server (index.html)
- `bin/` — build output

Build and test
```sh
# build
make

# run all tests
make test
```

Development notes
- The parser (`src/http_parser.c`) currently copies incoming data into an internal buffer and reports how many bytes it consumed. Consider refactoring to an in-place parser for more efficient streaming and pipelining.
- The server uses a simple write-queue per connection to handle partial non-blocking writes. It supports `Connection: keep-alive`/`close` semantics.

Security & limitations
- No TLS support (use a reverse proxy or add TLS with an external library)
- Limited HTTP feature set: no chunked transfer encoding support, limited MIME types
- `www/` resolving includes URL-decoding and normalization, but review carefully before exposing to untrusted content

Contributing
- Fixes, improvements and tests are welcome. Try to keep diffs small and include tests for parser changes.

License
This project is provided as-is for educational purposes.
