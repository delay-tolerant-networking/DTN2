#ifndef _FILE_IOCLIENT_H_
#define _FILE_IOCLIENT_H_

#include "FdIOClient.h"

/**
 * IOClient derivative for real files (i.e. not sockets).
 */
class FileIOClient : public FdIOClient {
public:
    FileIOClient(int fd) : FdIOClient(fd) {}

    /// System call wrappers
    int lseek(off_t offset, int whence);
};

#endif /* _FILE_IOCLIENT_H_ */
