#ifndef __IOCLIENT_H__
#define __IOCLIENT_H__

#include <sys/uio.h>

/**
 * Abstract interface for any stream type output channel.
 */
class IOClient {
public:
    //@{
    /**
     * System call wrappers.
     */
    virtual int read(char* bp, size_t len) = 0;
    virtual int write(const char* bp, size_t len) = 0;
    virtual int readv(const struct iovec* iov, int iovcnt)  = 0;
    virtual int writev(const struct iovec* iov, int iovcnt) = 0;
    //@}
    
    //@{
    /**
     * Read/write out the entire supplied buffer, potentially
     * requiring multiple system calls.
     *
     * @return the total number of bytes written, or -1 on error
     */
    virtual int readall(char* bp, size_t len) = 0;
    virtual int writeall(const char* bp, size_t len) = 0;
    virtual int readvall(const struct iovec* iov, int iovcnt) = 0;
    virtual int writevall(const struct iovec* iov, int iovcnt) = 0;
    //@}

    //@{
    /**
     * @brief Try to read the specified number of bytes, but don't
     * block for more than timeout milliseconds.
     *
     * @return the number of bytes read or the appropriate
     * IOTimeoutReturn_t code
     */
    virtual int timeout_read(char* bp, size_t len, int timeout_ms) = 0;
    virtual int timeout_readv(const struct iovec* iov, int iovcnt,
                              int timeout_ms) = 0;
    virtual int timeout_readall(char* bp, size_t len, int timeout_ms) = 0;
    virtual int timeout_readvall(const struct iovec* iov, int iovcnt,
                                 int timeout_ms) = 0;
    //@}

    /// Set the file descriptor's nonblocking status
    virtual int set_nonblocking(bool nonblocking) = 0;
};

#endif // __IOCLIENT_H__
