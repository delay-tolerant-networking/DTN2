#include "FileIOClient.h"
#include "IO.h"

int
FileIOClient::lseek(off_t offset, int whence)
{
    return IO::lseek(fd_, offset, whence, logpath_);
}
