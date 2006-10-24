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


#ifndef _NODE2_H_
#define _NODE2_H_

/*
 * Interface to DTN2  and Simulator
 */


#include "Node.h"

namespace dtnsim {



class GlueNode : public Processable, public oasys::Logger {

public:
    static long next() {
	return total_ ++ ;
    }

    
    Node(int id);
    virtual ~Node();

    virtual void process(Event *e); ///< virtual function from Processable
    int id() { return id_ ;}

    
   virtual  void chewing_complete(Contact* c, double size, Message* msg);
   virtual  void open_contact(Contact* c);
   virtual  void close_contact(Contact* c);
   virtual  void forward(Message* msg);
    
private:
    static long total_;
    int id_;
    std::vector<Message*> msgq_;
    //   EndpointID name_;
};

} // namespace dtnsim

#endif /* _NODE2_H_ */
