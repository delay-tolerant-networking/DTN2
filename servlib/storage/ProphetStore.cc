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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/debug/Log.h>
#include "ProphetStore.h"

template <>
dtn::ProphetStore* oasys::Singleton<dtn::ProphetStore,false>::instance_ = 0;

namespace dtn {

//----------------------------------------------------------------------
ProphetStore::ProphetStore(const oasys::StorageConfig& cfg)
    : cfg_(cfg),
      nodes_("ProphetStore","/dtn/storage/prophet",
             "ProphetNode","prophet")
{
}

//----------------------------------------------------------------------
int
ProphetStore::init(const oasys::StorageConfig& cfg,
                   oasys::DurableStore*        store)
{
    if (instance_ != NULL)
    {
        PANIC("ProphetStore::init called multiple times");
    }
    instance_ = new ProphetStore(cfg);
    return instance_->nodes_.do_init(cfg,store);
}

//----------------------------------------------------------------------
bool
ProphetStore::add(ProphetNode* node)
{
    ProphetNode* n = new ProphetNode(*node);
    bool ok = nodes_.add(n);
    delete n;
    return ok;
}

//----------------------------------------------------------------------
ProphetNode*
ProphetStore::get(const EndpointID& eid)
{
    log_debug_p("/dtn/route/store","get(%s)",eid.c_str());
    return nodes_.get(eid);
}

//----------------------------------------------------------------------
bool
ProphetStore::update(ProphetNode* node)
{
    ProphetNode* n = new ProphetNode(*node);
    bool ok = nodes_.update(n);
    delete n;
    return ok;
}

//----------------------------------------------------------------------
bool
ProphetStore::del(ProphetNode* node)
{
    EndpointID eid(node->dest_id());
    return nodes_.del(eid);
}

//----------------------------------------------------------------------
ProphetStore::iterator*
ProphetStore::new_iterator()
{
    return nodes_.new_iterator();
}

//----------------------------------------------------------------------
void
ProphetStore::close()
{
    nodes_.close();
}

} // namespace dtn
