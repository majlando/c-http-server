#include "../src/http_parser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_fragmented_request() {
    http_parser_t p;
    http_parser_init(&p);
    const char *part1 = "GET /frag HTTP/1.1\r\nHost: exa";
    const char *part2 = "mple.com\r\n\r\n";
    size_t consumed = 0;
    int r1 = http_parser_execute(&p, part1, strlen(part1), &consumed);
    assert(r1 == 0); // need more data
    int r2 = http_parser_execute(&p, part2, strlen(part2), &consumed);
    assert(r2 == 1);
    assert(strcmp(http_parser_method(&p), "GET") == 0);
    assert(strcmp(http_parser_path(&p), "/frag") == 0);
    http_parser_destroy(&p);
    printf("test_fragmented_request passed\n");
}

int main(void) {
    test_fragmented_request();
    printf("ALL FRAGMENT TESTS PASSED\n");
    return 0;
}
