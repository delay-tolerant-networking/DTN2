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

#include <netdb.h>                  // needed for gethostbyname
#include "libs/gateway_prot.h"
#include "libs/sha1.h"
#include "TcaRegistry.h"


static const char* APP_STRING = "tca";
static const char* CLIB_STRING = "rpcgen";
static const int DHT_PORT = 5852;

static const int DHT_KEYLEN = 20;           // number of uints in a key


///////////////////////////////////////////////////////////////////////////////
// Functions for interfacing with OpenDHT

// lookup hostname, store address in addr
static bool
lookup_host(const char* host, int port, struct sockaddr_in* addr)
{
    struct hostent* h = gethostbyname (host);
    if (h == NULL) return false;

    bzero (addr, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr = *((struct in_addr *) h->h_addr);
    return true;
}


// try to open connection to given dht node by addr
static CLIENT*
get_connection(sockaddr_in addr)
{
    int sockp = RPC_ANYSOCK;
    CLIENT * c = clnttcp_create(&addr, BAMBOO_DHT_GATEWAY_PROGRAM,
                        BAMBOO_DHT_GATEWAY_VERSION, &sockp, 0, 0);
    return c;
}


/*
// try to open connection to given dht node by hostname
static CLIENT*
get_connection(const char* hostname)
{
    struct sockaddr_in addr;
    if(lookup_host(hostname, DHT_PORT, &addr) < 0) return NULL;
    return get_connection(addr);
}
*/


// useful for probing a dht node to see if it's alive:
static bool
do_null_op(CLIENT* c)
{
    void* null_args = NULL;
    void* res = bamboo_dht_proc_null_2(&null_args, c);
    return (res != NULL);
}


// test dht node, printing status messages
// if successful, addr contains a valid sockaddr_in
static bool
test_node(const char* hostname, sockaddr_in& addr)
{
    printf("   testing dht node %s... ", hostname);

    // try to get host addr
    if (!lookup_host(hostname, DHT_PORT, &addr))
    {
        printf("lookup_host failed\n");
        return false;
    }

    // try to connect to node
    // Note: This step seems to be insanely slow when it fails. Is there
    // a way to timeout faster?
    CLIENT* p_client = get_connection(addr);
    if (p_client == NULL)
    {
        printf("get_connection failed\n");
        return false;
    }

    // try a null op
    if (!do_null_op(p_client))
    {
        printf("null_op failed\n");
        clnt_destroy(p_client);
        return false;
    }

    printf("succeeded.\n");
    clnt_destroy(p_client);
    return true;
}


// hash a key s, from original long-string form, down to 20-byte key
// usable in the dht
static void
hash(const std::string& s, uint8 digest[DHT_KEYLEN])
{
    // Use sha1 hash of endpointid to get a (probably) unique 20-byte key
    sha1_context ctx;
    sha1_starts(&ctx);
    sha1_update(&ctx, (unsigned char*)(s.c_str()), s.length());
    sha1_finish(&ctx, digest);
}


/*
static void
dump_digest(uint8 digest[DHT_KEYLEN])
{
    printf("digest=");
    for (int i=0; i<DHT_KEYLEN; ++i) printf("%c", digest[i]);
    printf("\n");
}
*/


///////////////////////////////////////////////////////////////////////////////
// class TcaRegistry


bool
TcaRegistry::init_nodes()
{
    // Construct list of available DHT nodes, hard coded at the moment.
    // TODO: Do something smarter here, like go to OpenDHT site and read
    // the current list of DHT nodes. Or read them from a local file that
    // somebody actively maintains.

    // To make this fast as possible for testing, cut this list down to just
    // a few. For greater reliability and scalability, use more nodes.
    dht_nodes_.push_back(std::string("cloudburst.uwaterloo.ca"));
    dht_nodes_.push_back(std::string("blast.uwaterloo.ca"));

    // Other known nodes:
    /*
    dht_nodes_.push_back(std::string("lefthand.eecs.harvard.edu"));
    dht_nodes_.push_back(std::string("node2.lbnl.nodes.planet-lab.org"));
    dht_nodes_.push_back(std::string("pl1.cs.utk.edu"));
    dht_nodes_.push_back(std::string("pl1.ece.toronto.edu"));
    dht_nodes_.push_back(std::string("planetlab2.cnds.jhu.edu"));
    dht_nodes_.push_back(std::string("ricepl-3.cs.rice.edu"));
    dht_nodes_.push_back(std::string("pli2-pa-3.hpl.hp.com"));
    dht_nodes_.push_back(std::string("planetlab10.millennium.berkeley.edu"));
    */

    return true;
}


bool
TcaRegistry::init_addrs()
{
    // First pass at "something smarter"... 
    // Test each dht node and keep only the nodes that are awake.

    // Usage Note: It would be good to call this function periodically
    // to refresh the list of "good" nodes.

    printf("Initializing TcaRegistry...\n");

    last_node_ = 0;

    sockaddr_in addr;
    for (unsigned int i=0; i<dht_nodes_.size(); ++i)
    {
        if (test_node(dht_nodes_[i].c_str(), addr))
        {
            // it's a keeper
            dht_addrs_.push_back(addr);
        }
    }
            
    if (dht_addrs_.size() == 0) return false;

    printf("...dht nodes available = %d / %d\n",
           dht_addrs_.size(), dht_nodes_.size());
    return true;
}



// write a registry record

bool
TcaRegistry::write(const RegRecord& rr, int ttl)
{
    CLIENT* p_node = get_node();
    if (p_node == NULL) return false;

    // printf("TcaRegistry::write: using node %d\n", int(p_node));

    uint8 key[DHT_KEYLEN];
    hash(rr.host_, key);
    // dump_digest(key);

    bamboo_put_args args;
    memset(&args, 0, sizeof(args));

    args.application = const_cast<char*>(APP_STRING);
    args.client_library = const_cast<char*>(CLIB_STRING);
    memcpy(args.key, key, DHT_KEYLEN);

    args.value.bamboo_value_len = rr.link_addr_.length() + 1;
    args.value.bamboo_value_val = const_cast<char*>(rr.link_addr_.c_str());

    args.ttl_sec = ttl;

    // TODO: Append other fields? Like timestamp of entry/refresh?
    
    bamboo_stat* res = bamboo_dht_proc_put_2(&args, p_node);
    // printf("TcaRegistry::write: put return code = %d\n", int(*res));
        
    return (*res == BAMBOO_OK);
}



// read a registry record
// rr.eid_ must be primed with the endpointid of the node to lookup

bool
TcaRegistry::read(RegRecord& rr)
{
    CLIENT* p_node = get_node();
    if (p_node == NULL) return false;

    // printf("TcaRegistry::read: using node %d\n", int(p_node));

    uint8 key[DHT_KEYLEN];
    hash(rr.host_, key);
    // dump_digest(key);

    bamboo_get_args args;
    memset(&args, 0, sizeof(args));

    args.application = const_cast<char*>(APP_STRING);
    args.client_library = const_cast<char*>(CLIB_STRING);
    memcpy(args.key, key, DHT_KEYLEN);

    // Note: to here, this function is identical to write()

    args.maxvals = 1;

    bamboo_get_res* res = bamboo_dht_proc_get_2(&args, p_node);
    if (res == NULL)
    {
        printf("TcaRegistry::read: get returned NULL\n");
        return false;
    }

    int n_values = res->values.values_len;
    
    if (n_values != 1)
    {
        // printf("TcaRegistry::read: get returned %d values\n", n_values);
        return false;
    }

    bamboo_value* p_val = &res->values.values_val[0];

    rr.link_addr_ = p_val->bamboo_value_val;
    printf("TcaRegistry::read: succeeded! value=%s\n", rr.link_addr_.c_str());

    return true;
}


/*
// This version gets nodes by dns name -- inneficient because of dns
// lookup, and also bad because the node in question may not be alive.
CLIENT*
TcaRegistry::get_node()
{
    // Get next available node. We deliberately spread the load around
    // among all availabe nodes, and also tolerate missing nodes.

    CLIENT* p_node = NULL;

    for (unsigned int i = last_node_ + 1; i != last_node_; ++i)
    {
        if (i == dht_nodes_.size()) i = 0;
        p_node = get_connection(dht_nodes_[i].c_str());
        if (p_node)
        {
            printf("TcaRegistry::get_node: using node %s\n",
                    dht_nodes_[i].c_str());
            last_node_ = i;
            break;
        }
    }

    return p_node;
}
*/


CLIENT*
TcaRegistry::get_node()
{
    // Get next available node. We deliberately spread the load around
    // among all availabe nodes, and also tolerate missing nodes.
    //
    // This version uses the addrs list which saves a dns lookup.
    // Also, the addrs list is only populated with the nodes that are alive
    // at startup, so there's less chance of failed attempts.

    CLIENT* p_node = NULL;

    for (unsigned int i = last_node_ + 1; i != last_node_; ++i)
    {
        if (i == dht_addrs_.size()) i = 0;
        p_node = get_connection(dht_addrs_[i]);
        if (p_node)
        {
            last_node_ = i;
            break;
        }
    }

    return p_node;
}



