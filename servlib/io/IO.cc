
#include <errno.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/fcntl.h>

#include "IO.h"
#include "debug/Log.h"

int
IO::read(int fd, char* bp, int len, const char* log)
{
    int cc = ::read(fd, (void*)bp, len);
    if (log) logf(log, LOG_DEBUG, "read %d/%d", cc, len);
    return cc;
}

int
IO::readv(int fd, struct iovec* iov, int iovcnt, const char* log)
{
    int total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        total += iov[i].iov_len;
    }

    int cc = ::readv(fd, iov, iovcnt);
    if (log) logf(log, LOG_DEBUG, "readv %d/%d", cc, total);
    
    return cc;
}
    
int
IO::write(int fd, const char* bp, int len, const char* log)
{
    int cc = ::write(fd, (void*)bp, len);
    if (log) logf(log, LOG_DEBUG, "write %d/%d", cc, len);
    return cc;
}

int
IO::writev(int fd, struct iovec* iov, int iovcnt, const char* log)
{
    int total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        total += iov[i].iov_len;
    }

    int cc = ::writev(fd, iov, iovcnt);
    if (log) logf(log, LOG_DEBUG, "writev %d/%d", cc, total);
    
    return cc;
}

int
IO::poll(int fd, int events, int timeout_ms, const char* log)
{
    int ret;
    struct pollfd pollfd;
    
    pollfd.fd = fd;
    pollfd.events = events;
    pollfd.revents = 0;

    ret = ::poll(&pollfd, 1, timeout_ms);
    if (ret < 0) {
        if (log) logf(log, LOG_ERR, "error in poll: %s", strerror(errno));
        return -1;
    }
    
    if ((pollfd.revents & POLLERR) || (pollfd.revents & POLLNVAL)) {
        if (log) logf(log, LOG_ERR, "poll returned error event");
        return -1;
    }
    
    return ret; // 0 or 1
}

int
IO::writeall(int fd, const char* bp, int len,
             const char* log)
{
    int cc, done = 0;
    do {
        cc = ::write(fd, bp, len);
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
IO::writevall(int fd, struct iovec* iov, int iovcnt,
              const char* log)
{
    int cc, total = 0, done = 0;
    for (int i = 0; i < iovcnt; ++i) {
        total += iov[i].iov_len;
    }

    if (log) logf(log, LOG_DEBUG, "writevall cnt %d, total %d", iovcnt, total);
    
    while (1)
    {
        cc = ::writev(fd, iov, iovcnt);
        if (cc < 0) {
            if (errno == EAGAIN) continue;
            return cc;
        }

        if (cc == 0)
            return done;
        
        done += cc;
        total -= cc;

        if (total == 0) break; // all done
        
        /**
         * Advance iov past any completed chunks, then adjust iov_base
         * and iov_cnt for the partially completed one.
         */
        while (cc >= (int)iov[0].iov_len)
        {
            if (log) logf(log, LOG_DEBUG, "writevall skipping all %d of %p",
                 iov[0].iov_len, iov[0].iov_base);
            cc -= iov[0].iov_len;
            iov++;
            iovcnt--;
        }
            
        if (log) logf(log, LOG_DEBUG, "writevall skipping %d of %p -> %p",
             cc, iov[0].iov_base, (((char*)iov[0].iov_base) + cc));
             
        iov[0].iov_base = (((char*)iov[0].iov_base) + cc);
        iov[0].iov_len  -= cc;
    }
    
    return done;
}

/**
 * Implement a blocking read with a timeout. This is really done by
 * first calling poll() and then calling read if there was actually
 * something to do.
 */
int
IO::timeout_read(int fd, char* bp, int len, int timeout_ms,
                 const char* log)
{
    ASSERT(timeout_ms > 0);
    
    int ret = poll(fd, POLLIN | POLLPRI, timeout_ms);
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
IO::timeout_readv(int fd, struct iovec* iov, int iovcnt, int timeout_ms,
                  const char* log)
{
    ASSERT(timeout_ms > 0);
    
    int ret = poll(fd, POLLIN | POLLPRI, timeout_ms);
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
