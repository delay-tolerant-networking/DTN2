
#include <errno.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/fcntl.h>

#include "IO.h"
#include "debug/Log.h"

int
IO::open(const char* path, int flags, const char* log)
{
    int fd = ::open(path, flags);
    if (log) {
        logf(log, LOG_DEBUG, "open %s (flags 0x%x): fd %d", path, flags, fd);
    }
    return fd;
}

int
IO::close(int fd, const char* log)
{
    int ret = ::close(fd);
    if (log) {
        logf(log, LOG_DEBUG, "close %d: %d", fd, ret);
    }
    return ret;
}

int
IO::read(int fd, char* bp, size_t len, const char* log)
{
    int cc = ::read(fd, (void*)bp, len);
    if (log) logf(log, LOG_DEBUG, "read %d/%d", cc, len);
    return cc;
}

int
IO::readv(int fd, const struct iovec* iov, int iovcnt, const char* log)
{
    size_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        total += iov[i].iov_len;
    }

    int cc = ::readv(fd, iov, iovcnt);
    if (log) logf(log, LOG_DEBUG, "readv %d/%d", cc, total);
    
    return cc;
}
    
int
IO::write(int fd, const char* bp, size_t len, const char* log)
{
    int cc = ::write(fd, (void*)bp, len);
    if (log) logf(log, LOG_DEBUG, "write %d/%d", cc, len);
    return cc;
}

int
IO::writev(int fd, const struct iovec* iov, int iovcnt, const char* log)
{
    size_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        total += iov[i].iov_len;
    }

    int cc = ::writev(fd, iov, iovcnt);
    if (log) logf(log, LOG_DEBUG, "writev %d/%d", cc, total);
    
    return cc;
}

int
IO::lseek(int fd, off_t offset, int whence, const char* log)
{
    int cc = ::lseek(fd, offset, whence);
    if (log) {
        logf(log, LOG_DEBUG, "lseek %u %s -> %d",
             (size_t)offset,
             (whence == SEEK_SET) ? "SEEK_SET" :
             (whence == SEEK_CUR) ? "SEEK_CUR" :
             (whence == SEEK_END) ? "SEEK_END" :
             "SEEK_INVALID",
             cc);
    }
    return cc;
}

int
IO::open(const char* path, int flags, mode_t mode, const char* log)
{
    int fd = ::open(path, flags, mode);
    if (log) {
        logf(log, LOG_DEBUG, "open %s (flags 0x%x mode 0x%x): fd %d",
             path, flags, mode, fd);
    }
    return fd;
}
    
int
IO::unlink(const char* path, const char* log)
{
    int ret = ::unlink(path);
    if (log) {
        logf(log, LOG_DEBUG, "unlink %s: %d", path, ret);
    }
    return ret;
}

int
IO::poll(int fd, int events, int* revents, int timeout_ms, const char* log)
{
    int ret;
    struct pollfd pollfd;
    
    pollfd.fd = fd;
    pollfd.events = events;
    pollfd.revents = 0;

    ret = ::poll(&pollfd, 1, timeout_ms);

    if (log)
        logf(log, LOG_DEBUG,
             "poll: events 0x%x timeout %d revents 0x%x ret %d",
             events, timeout_ms, pollfd.revents, ret);
                  
    if (ret < 0) {
        if (log && errno != EINTR)
            logf(log, LOG_ERR, "error in poll: %s", strerror(errno));
        return -1;
    }

    
    if (revents)
        *revents = pollfd.revents;
    
    return ret; // 0 or 1
}

int
IO::rwall(rw_func_t rw, int fd, char* bp, size_t len, const char* log)
{
    int cc, done = 0;
    do {
        cc = (*rw)(fd, bp, len);
        if (cc < 0) {
            if (errno == EAGAIN) continue;
            return cc;
        }
        
        if (cc == 0)
            return done;
        
        done += cc;
        bp += cc;
        len -= cc;
        
    } while (len > 0);

    return done;
}

int
IO::readall(int fd, char* bp, size_t len, const char* log)
{
    int cc = rwall(::read, fd, bp, len, log);
    if (log) logf(log, LOG_DEBUG, "readall %d/%d", cc, len);
    return cc;
}

int
IO::writeall(int fd, const char* bp, size_t len, const char* log)
{
    int cc = rwall((rw_func_t)::write, fd, (char*)bp, len, log);
    if (log) logf(log, LOG_DEBUG, "writeall %d/%d", cc, len);
    return cc;
}

int
IO::rwvall(rw_vfunc_t rw, int fd, const struct iovec* const_iov, int iovcnt,
           const char* log_func, const char* log)
{
    struct iovec* iov;
    struct iovec  static_iov[16];
    struct iovec* dynamic_iov = NULL;

    if (iovcnt <= 16) {
        iov = static_iov;
    } else {
        // maybe this shouldn't be logged at level warning, but for
        // now, keep it as such since it probably won't ever be an
        // issue and if it is, we can always demote the level later
        logf(log, LOG_WARN, "%s required to malloc since iovcnt is %d",
             log_func, iovcnt);
        dynamic_iov = (struct iovec*)malloc(sizeof(struct iovec) * iovcnt);
        iov = dynamic_iov;
    }
    
    memcpy(iov, const_iov, sizeof(struct iovec) * iovcnt);
    
    int cc, total = 0, done = 0;
    for (int i = 0; i < iovcnt; ++i) {
        total += iov[i].iov_len;
    }

    if (log) logf(log, LOG_DEBUG, "%s cnt %d, total %d",
                  log_func, iovcnt, total);
    
    while (1)
    {
        cc = (*rw)(fd, iov, iovcnt);
        if (cc < 0) {
            done = cc;
            goto done;
        }

        if (cc == 0) {
            goto done;
        }
        
        done += cc;
        total -= cc;

        if (total == 0) break; // all done
        
        /**
         * Advance iov past any completed chunks, then adjust iov_base
         * and iov_cnt for the partially completed one.
         */
        while (cc >= (int)iov[0].iov_len)
        {
            if (log) logf(log, LOG_DEBUG, "%s skipping all %d of %p", log_func,
                          iov[0].iov_len, iov[0].iov_base);
            cc -= iov[0].iov_len;
            iov++;
            iovcnt--;
        }
            
        if (log) logf(log, LOG_DEBUG, "%s skipping %d of %p -> %p", log_func,
                      cc, iov[0].iov_base, (((char*)iov[0].iov_base) + cc));
        
        iov[0].iov_base = (((char*)iov[0].iov_base) + cc);
        iov[0].iov_len  -= cc;
    }

 done:
    if (dynamic_iov != NULL)
        free(dynamic_iov);
    
    return done;
}

int
IO::readvall(int fd, const struct iovec* iov, int iovcnt,
             const char* log)
{
    return rwvall(::readv, fd, iov, iovcnt, "readvall", log);

}

int
IO::writevall(int fd, const struct iovec* iov, int iovcnt,
              const char* log)
{
    return rwvall(::writev, fd, iov, iovcnt, "writevall", log);
}

/**
 * Implement a blocking read with a timeout. This is really done by
 * first calling poll() and then calling read if there was actually
 * something to do.
 */
int
IO::timeout_read(int fd, char* bp, size_t len, int timeout_ms,
                 const char* log)
{
    ASSERT(timeout_ms >= 0);
    
    int ret = poll(fd, POLLIN | POLLPRI, NULL, timeout_ms, log);
    if (ret < 0)
        return IOERROR;

    if (ret == 0) {
        if (log) logf(log, LOG_DEBUG, "poll timed out");
        return IOTIMEOUT;
    }
    
    ASSERT(ret == 1);
    
    ret = read(fd, bp, len);
    
    if (ret < 0) {
        if (log) logf(log, LOG_ERR, "timeout_read error: %s", strerror(errno));
        return IOERROR;
    }

    if (ret == 0) {
        return IOEOF;
    }

    return ret;
}

int
IO::timeout_readv(int fd, const struct iovec* iov, int iovcnt, int timeout_ms,
                  const char* log)
{
    ASSERT(timeout_ms >= 0);
    
    int ret = poll(fd, POLLIN | POLLPRI, NULL, timeout_ms, log);
    if (ret < 0)
        return IOERROR;

    if (ret == 0) {
        if (log) logf(log, LOG_DEBUG, "poll timed out");
        return IOTIMEOUT;
    }
    
    ASSERT(ret == 1);
    
    ret = ::readv(fd, iov, iovcnt);

    if (ret < 0) {
        if (log) logf(log, LOG_ERR, "timeout_readv error: %s",
                      strerror(errno));
        return IOERROR;
    }

    if (ret == 0) {
        return IOEOF;
    }

    return ret;
}

int
IO::timeout_readall(int fd, char* bp, size_t len, int timeout_ms,
                    const char* log)
{
    ASSERT(timeout_ms >= 0);
    int ret;
    int total = 0;
    while (len > 0) {
        ret = timeout_read(fd, bp, len, timeout_ms, log);
        if (ret <= 0)
            return ret;

        total += ret;

        if (ret == (int)len) {
            return total;
            
        } else {
            bp  += ret;
            len -= ret;
        }
    }

    NOTREACHED;
}

int
IO::timeout_readvall(int fd, const struct iovec* iov, int iovcnt,
                     int timeout_ms, const char* log)
{
    // XXX/demmer 
    NOTIMPLEMENTED;
}

int
IO::set_nonblocking(int fd, bool nonblocking)
{
    int flags = 0;
    
    if ((flags = fcntl(fd, F_GETFL)) < 0) {
        return -1;
    }
    
    if (nonblocking) {
        if (flags & O_NONBLOCK) return 1; // already nonblocking
        flags = flags | O_NONBLOCK;
    } else {
        if (!(flags & O_NONBLOCK)) return 1; // already blocking
        flags = flags & ~O_NONBLOCK;
    }
    
    if (fcntl(fd, F_SETFL, flags) < 0) {
        return -1;
    }
    
    return 0;
}
