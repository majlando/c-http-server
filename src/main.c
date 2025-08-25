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
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(void) {
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

    printf("Listening on 0.0.0.0:8080 (epoll)\n");

    struct epoll_event events[64];

    for (;;) {
        int n = epoll_wait(epfd, events, 64, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
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

                    struct epoll_event cev;
                    cev.events = EPOLLIN | EPOLLET;
                    cev.data.fd = client;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client, &cev) < 0) {
                        perror("epoll_ctl: client add");
                        close(client);
                    }
                }
            } else {
                // handle client read
                int client = fd;
                char buf[2048];
                ssize_t r;
                int closed = 0;

                for (;;) {
                    r = recv(client, buf, sizeof(buf), 0);
                    if (r > 0) {
                        // keep reading until EAGAIN; this toy server ignores request contents
                        continue;
                    } else if (r == 0) {
                        closed = 1; // peer closed
                        break;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        // error
                        closed = 1;
                        break;
                    }
                }

                // write response (ignore partial writes for this minimal server)
                ssize_t s = send(client, response, strlen(response), 0);
                (void)s;

                epoll_ctl(epfd, EPOLL_CTL_DEL, client, NULL);
                close(client);
            }
        }
    }

    close(epfd);
    close(listen_fd);
    return 0;
}
