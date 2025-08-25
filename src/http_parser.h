#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stddef.h>

/* Max sizes (tuning knobs for safety) */
#define HTTPP_MAX_HEADERS 32
#define HTTPP_MAX_BUF 4096

typedef struct http_parser_s {
	char buf[HTTPP_MAX_BUF];
	size_t buflen;
	int headers;
	const char *method;
	const char *path;
	const char *version;
	const char *h_name[HTTPP_MAX_HEADERS];
	const char *h_value[HTTPP_MAX_HEADERS];
} http_parser_t;

/* Initialize parser */
void http_parser_init(http_parser_t *p);

/* Free any allocations inside parser */
void http_parser_destroy(http_parser_t *p);

/*
 * Feed data into the parser incrementally.
 * - data/len: incoming bytes
 * - consumed: out param set to number of bytes consumed from the input
 * Returns:
 *  0 => need more data
 *  1 => request parsed successfully (headers complete)
 * -1 => parse error
 */
int http_parser_execute(http_parser_t *p, const char *data, size_t len, size_t *consumed);

/* After successful parse, the following accessors are valid */
const char *http_parser_method(http_parser_t *p);
const char *http_parser_path(http_parser_t *p);
const char *http_parser_version(http_parser_t *p);
int http_parser_header_count(http_parser_t *p);
const char *http_parser_header_name(http_parser_t *p, int idx);
const char *http_parser_header_value(http_parser_t *p, int idx);

/* header end */
#endif
