#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_

#include "IPSocket.h"
#include "thread/Thread.h"

/**
 * \class TCPServer
 *
 * Wrapper class for a tcp server socket.
 */
class TCPServer : public IPSocket {
public:
    TCPServer(char* logbase = "/tcpserver");

    //@{
    /// System call wrapper
    int listen();
    int accept(int *fd, in_addr_t *addr, u_int16_t *port);
    //@}

    /**
     * @brief Try to accept a new connection, but don't block for more
     * than the timeout milliseconds.
     *
     * @return 0 on timeout, 1 on success, -1 on error
     */
    int timeout_accept(int *fd, in_addr_t *addr, u_int16_t *port, int timeout_ms);
};

/**
 * \class TCPServerThread
 *
 * Simple class that implements a thread of control that loops,
 * blocking on accept(), and issuing the accepted() callback when new
 * connections arrive.
 */
class TCPServerThread : public TCPServer, public Thread {
public:
    TCPServerThread(char* logbase = "/tcpserver")
        : TCPServer(logbase)
    {
    }
    
    /**
     * Virtual callback hook that gets called when new connections
     * arrive.
     */
    virtual void accepted(int fd, in_addr_t addr, u_int16_t port) = 0;

    /**
     * Loop forever, issuing blocking calls to TCPServer::accept(),
     * then calling the accepted() function when new connections
     * arrive
     * 
     * Note that unlike in the Thread base class, this run() method is
     * public in case we don't want to actually create a new thread
     * for this guy, but instead just want to run the main loop.
     */
    void run();
};
                        
#endif /* _TCP_SERVER_H_ */
