#ifndef tier_tcp_client_h
#define tier_tcp_client_h

#include "TcpSocket.h"
#include "lib/IOClient.h"

/**
 * Wrapper class for a tcp client socket.
 */
class TcpClient : public TcpSocket, public IOClient {
public:
    TcpClient(const char* logbase = "/tcpclient");
    TcpClient(int fd, in_addr_t local_addr, u_int16_t local_port,
              const char* logbase = "/tcpclient");

    //@{
    /// System call wrapper
    virtual int connect(in_addr_t remote_addr, u_int16_t remote_port);
    //@}

    //@{
    /// Virtual functions from IOClient interface
    virtual int read(char* bp, int len);
    virtual int readv(struct iovec* iov, int iovcnt);
    virtual int write(const char* bp, int len);
    virtual int writev(struct iovec* iov, int iovcnt);

    /// Write out the entire supplied buffer, potentially
    /// requiring multiple calls to write().
    virtual int writeall(const char* bp, int len);

    /// Similar function for iovec
    virtual int writevall(struct iovec* iov, int iovcnt);
    //@}

    //@{
    /**
     * @brief Try to read the specified number of bytes, but don't
     * block for more than timeout milliseconds.
     *
     * @return the number of bytes read or the appropriate
     * IOTimeoutReturn_t code
     */
    virtual int timeout_read(char* bp, int len, int timeout_ms);
    virtual int timeout_readv(struct iovec* iov, int iovcnt, int timeout_ms);
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

#endif /* tier_tcp_client_h */
