#ifndef _UDP_SERVER_H_
#define _UDP_SERVER_H_

#include "IPSocket.h"
#include "thread/Thread.h"

/**
 * \class UDPServer
 *
 * Wrapper class for a udp server socket.
 */
class UDPServer : public IPSocket {
public:
    UDPServer(char* logbase = "/udpserver");

    //@{
    /// System call wrapper
      int RcvMessage(int *fd, in_addr_t *addr, u_int16_t *port, 
		     char** pt_payload, size_t* payload_len);
    //@}

    /**
     * @brief Try to accept a new connection, but don't block for more
     * than the timeout milliseconds.
     *
     * @return 0 on timeout, 1 on success, -1 on error
     */
      int timeout_RcvMessage(int *fd, in_addr_t *addr, u_int16_t *port, char** pt_payload,
			     size_t* payload_len, int timeout_ms);
};

/**
 * \class UDPServerThread
 *
 * Simple class that implements a thread of control that loops,
 * blocking on RcvFrom(), and issuing the ProcessData() callback when new
 * data asynchronously arrives.
 */
class UDPServerThread : public UDPServer, public Thread {
public:
    UDPServerThread(char* logbase = "/udpserver")
        : UDPServer(logbase)
    {
    } 
    
    /**
     * Virtual callback hook that gets called when new connections
     * arrive.
     */
     virtual void ProcessData(char* payload, size_t payload_len) = 0;

    /**
     * Loop forever, issuing blocking calls to UDPServer::accept(),
     * then calling the accepted() function when new connections
     * arrive
     * 
     * Note that unlike in the Thread base class, this run() method is
     * public in case we don't want to actually create a new thread
     * for this guy, but instead just want to run the main loop.
     */
    void run();
};
                        
#endif /* _UDP_SERVER_H_ */
