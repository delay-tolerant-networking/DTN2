#ifndef __DUMMY_IO_H__
#define __DUMMY_IO_H__

#include "lib/IOClient.h"

/*!
 * IOClient which uses pure file descriptors.
 */
class FdIO : public IOClient {    
public:
    FdIO(int in_fd, int out_fd);

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
	
private:
    int in_fd_;
    int out_fd_;
};

#endif
