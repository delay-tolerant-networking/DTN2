#ifndef __IOCLIENT_H__
#define __IOCLIENT_H__

struct iovec;

/*!
 * Abstract interface for stream type output channels.
 */
class IOClient {
public:
    virtual int read(char* bp, int len)               = 0;
    virtual int readv(struct iovec* iov, int iovcnt)  = 0;
    virtual int write(const char* bp, int len)        = 0;
    virtual int writev(struct iovec* iov, int iovcnt) = 0;

    /// Write out the entire supplied buffer, potentially
    /// requiring multiple calls to write().
    virtual int writeall(const char* bp, int len) = 0;

    /// Similar function for iovec
    virtual int writevall(struct iovec* iov, int iovcnt) = 0;

    // Variants that include a timeout
    virtual int timeout_read(char* bp, int len, int timeout_ms) = 0;
    virtual int timeout_readv(struct iovec* iov, int iovcnt,
                              int timeout_ms)  = 0;

};

#endif // __IOCLIENT_H__
