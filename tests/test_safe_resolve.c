#include "../src/fsutils.h"
#include <limits.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char out[4096];
    int r;
    // simple index
    r = safe_resolve_path("www", "/", out, sizeof(out));
    assert(r == 0);
    printf("resolved / -> %s\n", out);
    // normal file
    r = safe_resolve_path("www", "/index.html", out, sizeof(out));
    assert(r == 0);
    // traversal attempt
    r = safe_resolve_path("www", "/../etc/passwd", out, sizeof(out));
    assert(r == -1);
    // encoded traversal attempt
    r = safe_resolve_path("www", "/%2e%2e/%2e%2e/etc/passwd", out, sizeof(out));
    assert(r == -1);
    printf("ALL FSUTILS TESTS PASSED\n");
    return 0;
}
