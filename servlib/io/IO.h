#ifndef _IO_H_
#define _IO_H_

#include <fcntl.h>
#include <stdlib.h>
#include <sys/uio.h>

/**
 * Return code values for the timeout enabled functions such as
 * timeout_read() and timeout_accept(). Note that the functions return
 * an int, not an enumerated type since they may return other
 * information, e.g. the number of bytes read.
 */
enum IOTimeoutReturn_t {
    IOEOF 	= 0,	/* eof */
    IOERROR 	= -1,	/* error */
    IOTIMEOUT 	= -2	/* timeout */
};

class IO {
public:
    //@{
    /// System call wrappers (for logging)
    static int open(const char* path, int flags,
                    const char* log = NULL);

    static int open(const char* path, int flags, mode_t mode,
                    const char* log = NULL);

    static int close(int fd,
                     const char* log = NULL);
    
    static int read(int fd, char* bp, size_t len,
                    const char* log = NULL);
    
    static int readv(int fd, const struct iovec* iov, int iovcnt,
                     const char* log = NULL);
    
    static int write(int fd, const char* bp, size_t len,
                     const char* log = NULL);
    
    static int writev(int fd, const struct iovec* iov, int iovcnt,
                      const char* log = NULL);

    static int unlink(const char* path, 
                      const char* log = NULL);
    
    static int lseek(int fd, off_t offset, int whence,
                      const char* log = NULL);
    //@}
    
    /// Wrapper around poll() for a single fd
    /// @return -1 for error, 0 or 1 to indicate readiness
    static int poll(int fd, int events, int timeout_ms,
                    const char* log = NULL);
    
    //@{
    /// Fill in the entire supplied buffer, potentially
    /// requiring multiple calls to read().
    static int readall(int fd, char* bp, size_t len,
                       const char* log = NULL);

    static int readvall(int fd, const struct iovec* iov, int iovcnt,
                         const char* log = NULL);
    //@}
    
    //@{
    /// Write out the entire supplied buffer, potentially
    /// requiring multiple calls to write().
    static int writeall(int fd, const char* bp, size_t len,
                        const char* log = NULL);

    static int writevall(int fd, const struct iovec* iov, int iovcnt,
                         const char* log = NULL);
    //@}

    //@{
    /**
     * @brief Try to read the specified number of bytes, but don't
     * block for more than timeout milliseconds.
     *
     * @return the number of bytes read or the appropriate
     * IOTimeoutReturn_t code
     */
    static int timeout_read(int fd, char* bp, size_t len, int timeout_ms,
                            const char* log = NULL);
    
    static int timeout_readv(int fd, const struct iovec* iov, int iovcnt,
                             int timeout_ms, const char* log = NULL);
    static int timeout_readall(int fd, char* bp, size_t len,
                               int timeout_ms, const char* log = NULL);
    static int timeout_readvall(int fd, const struct iovec* iov, int iovcnt,
                                int timeout_ms, const char* log = NULL);
    //@}
    
    /// Set the file descriptor's nonblocking status
    static int set_nonblocking(int fd, bool nonblocking);
    
private:
    IO();  // don't ever instantiate

    typedef ssize_t(*rw_func_t)(int, void*, size_t);
    typedef ssize_t(*rw_vfunc_t)(int, const struct iovec*, int);

    static int rwall(rw_func_t rw, int fd, char* bp, size_t len,
                     const char* log);
    
    static int rwvall(rw_vfunc_t rw, int fd,
                      const struct iovec* iov, int iovcnt,
                      const char* log_func, const char* log);
};

#endif /* _IO_H_ */

