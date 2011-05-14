#include "util.h"
#include "event.h"

#define BUF_SIZE 655350

struct rw_ctx {
  struct event *e;
  struct rwbuffer *rbuf;
  struct rwbuffer *wbuf;
};

int rw_handler(struct event *e, uint32_t events) {
  int size;
  struct rw_ctx *ctx = e->ctx;
  if (events & EPOLLIN) {
    if ((size = rwb_size_to_write(ctx->rbuf)) > 0) {
      if ((size = read(e->fd, rwb_write_buf(ctx->rbuf), size)) > 0) {
        rwb_write_size(ctx->rbuf, size);
        event_mod(ctx->e, ctx->e->events | EPOLLOUT);
      } else if (size == 0) {
        event_del(e);
        event_del(ctx->e);
      }
    }
  }

  if (events & EPOLLOUT) {
    if ((size = rwb_size_to_read(ctx->wbuf)) > 0) {
      if ((size = write(e->fd, rwb_read_buf(ctx->wbuf), size)) > 0) {
        if (size == rwb_size_to_read(ctx->wbuf)) {
          event_mod(e, e->events);
        }
        rwb_read_size(ctx->wbuf, size);
      }
    }
  }

  if (events & (EPOLLHUP | EPOLLHUP)) {
    tp_log("error[%d]", e->fd);
    event_del(e);
    event_del(ctx->e);
  }

  return 0;
}

int accept_handler(struct event *e, uint32_t events) {
  int  fd1, fd2;
  uint32_t size;
  struct sockaddr_in addr;

  if ((fd1 = accept(e->fd, (struct sockaddr*)&addr, &size)) != -1) {
    struct event *e1 = malloc(sizeof(struct event));
    struct event *e2 = malloc(sizeof(struct event));
    struct rw_ctx *ctx1 = malloc(sizeof(struct rw_ctx));
    struct rw_ctx *ctx2 = malloc(sizeof(struct rw_ctx));
    struct rwbuffer *buf1 = rwb_new(BUF_SIZE);
    struct rwbuffer *buf2 = rwb_new(BUF_SIZE);

    if ((fd2 = connect_addr("127.0.0.1", 11211)) == -1) {
      tp_log("connect failed: %s", strerror(errno));
    }

    ctx1->e = e2;
    ctx1->rbuf = buf1;
    ctx1->wbuf = buf2;
    e1->fd = fd1;
    e1->ctx = ctx1;
    e1->events = EPOLLIN | EPOLLHUP | EPOLLERR;
    e1->handler = rw_handler;
    event_add(e1);

    ctx2->e = e1;
    ctx2->rbuf = buf2;
    ctx2->wbuf = buf1;
    e2->fd = fd2;
    e2->ctx = ctx2;
    e2->events = EPOLLIN | EPOLLHUP | EPOLLERR;
    e2->handler = rw_handler;
    event_add(e2);
  }

  return 0;
}

int main(int argc, char **argv) {
  int fd;
  struct event *e = malloc(sizeof(struct event));

  event_init();

  fd = bind_addr("any", 11212);

  tp_log("lfd: %d", fd);

  e->fd = fd;
  e->events = EPOLLIN | EPOLLHUP | EPOLLERR;
  e->handler = accept_handler;
  event_add(e);

  while (1) {
    if (!process_event()) {
      //do house keeping
      tp_log("no event");
    }
  }

  return 0;
}

