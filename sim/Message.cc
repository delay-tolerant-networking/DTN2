#include "Message.h"

long Message::total_ = 0;

Message::Message() {}
Message::Message(int src, int dst, double size) 
{
    src_ = src;
    dst_ = dst;
    origsize_ = size;
    size_ = size;
    offset_ = 0;
    id_ = next();
}

int
Message::id() { return id_  ; }

Message*
Message::clone() 
{
    Message* retval = new Message();
    retval->src_ = src_;
    retval->dst_ = dst_;
    retval->size_ = size_;
    retval->origsize_ = origsize_;
    retval->offset_ = offset_;
    retval->id_ = id_;
    return retval;
}

void 
Message::rm_bytes(double len) 
{
    offset_ = offset_ + len;
    size_ = size_ - len;
}
    
