#ifndef tier_tcp_server_h
#define tier_tcp_server_h

#include "TcpSocket.h"
#include "lib/Thread.h"

/**
 * \class TcpServer
 *
 * Wrapper class for a tcp server socket.
 */
class TcpServer : public TcpSocket {
public:
    TcpServer(char* logbase = "/tcpserver");

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
 * \class TcpServerThread
 *
 * Simple class that implements a thread of control that loops,
 * blocking on accept(), and issuing the accepted() callback when new
 * connections arrive.
 */
class TcpServerThread : public TcpServer, public Thread {
public:
    TcpServerThread(char* logbase)
        : TcpServer(logbase), stop_(false)
    {
    }

    /**
     * @brief Virtual callback hook that gets called when new
     * connections arrive.
     */
    virtual void accepted(int fd, in_addr_t addr, u_int16_t port) = 0;

    /**
     * @brief Loop forever, issuing blocking calls to
     * TcpServer::accept(), then calling the accepted() function when
     * new connections arrive
     * 
     * Note that unlike in the Thread base class, this run() method is
     * public in case we don't want to actually create a new thread
     * for this guy, but instead just want to run the main loop.
     */
    void run();

    /**
     * @brief Stop the server loop.
     *
     * This simply sets a flag that's examined after each call to
     * accepted(), meaning that it won't interrupt the thread if it's
     * blocked in the kernel.
     */
    void stop() { stop_ = true; }

protected:
    bool stop_;
};
                        

#endif /* tier_tcp_server_h */
