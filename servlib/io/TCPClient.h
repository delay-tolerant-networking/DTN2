#ifndef _TCP_CLIENT_H_
#define _TCP_CLIENT_H_

#include "IPClient.h"

/**
 * Wrapper class for a tcp client socket.
 */
class TCPClient : public IPClient {
public:
    TCPClient(const char* logbase = "/tcpclient");
    TCPClient(int fd, in_addr_t remote_addr, u_int16_t remote_port,
              const char* logbase = "/tcpclient");

    /**
     * System call wrapper
     */
    virtual int connect(in_addr_t remote_addr, u_int16_t remote_port);

    /**
     * Try to connect to the remote host, but don't block for more
     * than timeout milliseconds. If there was an error (either
     * immediate or delayed), return it in *errp.
     *
     * @return 0 on success, IOERROR on error, and IOTIMEOUT on
     * timeout.
     */
    virtual int timeout_connect(in_addr_t remote_attr, u_int16_t remote_port,
                                int timeout_ms, int* errp = 0);
    
    
protected:
    int internal_connect(in_addr_t remote_attr, u_int16_t remote_port);
};

#endif /* _TCP_CLIENT_H_ */
