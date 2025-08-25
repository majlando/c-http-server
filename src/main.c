// Minimal epoll-based non-blocking HTTP server (toy)
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "http_parser.h"
#include "fsutils.h"
#include <signal.h>
#include <sys/stat.h>
#include <limits.h>

typedef struct connection_s {
    int fd;
    http_parser_t parser;
    char buf[8192];
    size_t buflen;
    /* outgoing write buffer for partial writes */
    char *wbuf;
    size_t wlen; /* total length of wbuf */
    size_t woff; /* bytes already written */
    int should_close; /* close after write completes */
} connection_t;

static volatile sig_atomic_t running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

static const char *response =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain; charset=utf-8\r\n"
    "Content-Length: 13\r\n"
    "Connection: close\r\n"
    "\r\n"
    "Hello, world!";

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return -1;
    return 0;
}

/* Simple MIME mapping based on file extension */
static const char *mime_type_for_path(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "text/plain; charset=utf-8";
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css") == 0) return "text/css; charset=utf-8";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    return "application/octet-stream";
}

/* path resolution moved to src/fsutils.c */

int main(void) {
    // open simple logfile
    FILE *logf = fopen("server.log", "a");
    if (!logf) logf = stderr;

    // handle signals for graceful shutdown
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    /* avoid SIGPIPE killing the process on write to closed socket */
    signal(SIGPIPE, SIG_IGN);
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 128) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    if (set_nonblocking(listen_fd) < 0) {
        perror("set_nonblocking");
        close(listen_fd);
        return 1;
    }

    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        close(listen_fd);
        return 1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("epoll_ctl: listen_fd");
        close(listen_fd);
        close(epfd);
        return 1;
    }

    fprintf(logf, "Listening on 0.0.0.0:8080 (epoll)\n");
    fflush(logf);

    struct epoll_event events[64];
    while (running) {
        int n = epoll_wait(epfd, events, 64, -1);
        if (n < 0) {
            if (errno == EINTR) {
                if (!running) break;
                continue;
            }
            perror("epoll_wait");
            break;
        }
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == listen_fd) {
                // accept loop
                for (;;) {
                    int client = accept(listen_fd, NULL, NULL);
                    if (client < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept");
                        break;
                    }
                    if (set_nonblocking(client) < 0) {
                        close(client);
                        continue;
                    }
                    connection_t *conn = calloc(1, sizeof(connection_t));
                    if (!conn) {
                        close(client);
                        continue;
                    }
                    conn->fd = client;
                    http_parser_init(&conn->parser);
                    conn->buflen = 0;
                    fprintf(logf, "accepted fd=%d\n", client);
                    fflush(logf);
                    struct epoll_event cev;
                    /* use level-triggering for simplicity; edge-triggering requires careful draining */
                    cev.events = EPOLLIN;
                    cev.data.ptr = conn;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client, &cev) < 0) {
                        perror("epoll_ctl: client add");
                        close(client);
                        free(conn);
                    }
                }
            } else {
                // handle client read
                connection_t *conn = (connection_t *)events[i].data.ptr;
                if (!conn) continue;
                int client = conn->fd;
                /* if socket is writable, try to flush pending write buffer */
                if (events[i].events & EPOLLOUT) {
                    while (conn->woff < conn->wlen) {
                        ssize_t s = send(client, conn->wbuf + conn->woff, conn->wlen - conn->woff, 0);
                        if (s > 0) {
                            conn->woff += (size_t)s;
                            continue;
                        }
                        if (s < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
                        /* on error, drop connection */
                        conn->should_close = 1;
                        break;
                    }
                    if (conn->woff == conn->wlen) {
                        free(conn->wbuf);
                        conn->wbuf = NULL;
                        conn->wlen = conn->woff = 0;
                        /* remove EPOLLOUT interest */
                        struct epoll_event mev;
                        mev.events = EPOLLIN;
                        mev.data.ptr = conn;
                        epoll_ctl(epfd, EPOLL_CTL_MOD, client, &mev);
                        if (conn->should_close) {
                            epoll_ctl(epfd, EPOLL_CTL_DEL, client, NULL);
                            http_parser_destroy(&conn->parser);
                            close(client);
                            free(conn);
                            continue;
                        }
                    }
                }
                ssize_t r = 0;
                int done = 0;
                for (;;) {
                    r = recv(client, conn->buf + conn->buflen, sizeof(conn->buf) - conn->buflen - 1, 0);
                    if (r > 0) {
                        conn->buflen += r;
                        if (conn->buflen >= sizeof(conn->buf) - 1) break;
                        continue;
                    } else if (r == 0) {
                        done = 1; // peer closed
                        break;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        done = 1;
                        break;
                    }
                }
                conn->buf[conn->buflen] = '\0';
                size_t consumed = 0;
                int pres = http_parser_execute(&conn->parser, conn->buf, conn->buflen, &consumed);
                if (pres == 1) {
                    // Serve static files for GET, otherwise respond with hello
                    const char *method = http_parser_method(&conn->parser) ?: "";
                    const char *path = http_parser_path(&conn->parser) ?: "/";
                    /* determine whether the client requested to close the connection */
                    int req_close = 0;
                    const char *ver = http_parser_version(&conn->parser) ?: "";
                    int hcount = http_parser_header_count(&conn->parser);
                    for (int hi = 0; hi < hcount; ++hi) {
                        const char *hn = http_parser_header_name(&conn->parser, hi);
                        const char *hv = http_parser_header_value(&conn->parser, hi);
                        if (hn && hv && strcasecmp(hn, "Connection") == 0) {
                            if (strcasecmp(hv, "close") == 0) req_close = 1;
                            if (strcasecmp(hv, "keep-alive") == 0) req_close = 0;
                        }
                    }
                    /* HTTP/1.0 defaults to close unless 'Connection: keep-alive' present */
                    if (strncmp(ver, "HTTP/1.0", 8) == 0 && hcount == 0) req_close = 1;
                    /* set per-connection close flag according to request */
                    conn->should_close = req_close;
                    if (strcmp(method, "GET") == 0) {
                        char fullpath[PATH_MAX];
                        if (safe_resolve_path("www", path, fullpath, sizeof(fullpath)) == 0) {
                            struct stat st;
                            if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode)) {
                                FILE *f = fopen(fullpath, "rb");
                                if (f) {
                                    size_t sz = (size_t)st.st_size;
                                    char *buf = malloc(sz);
                                    if (buf && fread(buf, 1, sz, f) == sz) {
                                        const char *ctype = mime_type_for_path(fullpath);
                                        char hdr[256];
                                        const char *connval = conn->should_close ? "close" : "keep-alive";
                                        int hlen = snprintf(hdr, sizeof(hdr), "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: %s\r\n\r\n", ctype, sz, connval);
                                        /* send header+body using robust writer that handles partial writes */
                                        /* try header then body; if queued, mark should_close and let EPOLLOUT flush */
                                        size_t total_hdr = (size_t)hlen;
                                        /* attempt to send header */
                                        ssize_t hs = send(client, hdr, total_hdr, 0);
                                        if (hs < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                                            /* fatal send error */
                                            free(buf);
                                            fclose(f);
                                            done = 1;
                                        } else {
                                            size_t sent_hdr = hs > 0 ? (size_t)hs : 0;
                                            if (sent_hdr < total_hdr) {
                                                /* queue remaining header + body */
                                                size_t rem = total_hdr - sent_hdr + sz;
                                                char *w = malloc(rem);
                                                if (w) {
                                                    memcpy(w, hdr + sent_hdr, total_hdr - sent_hdr);
                                                    memcpy(w + (total_hdr - sent_hdr), buf, sz);
                                                    free(buf);
                                                    fclose(f);
                                                    conn->wbuf = w;
                                                    conn->wlen = rem;
                                                    conn->woff = 0;
                                                    conn->should_close = 1;
                                                    /* enable EPOLLOUT */
                                                    struct epoll_event mev;
                                                    mev.events = EPOLLIN | EPOLLOUT;
                                                    mev.data.ptr = conn;
                                                    epoll_ctl(epfd, EPOLL_CTL_MOD, client, &mev);
                                                } else {
                                                    free(buf);
                                                    fclose(f);
                                                }
                                            } else {
                                                /* header fully sent, now send body */
                                                ssize_t bs = send(client, buf, sz, 0);
                                                if (bs < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                                                    free(buf);
                                                    fclose(f);
                                                    done = 1;
                                                } else if (bs < (ssize_t)sz) {
                                                    size_t rem = (size_t)sz - (bs > 0 ? (size_t)bs : 0);
                                                    char *w = malloc(rem);
                                                    if (w) {
                                                        size_t sentb = bs > 0 ? (size_t)bs : 0;
                                                        memcpy(w, buf + sentb, rem);
                                                        free(buf);
                                                        fclose(f);
                                                        conn->wbuf = w;
                                                        conn->wlen = rem;
                                                        conn->woff = 0;
                                                        conn->should_close = 1;
                                                        struct epoll_event mev;
                                                        mev.events = EPOLLIN | EPOLLOUT;
                                                        mev.data.ptr = conn;
                                                        epoll_ctl(epfd, EPOLL_CTL_MOD, client, &mev);
                                                    } else {
                                                        free(buf);
                                                        fclose(f);
                                                    }
                                                } else {
                                                    free(buf);
                                                    fclose(f);
                                                    if (conn->should_close) done = 1;
                                                    else {
                                                        /* prepare for next request on this connection */
                                                        http_parser_destroy(&conn->parser);
                                                        http_parser_init(&conn->parser);
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        free(buf);
                                        fclose(f);
                                        if (conn->should_close) done = 1;
                                        else {
                                            http_parser_destroy(&conn->parser);
                                            http_parser_init(&conn->parser);
                                        }
                                    }
                            }
                        }
                    }
                    if (!done) {
                        const char *resp = response;
                        size_t rlen = strlen(resp);
                        /* prepare response header with appropriate Connection value */
                        const char *connval = conn->should_close ? "close" : "keep-alive";
                        char hdr[256];
                        int hlen = snprintf(hdr, sizeof(hdr), "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %zu\r\nConnection: %s\r\n\r\n", rlen, connval);
                        /* send header */
                        ssize_t hs = send(client, hdr, (size_t)hlen, 0);
                        if (hs < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                            done = 1;
                        } else {
                            size_t sent_hdr = hs > 0 ? (size_t)hs : 0;
                            if (sent_hdr < (size_t)hlen) {
                                /* queue remaining header + body */
                                size_t rem = (size_t)hlen - sent_hdr + rlen;
                                char *w = malloc(rem);
                                if (w) {
                                    memcpy(w, hdr + sent_hdr, (size_t)hlen - sent_hdr);
                                    memcpy(w + ((size_t)hlen - sent_hdr), resp, rlen);
                                    conn->wbuf = w;
                                    conn->wlen = rem;
                                    conn->woff = 0;
                                    /* ensure EPOLLOUT is enabled */
                                    struct epoll_event mev;
                                    mev.events = EPOLLIN | EPOLLOUT;
                                    mev.data.ptr = conn;
                                    epoll_ctl(epfd, EPOLL_CTL_MOD, client, &mev);
                                } else {
                                    done = 1;
                                }
                            } else {
                                /* header fully sent -> send body */
                                ssize_t bs = send(client, resp, rlen, 0);
                                if (bs < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                                    done = 1;
                                } else if (bs < (ssize_t)rlen) {
                                    size_t rem = rlen - (bs > 0 ? (size_t)bs : 0);
                                    char *w = malloc(rem);
                                    if (w) {
                                        size_t sentb = bs > 0 ? (size_t)bs : 0;
                                        memcpy(w, resp + sentb, rem);
                                        conn->wbuf = w;
                                        conn->wlen = rem;
                                        conn->woff = 0;
                                        struct epoll_event mev;
                                        mev.events = EPOLLIN | EPOLLOUT;
                                        mev.data.ptr = conn;
                                        epoll_ctl(epfd, EPOLL_CTL_MOD, client, &mev);
                                    } else {
                                        done = 1;
                                    }
                                } else {
                                    /* fully sent */
                                    if (conn->should_close) done = 1;
                                    else {
                                        http_parser_destroy(&conn->parser);
                                        http_parser_init(&conn->parser);
                                    }
                                }
                            }
                        }
                    }
                    fprintf(logf, "served fd=%d %s %s\n", client, method, path);
                    fflush(logf);
                } else if (pres == -1) {
                    const char *err = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
                    /* try to send error; if partial, queue and mark close */
                    size_t elen = strlen(err);
                    ssize_t es = send(client, err, elen, 0);
                    if (es < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                        done = 1;
                    } else if (es < (ssize_t)elen) {
                        size_t rem = elen - (es > 0 ? (size_t)es : 0);
                        char *w = malloc(rem);
                        if (w) {
                            size_t sent = es > 0 ? (size_t)es : 0;
                            memcpy(w, err + sent, rem);
                            conn->wbuf = w;
                            conn->wlen = rem;
                            conn->woff = 0;
                            conn->should_close = 1;
                            struct epoll_event mev;
                            mev.events = EPOLLIN | EPOLLOUT;
                            mev.data.ptr = conn;
                            epoll_ctl(epfd, EPOLL_CTL_MOD, client, &mev);
                        } else {
                            done = 1;
                        }
                    } else {
                        done = 1;
                    }
                    fprintf(logf, "bad request fd=%d\n", client);
                    fflush(logf);
                }
                // Remove consumed bytes from buffer
                if (consumed > 0 && consumed <= conn->buflen) {
                    memmove(conn->buf, conn->buf + consumed, conn->buflen - consumed);
                    conn->buflen -= consumed;
                }
                if (done) {
                    /* if there is pending write buffered, leave connection open until flushed */
                    if (conn->wbuf && conn->wlen > conn->woff) {
                        conn->should_close = 1;
                        /* ensure EPOLLOUT interest is set (already done when queued above) */
                    } else {
                        epoll_ctl(epfd, EPOLL_CTL_DEL, client, NULL);
                        http_parser_destroy(&conn->parser);
                        close(client);
                        free(conn);
                    }
                }
            }
        }
    }

    // shutdown sequence
    fprintf(logf, "shutting down\n");
    fflush(logf);

    close(epfd);
    close(listen_fd);
    if (logf && logf != stderr) fclose(logf);
    return 0;
}



}


