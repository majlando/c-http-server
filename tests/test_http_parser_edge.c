#include "../src/http_parser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void test_invalid_request() {
    http_parser_t p;
    http_parser_init(&p);
    const char *bad = "BADREQUEST\r\n\r\n";
    size_t consumed = 0;
    int r = http_parser_execute(&p, bad, strlen(bad), &consumed);
    // parser should detect malformed request-line and return -1
    assert(r == -1);
    printf("test_invalid_request passed\n");
}

void test_header_without_colon() {
    http_parser_t p;
    http_parser_init(&p);
    const char *req = "GET / HTTP/1.1\r\nHost example.com\r\n\r\n";
    size_t consumed = 0;
    int r = http_parser_execute(&p, req, strlen(req), &consumed);
    // parser doesn't crash and should still parse the request-line
    assert(r == 1);
    // header without colon should be ignored, so header count should be 0
    assert(http_parser_header_count(&p) == 0);
    http_parser_destroy(&p);
    printf("test_header_without_colon passed\n");
}

void test_many_headers() {
    http_parser_t p;
    http_parser_init(&p);
    // build a request with many headers up to HTTPP_MAX_HEADERS
    size_t bufsize = 8192;
    char *buf = malloc(bufsize);
    strcpy(buf, "GET /many HTTP/1.1\r\n");
    for (int i = 0; i < HTTPP_MAX_HEADERS; ++i) {
        char hdr[64];
        snprintf(hdr, sizeof(hdr), "H%d: v%d\r\n", i, i);
        strncat(buf, hdr, bufsize - strlen(buf) - 1);
    }
    strncat(buf, "\r\n", bufsize - strlen(buf) - 1);

    size_t consumed = 0;
    int r = http_parser_execute(&p, buf, strlen(buf), &consumed);
    assert(r == 1);
    assert(http_parser_header_count(&p) == HTTPP_MAX_HEADERS);
    http_parser_destroy(&p);
    free(buf);
    printf("test_many_headers passed\n");
}

void test_long_header_value() {
    http_parser_t p;
    http_parser_init(&p);
    // create a long header value but within limits
    size_t val_len = 2000;
    char *val = malloc(val_len + 1);
    memset(val, 'A', val_len);
    val[val_len] = '\0';
    size_t bufsize = val_len + 200;
    char *buf = malloc(bufsize);
    snprintf(buf, bufsize, "GET /long HTTP/1.1\r\nLong: %s\r\n\r\n", val);
    size_t consumed = 0;
    int r = http_parser_execute(&p, buf, strlen(buf), &consumed);
    assert(r == 1);
    assert(http_parser_header_count(&p) == 1);
    const char *v = http_parser_header_value(&p, 0);
    assert(v && strlen(v) == val_len);
    http_parser_destroy(&p);
    free(val);
    free(buf);
    printf("test_long_header_value passed\n");
}

int main(void) {
    test_invalid_request();
    test_header_without_colon();
    test_many_headers();
    test_long_header_value();
    printf("ALL EDGE TESTS PASSED\n");
    return 0;
}
