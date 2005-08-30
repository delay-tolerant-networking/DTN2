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
#ifndef _SDNV_H_
#define _SDNV_H_

#include <oasys/compat/inttypes.h>

namespace dtn {

/**
 * Class to handle parsing and formatting of self describing numeric
 * values (SDNVs).
 *
 * The basic idea is to enable a compact byte representation of
 * numeric values that may widely vary in size. This encoding is based
 * on the ASN.1 specification for encoding Object Identifier Arcs.
 *
 * Conceptually, the integer value to be encoded is split into 7-bit
 * segments. These are encoded into the output byte stream, such that
 * the high order bit in each byte is set to one for all bytes except
 * the last one.
 *
 * Note that this implementation only handles values up to 64-bits in
 * length (since the conversion is between a u_int64_t and the encoded
 * byte sequence).
 */
class SDNV {
public:
    /**
     * Return the number of bytes needed to encode the given value.
     */
    static size_t encoding_len(u_int64_t val);
    
    /**
     * Convert the given 64-bit integer into an SDNV.
     *
     * @return The number of bytes used, or -1 on error.
     */
    static int encode(u_int64_t val, u_char* bp, size_t len);
    
    /**
     * Convert an SDNV pointed to by bp into a unsigned 64-bit
     * integer.
     *
     * @return The number of bytes of bp consumed, or -1 on error.
     */
    static int decode(const u_char* bp, size_t len, u_int64_t* val);

    /**
     * Convert an SDNV pointed to by bp into a unsigned 32-bit
     * integer. Checks for overflow in the SDNV.
     *
     * @return The number of bytes of bp consumed, or -1 on error.
     */
    static int decode(const u_char* bp, size_t len, u_int32_t* val)
    {
        u_int64_t lval;
        int ret = decode(bp, len, &lval);
        
        if (lval > 0xffffffffLL) {
            return -1;
        }

        *val = (u_int32_t)lval;
        
        return ret;
    }
};

} // namespace dtn

#endif /* _SDNV_H_ */
