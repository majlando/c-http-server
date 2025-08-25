#include "http_parser.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// internal buffer and state are defined in header

static char *trim_crlf(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\r' || s[n-1] == '\n')) s[--n] = '\0';
    return s;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

void http_parser_init(http_parser_t *p) {
    memset(p, 0, sizeof(*p));
}

int http_parser_execute(http_parser_t *p, const char *data, size_t len, size_t *consumed) {
    *consumed = 0;
    if (p->buflen + len >= sizeof(p->buf)) return -1;
    memcpy(p->buf + p->buflen, data, len);
    p->buflen += len;
    p->buf[p->buflen] = '\0';

    // Look for CRLF CRLF which ends headers
    char *pos = strstr(p->buf, "\r\n\r\n");
    if (!pos) {
        // not complete
        *consumed = len;
        return 0;
    }

    // Do not write into buffer at pos yet; hdr_end marks end of headers
    size_t hdr_len = (pos - p->buf) + 4;
    *consumed = hdr_len;

    // parse request-line using the CRLF that ends the first line
    char *line_end = strstr(p->buf, "\r\n");
    if (!line_end) return -1;
    *line_end = '\0';
    char *line = p->buf;
    char *sp1 = strchr(line, ' ');
    if (!sp1) return -1;
    char *sp2 = strchr(sp1 + 1, ' ');
    if (!sp2) return -1;
    *sp1 = '\0';
    *sp2 = '\0';
    p->method = xstrdup(line);
    p->path = xstrdup(sp1 + 1);
    p->version = xstrdup(sp2 + 1);

    // parse headers by copying header block into a temporary buffer
    p->headers = 0;
    char *hdr_start = line_end + 2;
    // include the CRLF that terminates the last header (pos points to the CR of CRLFCRLF)
    size_t hdr_block_len = (pos - hdr_start) + 2;
    if (hdr_block_len > 0) {
        char *hb = malloc(hdr_block_len + 1);
        if (hb) {
            memcpy(hb, hdr_start, hdr_block_len);
            hb[hdr_block_len] = '\0';
            char *linep = hb;
            char *hb_end = hb + hdr_block_len;
            while (linep < hb_end && p->headers < HTTPP_MAX_HEADERS) {
                char *newline = memchr(linep, '\n', hb_end - linep);
                if (!newline) break;
                // remove optional '\r' before newline
                if (newline > linep && *(newline - 1) == '\r') *(newline - 1) = '\0';
                else *newline = '\0';
                if (*linep == '\0') break;
                char *colon = strchr(linep, ':');
                if (colon) {
                    *colon = '\0';
                    char *name = linep;
                    char *value = colon + 1;
                    while (*value && isspace((unsigned char)*value)) value++;
                    p->h_name[p->headers] = xstrdup(name);
                    p->h_value[p->headers] = xstrdup(trim_crlf(value));
                    p->headers++;
                }
                linep = newline + 1;
            }
            free(hb);
        }
    }

    return 1;
}

void http_parser_destroy(http_parser_t *p) {
    if (!p) return;
    free((void*)p->method);
    free((void*)p->path);
    free((void*)p->version);
    for (int i = 0; i < p->headers; ++i) {
        free((void*)p->h_name[i]);
        free((void*)p->h_value[i]);
    }
    // reset state
    memset(p, 0, sizeof(*p));
}

const char *http_parser_method(http_parser_t *p) { return p->method; }
const char *http_parser_path(http_parser_t *p) { return p->path; }
const char *http_parser_version(http_parser_t *p) { return p->version; }
int http_parser_header_count(http_parser_t *p) { return p->headers; }
const char *http_parser_header_name(http_parser_t *p, int idx) { return p->h_name[idx]; }
const char *http_parser_header_value(http_parser_t *p, int idx) { return p->h_value[idx]; }
