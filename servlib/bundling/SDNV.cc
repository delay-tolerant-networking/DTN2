/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2005 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Log.h>

#include "SDNV.h"

namespace dtn {

int
SDNV::encode(u_int64_t val, u_char* bp, size_t len)
{
    u_char* start = bp;

    /*
     * Figure out how many bytes we need for the encoding.
     */
    size_t val_len = 0;
    u_int64_t tmp = val;

    do {
        tmp = tmp >> 7;
        val_len++;
    } while (tmp != 0);

    ASSERT(val_len > 0);
    ASSERT(val_len <= 10);

    /*
     * Make sure we have enough buffer space.
     */
    if (len < val_len) {
        return -1;
    }

    /*
     * Now advance bp to the last byte and fill it in backwards with
     * the value bytes.
     */
    bp += val_len;
    u_char high_bit = 0; // for the last octet
    do {
        --bp;
        *bp = (u_char)(high_bit | (val & 0x7f));
        high_bit = (1 << 7); // for all but the last octet
        val = val >> 7;
    } while (val != 0);

    ASSERT(bp == start);

    return val_len;
}

size_t
SDNV::encoding_len(u_int64_t val)
{
    u_char buf[16];
    int ret = encode(val, buf, sizeof(buf));
    ASSERT(ret != -1 && ret != 0);
    return ret;
}

int
SDNV::decode(const u_char* bp, size_t len, u_int64_t* val)
{
    const u_char* start = bp;
    
    if (!val) {
        return -1;
    }

    /*
     * Zero out the existing value, then shift in the bytes of the
     * encoding one by one until we hit a byte that has a zero
     * high-order bit.
     */
    size_t val_len = 0;
    *val = 0;
    do {
        if (len == 0)
            return -1; // buffer too short
        
        *val = (*val << 7) | (*bp & 0x7f);
        ++val_len;
        
        if ((*bp & (1 << 7)) == 0)
            break; // all done;

        ++bp;
        --len;
    } while (1);

    /*
     * Since the spec allows for infinite length values but this
     * implementation only handles up to 64 bits, check for overflow.
     * Note that the only supportably 10 byte SDNV must store exactly
     * one bit in the first byte of the encoding (i.e. the 64'th bit
     * of the original value).
     */
    if ((val_len > 10) || ((val_len == 10) && (*start != 0x81))) {
        log_err("/sdnv", "overflow value in sdnv!!!");
        return -1;
    }

    return val_len;
}

} // namespace dtn
