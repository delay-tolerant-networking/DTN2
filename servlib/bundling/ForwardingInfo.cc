/*
 * License Agreement
 * 
 * NOTICE
 * This software (or technical data) was produced for the U. S.
 * Government under contract W15P7T-05-C-F600, and is
 * subject to the Rights in Data-General Clause 52.227-14 (JUNE 1987)
 * 
 * Copyright (C) 2006. The MITRE Corporation (http://www.mitre.org/).
 * All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * The US Government will not be charged any license fee and/or
 * royalties related to this software.
 * 
 * * Neither name of The MITRE Corporation; nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ForwardingInfo.h"

namespace dtn {

void 
ForwardingInfo::serialize(oasys::SerializeAction *a)
{
    a->process("state", &state_);
    a->process("action", &action_);
    a->process("clayer", &clayer_);
    a->process("nextHop", &nexthop_);

    // casting won't be necessary after port to oasys::Time
    a->process("timestamp_sec",
        reinterpret_cast< u_int32_t * >(&timestamp_.tv_sec));
    a->process("timestamp_usec",
        reinterpret_cast< u_int32_t * >(&timestamp_.tv_usec));
}

} // namespace dtn
