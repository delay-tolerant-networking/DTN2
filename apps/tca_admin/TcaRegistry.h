/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * University of Waterloo Open Source License
 * 
 * Copyright (c) 2005 University of Waterloo. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *  
 *   Neither the name of the University of Waterloo nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY
 * OF WATERLOO OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _TCA_REGISTRY_H_
#define _TCA_REGISTRY_H_

#include <netinet/in.h>
#include <string>
#include <vector>


struct CLIENT;



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
