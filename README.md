# c-http-server

Minimal educational HTTP server written in C. Small, readable example that demonstrates non-blocking sockets with epoll and an incremental HTTP request parser.

Badges

![CI](https://github.com/majlando/c-http-server/actions/workflows/ci.yml/badge.svg)

Quickstart

1. Build:

```sh
make
```

2. Run the server (listens on port 8080):

```sh
./bin/c-http-server
```

3. From another terminal:

```sh
curl -i http://127.0.0.1:8080/
```

Tests

- Run all tests (they are small and fast):

```sh
make test
```

- Quick sanitizer build (AddressSanitizer + UBSan):

```sh
make asan
```

Repository layout

- `src/` — server, parser and utilities
- `tests/` — small test binaries exercising parser and helpers
- `www/` — static files served by the server (edit `www/index.html`)
- `bin/` — build output

Design notes

- Focus: clarity and education. The server is intentionally compact rather than fully featured.
- Core features: non-blocking I/O with `epoll`, incremental request parsing (handles fragmented input), simple static file serving.
- Limitations: no TLS, no chunked transfer support, limited MIME map. Use a reverse proxy for production use.

Development and contribution

- Keep changes small and add tests for parser changes.
- Recommended CI: run the unit tests and an ASAN/UBSan build.

Security

- `src/fsutils.c` resolves and validates requested filesystem paths using `realpath()` to avoid directory-traversal via `..` and symlinks. Review behaviour if serving untrusted content.

License

Provided for educational use.
