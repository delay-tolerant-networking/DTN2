#ifndef _IP_CLIENT_H_
#define _IP_CLIENT_H_

#include "IPSocket.h"
#include "IOClient.h"

/**
 * Base class that unifies the IPSocket and IOClient interfaces. Both
 * TCPClient and UDPClient derive from IPClient.
 */
class IPClient : public IPSocket, public IOClient {
public:
    IPClient(const char* logbase, int socktype);
    IPClient(int fd, in_addr_t remote_addr, u_int16_t remote_port,
             const char* logbase);
    virtual ~IPClient();
    
    // Virtual from IOClient
    virtual int read(char* bp, size_t len);
    virtual int write(const char* bp, size_t len);
    virtual int readv(const struct iovec* iov, int iovcnt);
    virtual int writev(const struct iovec* iov, int iovcnt);
    
    virtual int readall(char* bp, size_t len);
    virtual int writeall(const char* bp, size_t len);
    virtual int readvall(const struct iovec* iov, int iovcnt);
    virtual int writevall(const struct iovec* iov, int iovcnt);
    
    virtual int timeout_read(char* bp, size_t len, int timeout_ms);
    virtual int timeout_readv(const struct iovec* iov, int iovcnt,
                              int timeout_ms);
    virtual int timeout_readall(char* bp, size_t len, int timeout_ms);
    virtual int timeout_readvall(const struct iovec* iov, int iovcnt,
                                 int timeout_ms);
};

#endif /* _IP_CLIENT_H_ */
