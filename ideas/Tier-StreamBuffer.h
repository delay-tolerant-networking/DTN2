#ifndef __STREAM_BUFFER_H__
#define __STREAM_BUFFER_H__

#include <stdlib.h>

class StreamBuffer {
public:
    StreamBuffer(size_t size = DEFAULT_BUFSIZE);
    ~StreamBuffer();

    void set_size(size_t size);

    char* start();
    char* end();
    
    void reserve(size_t amount);
    void fill(size_t amount);
    void consume(size_t amount);
    void clear();
    
    size_t fullbytes();
    size_t tailbytes();

private:
    void realloc(size_t size);
    void moveup();

    size_t start_;
    size_t end_;
    size_t size_;

    char* buf_;

    static const size_t DEFAULT_BUFSIZE = 512;
};

#endif //__STREAM_BUFFER_H__
