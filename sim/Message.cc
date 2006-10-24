/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Message.h"

namespace dtnsim {

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
    

} // namespace dtnsim
