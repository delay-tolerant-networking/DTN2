#ifndef _FD_IOCLIENT_H_
#define _FD_IOCLIENT_H_

#include "IOClient.h"

/*!
 * IOClient which uses pure file descriptors.
 */
class FdIOClient : public IOClient {    
public:
    FdIOClient(int fd);

    virtual int read(char* bp, int len);
    virtual int readv(struct iovec* iov, int iovcnt);
    virtual int write(const char* bp, int len);
    virtual int writev(struct iovec* iov, int iovcnt);

    /// Write out the entire supplied buffer, potentially
    /// requiring multiple calls to write().
    virtual int writeall(const char* bp, int len);

    /// Similar function for iovec
    virtual int writevall(struct iovec* iov, int iovcnt);
    
    // Variants that include a timeout
    virtual int timeout_read(char* bp, int len, int timeout_ms);
    virtual int timeout_readv(struct iovec* iov, int iovcnt,
                              int timeout_ms);
protected:
    int fd_;
};

#endif /* _FD_IOCLIENT_H_ */
