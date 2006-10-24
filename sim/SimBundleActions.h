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

#ifndef _SIM_BUNDLE_ACTIONS_H_
#define _SIM_BUNDLE_ACTIONS_H_

#include "bundling/BundleActions.h"

using namespace dtn;

namespace dtnsim {

/**
 * Intermediary class that provides the interface from the router to
 * the rest of the system. All functions are virtual since the
 * simulator overrides them to provide equivalent functionality (in
 * the simulated environment).
 */
class SimBundleActions : public BundleActions {
public:
    SimBundleActions() : BundleActions() {}
    virtual ~SimBundleActions() {}

    /**
     * Add the given bundle to the data store.
     */
    virtual void store_add(Bundle* bundle);

    /**
     * Remove the given bundle from the data store.
     */
    virtual void store_del(Bundle* bundle);

};

} // namespace dtnsim

#endif /* _SIM_BUNDLE_ACTIONS_H_ */
