#ifndef _FILE_IOCLIENT_H_
#define _FILE_IOCLIENT_H_

#include "FdIOClient.h"
#include <fcntl.h>
#include <string.h>

/**
 * IOClient derivative for real files (i.e. not sockets). Unlike the
 * base class FdIOClient, FileIOClient contains the path as a member
 * variable and exposes file specific system calls, i.e. open(),
 * lseek(), etc.
 */
class FileIOClient : public FdIOClient {
public:
    /// Basic constructor, leaves both path and fd unset
    FileIOClient() : FdIOClient(-1) {}
    virtual ~FileIOClient() {}

    ///@{
    /// System call wrappers
    int open(const char* path, int flags);
    int open(const char* path, int flags, mode_t mode);
    int close();
    int unlink();
    int lseek(off_t offset, int whence);
    ///@}

    /// Path accessor
    const char* path() { return path_.c_str(); }

protected:
    /// Path to the file
    std::string path_;
};

#endif /* _FILE_IOCLIENT_H_ */
