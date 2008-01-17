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

#ifndef _SESSION_TABLE_H_
#define _SESSION_TABLE_H_

#include <map>

namespace dtn {

class EndpointID;
class Session;

/**
 * Table to manage open sessions.
 */
class SessionTable {
public:
    SessionTable();
    Session* lookup_session(const EndpointID& eid) const;
    void     add_session(Session* s);
    Session* get_session(const EndpointID& eid);

protected:
    typedef std::map<EndpointID, Session*> SessionMap;
    SessionMap table_;
};

} // namespace dtn


#endif /* _SESSION_TABLE_H_ */
