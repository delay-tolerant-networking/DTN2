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

#ifndef _SIM_CONVERGENCE_LAYER_H_
#define _SIM_CONVERGENCE_LAYER_H_


#include "conv_layers/ConvergenceLayer.h"

using namespace dtn;

namespace dtnsim {

/**
 * Simulator implementation of the Convergence Layer API.
 */
class SimConvergenceLayer : public ConvergenceLayer {
    
public:
    /**
     * Singleton initializer.
     */
    static void init()
    {
        instance_ = new SimConvergenceLayer();
    }

    /**
     * Singleton accessor
     */
    static SimConvergenceLayer* instance() { return instance_; }

    /**
     * Constructor.
     */
    SimConvergenceLayer();

    /// @{
    /// Virtual from ConvergenceLayer
    bool init_link(const LinkRef& link, int argc, const char* argv[]);
    void delete_link(const LinkRef& link);
    bool open_contact(const ContactRef& contact);
    void send_bundle(const ContactRef& contact, Bundle* bundle);
    /// @}
    
protected:
    static SimConvergenceLayer* instance_;
    u_char buf_[65536];
};

} // namespace dtnsim

#endif /* _SIM_CONVERGENCE_LAYER_H_ */
