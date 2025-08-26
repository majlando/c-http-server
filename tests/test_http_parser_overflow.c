#include "../src/http_parser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    http_parser_t p;
    http_parser_init(&p);
    /* build a request that will exceed HTTPP_MAX_BUF when fed in one shot */
    size_t big = HTTPP_MAX_BUF + 100;
    /* allocate safely: ensure plenty of room for request-line and headers */
    size_t alloc = big + 256;
    char *buf = malloc(alloc);
    if (!buf) return 1;
    /* start with request-line */
    strcpy(buf, "GET /");
    size_t off = strlen(buf);
    /* fill with many 'a' characters into the path to overflow */
    size_t fill = big - off - 10;
    for (size_t i = 0; i < fill; ++i) buf[off + i] = 'a';
    off += fill;
    strcpy(buf + off, " HTTP/1.1\r\nHost: example.com\r\n\r\n");
    size_t consumed = 0;
    int r = http_parser_execute(&p, buf, strlen(buf), &consumed);
    /* Expect overflow */
    assert(r == HTTPP_OVERFLOW);
    free(buf);
    printf("test_overflow passed\n");
    return 0;
}
