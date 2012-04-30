/*
 *    Copyright 2005-2006 Intel Corporation
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

#ifndef _SIM_REGISTRATION_H_
#define _SIM_REGISTRATION_H_

#include <map>

#include "Node.h"
#include "reg/Registration.h"

using namespace dtn;

namespace dtnsim {

/**
 * Registration used for the simulator
 */
class SimRegistration : public oasys::Formatter, public APIRegistration {
public:
    SimRegistration(Node* node, const EndpointID& endpoint, u_int32_t expiration = 10);

    /**
     * Deliver the given bundle.
     */
    void deliver_bundle(Bundle* bundle);

    /**
     * Virtual from Formatter
     */
    int format(char* buf, size_t sz) const;

protected:
    Node* node_;
};

} // namespace dtnsim

#endif /* _SIM_REGISTRATION_H_ */
