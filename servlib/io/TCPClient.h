#ifndef _TCP_CLIENT_H_
#define _TCP_CLIENT_H_

#include "IPSocket.h"
#include "IOClient.h"

/**
 * Wrapper class for a tcp client socket.
 */
class TCPClient : public IPSocket, public IOClient {
public:
    TCPClient(const char* logbase = "/tcpclient");
    TCPClient(int fd, in_addr_t remote_addr, u_int16_t remote_port,
              const char* logbase = "/tcpclient");

    //@{
    /// System call wrapper
    virtual int connect(in_addr_t remote_addr, u_int16_t remote_port);
    //@}

    //@{
    /// Virtual functions from IOClient interface
    virtual int read(char* bp, size_t len);
    virtual int readv(const struct iovec* iov, int iovcnt);
    virtual int write(const char* bp, size_t len);
    virtual int writev(const struct iovec* iov, int iovcnt);

    /// Read or write out the entire supplied buffer, potentially
    /// requiring multiple calls to write().
    virtual int readall(char* bp, size_t len);
    virtual int writeall(const char* bp, size_t len);

    /// Similar function for iovec
    virtual int readvall(const struct iovec* iov, int iovcnt);
    virtual int writevall(const struct iovec* iov, int iovcnt);
    //@}

    //@{
    /**
     * @brief Try to read the specified number of bytes, but don't
     * block for more than timeout milliseconds.
     *
     * @return the number of bytes read or the appropriate
     * IOTimeoutReturn_t code
     */
    virtual int timeout_read(char* bp, size_t len, int timeout_ms);
    virtual int timeout_readv(const struct iovec* iov, int iovcnt, int timeout_ms);
    //@}

    //@{
    /**
     * @brief Try to connect to the remote host, but don't block for
     * more than timeout milliseconds. If there was an error (either
     * immediate or delayed), return it in *errp.
     *
     * @return 0 on success, IOERROR on error, and IOTIMEOUT on
     * timeout.
     */
    virtual int timeout_connect(in_addr_t remote_attr, u_int16_t remote_port,
                                int timeout_ms, int* errp = 0);
    //@}
    
protected:
    int internal_connect(in_addr_t remote_attr, u_int16_t remote_port);
};

#endif /* _TCP_CLIENT_H_ */
