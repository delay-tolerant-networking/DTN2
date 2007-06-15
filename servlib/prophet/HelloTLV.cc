/*
 *    Copyright 2007 Baylor University
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

#include <arpa/inet.h> // for hton[ls] and ntoh[ls]
#include "Util.h"
#include "HelloTLV.h"
#include "BundleCore.h"

namespace prophet
{

HelloTLV::HelloTLV()
    : BaseTLV(BaseTLV::HELLO_TLV),
      hf_(HF_UNKNOWN),
      timer_(0) {}

HelloTLV::HelloTLV(hello_hf_t hf,
                   u_int8_t timer,
                   const std::string& sender)
    : BaseTLV(BaseTLV::HELLO_TLV,0,
              HelloTLVHeaderSize + FOUR_BYTE_ALIGN(sender.size())),
      hf_(hf),
      timer_(timer),
      sender_(sender)
{
}

bool
HelloTLV::deserialize(const u_char* bp, size_t len)
{
    HelloTLVHeader* hdr = (HelloTLVHeader*) bp;

    // weed out the oddball
    if (bp == NULL) return false;

    // fail out if wrong byte code
    if (hdr->type != HELLO_TLV) return false;

    // fail out if less than min size
    if (len < HelloTLVHeaderSize) return false;

    // fail out if length's don't match up
    size_t hello_len = ntohs(hdr->length);
    if (len < hello_len) return false;
    if (hello_len < hdr->name_length) return false;

    // Parse!
    hf_     = (hello_hf_t) hdr->HF;
    length_ = hello_len;
    timer_  = hdr->timer;

    std::string name((char*)&hdr->sender_name[0],(int)hdr->name_length);
    sender_.assign(name);

    return true;
}

size_t
HelloTLV::serialize(u_char* bp, size_t len) const
{
    // weed out the oddball
    if (sender_ == "") return 0;
    if (typecode_ != HELLO_TLV) return 0;

    // align to 32 bit boundary
    size_t hello_sz = FOUR_BYTE_ALIGN(sender_.length() + HelloTLVHeaderSize);

    // check our budget
    if (hello_sz > len) return 0;

    // write out to buffer
    HelloTLVHeader* hdr = (HelloTLVHeader*) bp;
    length_     = hello_sz;
    memset(hdr,0,length_);
    hdr->type   = typecode_;
    hdr->HF     = hf_;
    hdr->length = htons(length_);
    hdr->timer  = timer_;
    // mask out one-byte value (unnecessary?)
    hdr->name_length = sender_.length() & 0xff;

    size_t buflen = hello_sz;

    memcpy(&hdr->sender_name[0],sender_.c_str(),sender_.length());

    // walk the end of the buffer to zero out any slack
    while (buflen-- > HelloTLVHeaderSize + sender_.length())
        bp[buflen] = '\0';

    return hello_sz;
}

}; // namespace prophet
