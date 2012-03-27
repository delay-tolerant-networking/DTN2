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

#ifndef _PROPHET_STORE_H_
#define _PROPHET_STORE_H_

#include <oasys/debug/DebugUtils.h>
#include <oasys/serialize/TypeShims.h>
#include <oasys/storage/InternalKeyDurableTable.h>
#include <oasys/util/Singleton.h>
#include <oasys/storage/StorageConfig.h>
#include "naming/EndpointID.h"
#include "routing/ProphetNode.h"

namespace dtn {

class EndpointIDShim : public EndpointID,
                       public oasys::Formatter {
public:
    EndpointIDShim()
        : oasys::Formatter() {}
    EndpointIDShim(EndpointID eid)
        : EndpointID(eid),
          oasys::Formatter() {}
    virtual ~EndpointIDShim() {}

    int format(char *buf, size_t sz) const {
        return snprintf(buf, sz, "%s", c_str());
    }

    // Used to indicate how big a field is needed for the key.
    // Return is number of bytes needed where 0 means variable
    //
    static int shim_length() { return 0; }

    EndpointID value() const { return *this; }
};

class ProphetStore : public oasys::Singleton<ProphetStore, false> {
public:
    typedef oasys::InternalKeyDurableTable<
            EndpointIDShim,EndpointID,ProphetNode> ProphetDurableTable;
    typedef ProphetDurableTable::iterator iterator;

    /**
     * Boot time initializer
     */
    static int init(const oasys::StorageConfig& cfg,
                          oasys::DurableStore*  store);

    /**
     * Constructor
     */
    ProphetStore(const oasys::StorageConfig& cfg);

    /// Add a new ProphetNode
    bool add(ProphetNode* node);

    /// Retrieve a ProphetNode
    ProphetNode* get(const EndpointID& remote_eid);

    /// Update the ProphetNode data
    bool update(ProphetNode* node);

    /// Delete the ProphetNode
    bool del(ProphetNode* node);

    /// Return a new iterator.
    iterator* new_iterator();

    /// Close down the table
    void close();

protected:

    const oasys::StorageConfig& cfg_; ///< storage configuration
    ProphetDurableTable nodes_;       ///< ProphetNode information base
};

} // namespace dtn

#endif // _PROPHET_STORE_H_
