
#include "NetUtils.h"
#include "TCPServer.h"
#include "debug/Debug.h"
#include "debug/Log.h"

#include <sys/errno.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

TCPServer::TCPServer(char* logbase)
    : IPSocket(logbase, SOCK_STREAM)
{
    params_.reuseaddr_ = 1;
}

int
TCPServer::listen()
{
    logf(LOG_DEBUG, "listening");
    ASSERT(fd_ != -1);

    if (::listen(fd_, SOMAXCONN) == -1) {
        logf(LOG_ERR, "error in listen(): %s",strerror(errno));
        return -1;
    }
    
    set_state(LISTENING);
    return 0;
}
    
int
TCPServer::accept(int *fd, in_addr_t *addr, u_int16_t *port)
{
    ASSERT(state_ == LISTENING);
    
    struct sockaddr_in sa;
    socklen_t sl = sizeof(sa);
    memset(&sa, 0, sizeof(sa));
    int ret = ::accept(fd_, (sockaddr*)&sa, &sl);
    if (ret == -1) {
        if (errno != EINTR)
            logf(LOG_ERR, "error in accept(): %s", strerror(errno));
        return ret;
    }
    
    *fd = ret;
    *addr = sa.sin_addr.s_addr;
    *port = ntohs(sa.sin_port);

    return 0;
}

int
TCPServer::timeout_accept(int *fd, in_addr_t *addr, u_int16_t *port,
                          int timeout_ms)
{
    int ret = poll(POLLIN, timeout_ms);

    if (ret < 0)  return IOERROR;
    if (ret == 0) return IOTIMEOUT;
    ASSERT(ret == 1);

    ret = accept(fd, addr, port);

    if (ret < 0) {
        return IOERROR;
    }

    return 1; // accept'd
}

void
TCPServerThread::run()
{
    int fd;
    in_addr_t addr;
    u_int16_t port;

    while (1) {
        // check if someone has told us to quit by setting the
        // should_stop flag. if so, we're all done.
        if (should_stop())
            break;
        
        // block in accept waiting for new connections
        if (accept(&fd, &addr, &port) != 0) {
            if (errno == EINTR)
                continue;

            logf(LOG_ERR, "error in accept(): %d %s", errno, strerror(errno));
            close();
            break;
        }
        
        logf(LOG_DEBUG, "accepted connection fd %d from %s:%d",
             fd, intoa(addr), port);

        accepted(fd, addr, port);
    }
}
