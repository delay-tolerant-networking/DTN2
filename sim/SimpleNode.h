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

#ifndef _SIMPLE_NODE_H_
#define _SIMPLE_NODE_H_

#include "Node.h"

namespace dtnsim {

/*
 * A simple node definition that performs random routing
 */

class SimpleNode : public Node {

public:

    SimpleNode(int id, const char* logpath);
    virtual ~SimpleNode();
    
    /**
     * Virtual functions from Node
     */

    virtual void process(Event *e); 
    virtual  void chewing_complete(SimContact* c, double size, Message* msg);
    virtual  void open_contact(SimContact* c);
    virtual  void close_contact(SimContact* c);
    virtual  void message_received(Message* msg);
    
    
private:
    virtual  void forward(Message* msg);
    std::vector<Message*> msgq_; /// message to be forwarded

};

} // namespace dtnsim

#endif /* _SIMPLE_NODE_H_ */
