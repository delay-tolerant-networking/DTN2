#ifndef _IO_H_
#define _IO_H_

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
    /// System call wrapper (for logging)
    static int read(int fd, char* bp, int len,
                    const char* log = NULL);
    
    static int readv(int fd, struct iovec* iov, int iovcnt,
                     const char* log = NULL);
    
    static int write(int fd, const char* bp, int len,
                     const char* log = NULL);
    
    static int writev(int fd, struct iovec* iov, int iovcnt,
                      const char* log = NULL);
    //@}
    
    /// Wrapper around poll() for a single fd
    /// @return -1 for error, 0 or 1 to indicate readiness
    static int poll(int fd, int events, int timeout_ms,
                    const char* log = NULL);
    
    //@{
    /// Write out the entire supplied buffer, potentially
    /// requiring multiple calls to write().
    static int writeall(int fd, const char* bp, int len,
                        const char* log = NULL);

    static int writevall(int fd, struct iovec* iov, int iovcnt,
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
    static int timeout_read(int fd, char* bp, int len, int timeout_ms,
                            const char* log = NULL);
    
    static int timeout_readv(int fd, struct iovec* iov, int iovcnt,
                             int timeout_ms, const char* log = NULL);
    //@}
    
    /// Set the file descriptor's nonblocking status
    static int set_nonblocking(int fd, bool nonblocking);
    
private:
    IO();  // don't ever instantiate

};

#endif /* _IO_H_ */

