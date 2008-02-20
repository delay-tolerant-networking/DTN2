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

#include <oasys/util/StringBuffer.h>

#include "SessionTable.h"
#include "Session.h"
#include "naming/EndpointID.h"

namespace dtn {

//----------------------------------------------------------------------
SessionTable::SessionTable()
{
}

//----------------------------------------------------------------------
Session*
SessionTable::lookup_session(const EndpointID& eid) const
{
    SessionMap::const_iterator iter = table_.find(eid);
    if (iter == table_.end())
        return NULL;
    
    return iter->second;
}

//----------------------------------------------------------------------
void
SessionTable::add_session(Session* s)
{
    ASSERT(lookup_session(s->eid()) == NULL);
    
    table_[s->eid()] = s;
}

//----------------------------------------------------------------------
Session*
SessionTable::get_session(const EndpointID& eid)
{
    Session* session = lookup_session(eid);
    if (! session) {
        session = new Session(eid);
        add_session(session);
    }
    return session;
}

//----------------------------------------------------------------------
void
SessionTable::dump(oasys::StringBuffer* buf) const
{
    SessionMap::const_iterator i;
    SubscriberList::const_iterator j;
    for (i = table_.begin(); i != table_.end(); ++i) {

        Session* session = i->second;
        buf->appendf("    %s upstream=*%p subscribers=",
                     session->eid().c_str(), &session->upstream());
        
        for (j = session->subscribers().begin();
             j != session->subscribers().end(); ++j) {

            const Subscriber& sub = *j;
            if (sub == session->upstream()) {
                continue;
            }
            buf->appendf("*%p ", &sub);
        }
        buf->appendf("\n");
    }
}

} // namespace dtn

