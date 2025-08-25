#include "../src/http_parser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_simple_request() {
    http_parser_t p;
    http_parser_init(&p);
    const char *req = "GET /hello HTTP/1.1\r\nHost: example.com\r\nUser-Agent: test\r\n\r\n";
    size_t consumed = 0;
    int r = http_parser_execute(&p, req, strlen(req), &consumed);
    assert(r == 1);
    printf("consumed=%zu\n", consumed);
    printf("raw buffer=\n[%s]\n", p.buf);
    fflush(stdout);
    assert(strcmp(http_parser_method(&p), "GET") == 0);
    assert(strcmp(http_parser_path(&p), "/hello") == 0);
    assert(strcmp(http_parser_version(&p), "HTTP/1.1") == 0);
    int hc = http_parser_header_count(&p);
    printf("headers: %d\n", hc);
    fflush(stdout);
    for (int i = 0; i < hc; ++i) {
        printf("  %s: %s\n", http_parser_header_name(&p, i), http_parser_header_value(&p, i));
    }
    assert(hc == 2);
    printf("test_simple_request passed\n");
}

int main(void) {
    test_simple_request();
    printf("ALL TESTS PASSED\n");
    return 0;
}
