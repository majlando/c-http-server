#define _GNU_SOURCE
#include "fsutils.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

/* Decode percent-encoded characters in-place. Returns 0 on success, -1 on invalid encoding. */
static int url_decode_inplace(char *s) {
    char *dst = s;
    char *src = s;
    while (*src) {
        if (*src == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            char buf[3] = { src[1], src[2], '\0' };
            char c = (char)strtol(buf, NULL, 16);
            *dst++ = c;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return 0;
}

/* Collapse "." and ".." components from a path (posix style). Returns newly written path in buf. */
static int normalize_path(char *buf, size_t buflen) {
    char *parts[PATH_MAX];
    size_t pcount = 0;
    char *saveptr = NULL;
    char *s = buf;
    // skip leading '/'
    if (*s == '/') s++;
    char *tok = strtok_r(s, "/", &saveptr);
    while (tok) {
        if (strcmp(tok, "") == 0 || strcmp(tok, ".") == 0) {
            // skip
        } else if (strcmp(tok, "..") == 0) {
            if (pcount > 0) pcount--;
            else return -1; // trying to go above base
        } else {
            if (pcount < PATH_MAX) parts[pcount++] = tok;
            else return -1;
        }
        tok = strtok_r(NULL, "/", &saveptr);
    }
    // rebuild
    char out[PATH_MAX];
    size_t off = 0;
    out[off++] = '/';
    for (size_t i = 0; i < pcount; ++i) {
        size_t l = strlen(parts[i]);
        if (off + l + 1 >= sizeof(out)) return -1;
        memcpy(out + off, parts[i], l);
        off += l;
        if (i + 1 < pcount) out[off++] = '/';
    }
    out[off] = '\0';
    if (off + 1 > buflen) return -1;
    memcpy(buf, out, off + 1);
    return 0;
}

int safe_resolve_path(const char *base, const char *reqpath, char *outbuf, size_t outlen) {
    if (!base || !reqpath || !outbuf) return -1;
    // make a local mutable copy of reqpath
    char tmp[PATH_MAX];
    if (strlen(reqpath) >= sizeof(tmp)) return -1;
    strncpy(tmp, reqpath, sizeof(tmp));
    tmp[sizeof(tmp)-1] = '\0';
    // default to /index.html
    if (tmp[0] == '\0' || strcmp(tmp, "/") == 0) strncpy(tmp, "/index.html", sizeof(tmp));
    // URL-decode in-place
    if (url_decode_inplace(tmp) != 0) return -1;
    // normalize to collapse .. and . components
    if (normalize_path(tmp, sizeof(tmp)) != 0) return -1;
    // join base + tmp
    char candidate[PATH_MAX];
    if (snprintf(candidate, sizeof(candidate), "%s%s", base, tmp) >= (int)sizeof(candidate)) return -1;
    // resolve real paths
    char real_candidate[PATH_MAX];
    if (!realpath(candidate, real_candidate)) return -1;
    char real_base[PATH_MAX];
    if (!realpath(base, real_base)) return -1;
    size_t bl = strlen(real_base);
    if (strncmp(real_candidate, real_base, bl) != 0) return -1;
    if (strlen(real_candidate) + 1 > outlen) return -1;
    strcpy(outbuf, real_candidate);
    return 0;
}
