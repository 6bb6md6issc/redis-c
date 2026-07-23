#include <netinet/ip.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <stdint.h>
#include <vector>
#include <unistd.h>
// #include <arpa/inet.h>

typedef std::vector<uint8_t> Buffer;

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void msg_errno(const char *msg) {
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

static void die(const char *msg) {
    fprintf(stderr, "[%d] %s\n", errno, msg);
    abort();
}

struct Conn {
  int fd = -1;
  bool want_read = false;
  bool want_write = false;
  bool want_close = false;

  Buffer incoming;
  Buffer outgoing;
};

static void fd_set_non_block(int fd) {
  errno = 0;
  int flags = fcntl(fd, F_GETFL, 0);
  if (errno) {
    die("fcntl get error");
  }
  flags |= O_NONBLOCK;

  fcntl(fd, F_SETFL, 0);
  if (errno) {
    die("fcntl error");
  }
}

static Conn* handle_accept(int fd) {
  struct sockaddr_in client_addr = {};
  socklen_t socklen = sizeof(client_addr);
  int connfd = accept(fd, (struct sockaddr*) &client_addr, &socklen);
  if (connfd < 0) {
    msg_errno("accept");
    return NULL;
  }

  uint32_t ip = client_addr.sin_addr.s_addr;
    fprintf(stderr, "new client from %u.%u.%u.%u:%u\n",
        ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24,
        ntohs(client_addr.sin_port)
    );

  fd_set_non_block(connfd);
  Conn* conn = new Conn();
  conn->fd = connfd;
  conn->want_read = true;
  return conn;
}


int main() {
  // ipv4 and tcp
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    die("socket()");
  }
  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
  
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohl(0);

  int rv = bind(fd, (const sockaddr*) &addr, sizeof(addr));
  if (rv < 0) {
    die("bind()");
  }
  fd_set_non_block(fd);

  rv = listen(fd, SOMAXCONN);
  if (rv < 0) {
    die("listen");
  }

  std::vector<Conn*> fd2conn;
  std::vector<struct pollfd> poll_args;
  
  while (true) {
    poll_args.clear();
    struct pollfd pfd = {fd, POLLIN, 0};
    poll_args.push_back(pfd);

    for (Conn* conn: fd2conn) {
      if (!conn){
        continue;
      }
      struct pollfd pfd = {conn->fd, POLLERR, 0};
      if (conn->want_read) {
        pfd.events |= POLLIN;
      }
      if (conn->want_write) {
        pfd.events |= POLLOUT;
      }
      poll_args.push_back(pfd);
    }

    int rv = poll(poll_args.data(), (nfds_t) poll_args.size(), -1);
    if (rv < 0 && errno == EINTR) {
      continue;
    }
    if (rv < 0) {
      die("poll()");
    }

    // accept new client and resize if needed
    if (poll_args[0].revents != 0) {
      Conn* conn = handle_accept(poll_args[0].fd);
      if (conn && (size_t) conn->fd > poll_args.size()){
        poll_args.resize(conn->fd + 1);
      }
      fd2conn[conn->fd] = conn;
    }


    for (size_t i = 1; i < poll_args.size(); i++) {
      uint32_t ready = pfd.revents;
      if (!ready) {
        continue;
      }
      Conn* conn = fd2conn[poll_args[i].fd];
      if (ready & POLLIN) {
        conn->want_read = true;
      }
      if (ready & POLLOUT) {
        conn->want_write = true;
      }
      if (ready & POLLERR || conn->want_close) {
        close(conn->fd);
        fd2conn[conn->fd] = NULL;
        delete conn;
      }
    }
  }
  return 0;
}