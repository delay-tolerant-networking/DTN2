/*
 *    Copyright 2007 Intel Corporation
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

#ifdef HAVE_CONFIG_H
#include <dtn-config.h>
#endif

#include <oasys/serialize/MarshalSerialize.h>
#include <oasys/util/ScratchBuffer.h>

#include "DTLSR.h"
#include "bundling/Bundle.h"

namespace dtn {

//----------------------------------------------------------------------
void
DTLSR::LinkParams::serialize(oasys::SerializeAction* a)
{
    a->process("state", &state_);
    a->process("cost",  &cost_);
    a->process("delay", &delay_);
    a->process("bw",    &bw_);
    a->process("qcount",&qcount_);
    a->process("qsize", &qsize_);
}

//----------------------------------------------------------------------
void
DTLSR::LinkState::serialize(oasys::SerializeAction* a)
{
    a->process("dest",    &dest_);
    a->process("id",      &id_);
    a->process("elapsed", &elapsed_);
    a->process("params",  &params_);
}

//----------------------------------------------------------------------
void
DTLSR::LSA::serialize(oasys::SerializeAction* a)
{
    a->process("seqno",   &seqno_);
    a->process("links",   &links_);
}

//----------------------------------------------------------------------
void
DTLSR::format_lsa_bundle(Bundle* bundle, const LSA* lsa)
{
    oasys::MarshalSize ms(oasys::Serialize::CONTEXT_NETWORK);
    if (ms.action(lsa) != 0) {
        log_crit_p("/dtn/route/dtlsr", "error sizing lsa");
        return;
    }
    size_t len = ms.size();

    // XXX/demmer should define a new serialization class that
    // overrides the Marshal::process for strings and uses an SDNV
    // length instead. Could also send the params in a packed struct
    // for convenience

    oasys::ScratchBuffer<u_char*, 256> buf;
    
    oasys::Marshal m(oasys::Serialize::CONTEXT_NETWORK, &buf);
    if (m.action(lsa) != 0) {
        log_crit_p("/dtn/route/dtlsr", "error marshalling lsa");
        return;
    }

    bundle->mutable_payload()->set_length(1 + len);

    u_char type = MSG_LSA;
    bundle->mutable_payload()->write_data(&type,     0, 1);
    bundle->mutable_payload()->write_data(buf.buf(), 1, len);
}

//----------------------------------------------------------------------
bool
DTLSR::parse_lsa_bundle(const Bundle* bundle, LSA* lsa)
{
    oasys::ScratchBuffer<u_char*, 256> buf;
    size_t len = bundle->payload().length();
    bundle->payload().read_data(0, len, buf.buf(len));

    if (buf.buf()[0] != MSG_LSA) {
        log_warn_p("/dtn/route/dtlsr",
                   "parse_lsa_bundle: typecode byte %u != MSG_LSA",
                   buf.buf()[0]);
        return false;
    }
    
    oasys::Unmarshal um(oasys::Serialize::CONTEXT_NETWORK,
                        buf.buf() + 1, len - 1);
    if (um.action(lsa) != 0) {
        log_warn_p("/dtn/route/dtlsr",
                   "error unmarshalling lsa vector");
        return false;
    }

    return true;
}

} // namespace dtn
