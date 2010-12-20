/*
 *    Copyright 2006 Intel Corporation
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

#ifndef _CUSTODYSIGNAL_H_
#define _CUSTODYSIGNAL_H_

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "Bundle.h"
#include "BundleProtocol.h"

namespace dtn {

/**
 * Utility class to format and parse custody signal bundles.
 */
class CustodySignal {
public:
    /**
     * The reason codes are defined in the bundle protocol class.
     */
    typedef BundleProtocol::custody_signal_reason_t reason_t;

    /**
     * Struct to hold the payload data of the custody signal.
     */
    struct data_t {
        u_int8_t        admin_type_;
        u_int8_t        admin_flags_;
        bool            succeeded_;
        u_int8_t        reason_;
        u_int64_t       orig_frag_offset_;
        u_int64_t       orig_frag_length_;
        BundleTimestamp custody_signal_tv_;
        BundleTimestamp orig_creation_tv_;
        EndpointID      orig_source_eid_;
    };

    /**
     * Constructor-like function to create a new custody signal bundle.
     */
    static void create_custody_signal(Bundle*           bundle,
                                      const Bundle*     orig_bundle,
                                      const EndpointID& source_eid,
                                      bool              succeeded,
                                      reason_t          reason);

    /**
     * Parsing function for custody signal bundles.
     */
    static bool parse_custody_signal(data_t* data,
                                     const u_char* bp, u_int len);

    /**
     * Pretty printer for custody signal reasons.
     */
    static const char* reason_to_str(u_int8_t reason);
};

} // namespace dtn

#endif /* _CUSTODYSIGNAL_H_ */
