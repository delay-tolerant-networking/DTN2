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

#ifndef _DTLSR_H_
#define _DTLSR_H_

#include <oasys/compat/inttypes.h>
#include <oasys/serialize/SerializableVector.h>
#include "naming/EndpointID.h"

namespace dtn {

class Bundle;

/**
 * Static class used for generic functionality in the DTLSR (Delay
 * Tolerant Link State Router) implementation.
 */
class DTLSR {
public:
    //----------------------------------------------------------------------
    /// Message types
    typedef enum {
        MSG_LSA = 1,
        MSG_EIDA = 2,
    } msg_type_t;

    //----------------------------------------------------------------------
    /// Link parameters that are sent over the network.
    class LinkParams : public oasys::SerializableObject {
    public:
        LinkParams() :
            state_(LINK_UP), cost_(0), delay_(0), bw_(0), qcount_(0), qsize_(0) {}
        
        LinkParams(const oasys::Builder&) {}
        virtual ~LinkParams() {}

        virtual void serialize(oasys::SerializeAction* a);

        /// states
        enum {
            LINK_UP   = 0x1,
            LINK_DOWN = 0x2
        };

        u_int8_t  state_;    ///< LINK_UP or LINK_DOWN
        u_int32_t cost_;     ///< configured link cost
        u_int32_t delay_;    ///< estimated link delay
        u_int32_t bw_;       ///< estimated link bandwidth
        u_int32_t qcount_;   ///< number of bundles in queue
        u_int32_t qsize_;    ///< total size of bundles in queue
    };

    //----------------------------------------------------------------------
    /// Structure used in LSAs for link state announcement
    class LinkState : public oasys::SerializableObject {
    public:
        LinkState(){}
        LinkState(const oasys::Builder&) {}
        virtual ~LinkState() {}

        virtual void serialize(oasys::SerializeAction* a);
        
        EndpointID  dest_;    ///< Destination node
        std::string id_;      ///< Link name
        u_int32_t   elapsed_; ///< Time since last state change for
                              ///  this link
        LinkParams  params_;  ///< Parameters of the link
    };
    
    typedef oasys::SerializableVector<LinkState> LinkStateVec;

    //----------------------------------------------------------------------
    /// The LSA that's sent over the network
    class LSA : public oasys::SerializableObject {
    public:
        LSA() {}
        LSA(const oasys::Builder&) {}
        virtual ~LSA() {}

        virtual void serialize(oasys::SerializeAction* a);

        u_int64_t        seqno_; ///< Strictly increasing sequence
                                 ///  number for this source
        LinkStateVec     links_; ///< Vector of link states
    };

    /**
     * Format the LSA into the given bundle's payload.
     */
    static void format_lsa_bundle(Bundle* bundle, const LSA* lsa);

    /**
     * Parse an LSA bundle.
     */
    static bool parse_lsa_bundle(const Bundle* bundle, LSA* lsa);
};

} // namespace dtn

#endif /* _DTLSR_H_ */
