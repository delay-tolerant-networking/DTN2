/*
 *    Copyright 2005-2006 University of Waterloo
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


#ifndef _TCA_REGISTRY_H_
#define _TCA_REGISTRY_H_

#include <netinet/in.h>
#include <string>
#include <vector>
#include <rpc/rpc.h>



class RegRecord
{
public:
    RegRecord() : host_(), link_addr_() { }

    RegRecord(const std::string& host, const std::string& link_addr)
        : host_(host), link_addr_(link_addr) { }

    std::string     host_;          // partial eid, eg.  "tca://host"
    std::string     link_addr_;

    const std::string str() const { return host_ + " -> " + link_addr_; }
};



class TcaRegistry
{
public:

    TcaRegistry() : last_node_(0) { }

    bool init_nodes();
    bool init_addrs();

    bool write(const RegRecord& rr, int ttl);
    bool read(RegRecord& rr);

protected:

    std::vector<std::string> dht_nodes_;    // all available node names
    std::vector<sockaddr_in> dht_addrs_;    // all available node addrs

    unsigned int last_node_;                // index of last node used

    CLIENT* get_node(); // get next available node, starting at cur_node_

};


#endif /* _TCA_REGISTRY_H_ */
