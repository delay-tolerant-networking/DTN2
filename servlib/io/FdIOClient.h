#ifndef _FD_IOCLIENT_H_
#define _FD_IOCLIENT_H_

#include "IOClient.h"
#include "debug/Logger.h"

/*!
 * IOClient which uses pure file descriptors.
 */
class FdIOClient : public IOClient, public Logger {
public:
    FdIOClient(int fd);

    virtual int read(char* bp, size_t len);
    virtual int readv(const struct iovec* iov, int iovcnt);
    virtual int write(const char* bp, size_t len);
    virtual int writev(const struct iovec* iov, int iovcnt);

    /// Write out the entire supplied buffer, potentially
    /// requiring multiple calls to write().
    virtual int readall(char* bp, size_t len);
    virtual int writeall(const char* bp, size_t len);

    /// Similar function for iovec
    virtual int readvall(const struct iovec* iov, int iovcnt);
    virtual int writevall(const struct iovec* iov, int iovcnt);
    
    // Variants that include a timeout
    virtual int timeout_read(char* bp, size_t len, int timeout_ms);
    virtual int timeout_readv(const struct iovec* iov, int iovcnt,
                              int timeout_ms);
    virtual int timeout_readall(char* bp, size_t len, int timeout_ms);
    virtual int timeout_readvall(const struct iovec* iov, int iovcnt,
                                 int timeout_ms);
protected:
    int fd_;
};

#endif /* _FD_IOCLIENT_H_ */
