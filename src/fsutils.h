#ifndef FSUTILS_H
#define FSUTILS_H

#include <stddef.h>

/* Safely join base dir and request path, preventing directory traversal.
 * - base: directory to serve (e.g. "www")
 * - reqpath: URL path from request (e.g. "/", "/index.html", "/a/b")
 * - outbuf/outlen: output absolute path
 * Returns 0 on success, -1 on error (invalid path, outside base, or buffer too small).
 */
int safe_resolve_path(const char *base, const char *reqpath, char *outbuf, size_t outlen);

#endif
