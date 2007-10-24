/*
 *    Copyright 2006 Intel Corporation
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

#include "gateway_rpc.h"
#include "gateway_prot.h"
#include <sys/socket.h>             // needed for AF_INET
#include <netdb.h>                  // needed for gethostbyname
#include <string.h>
#include <stdio.h>

static const int DHT_PORT = 5852;

///////////////////////////////////////////////////////////////////////////////
// Functions for interfacing with OpenDHT

// lookup hostname, store address in addr
int
lookup_host(const char* host, int port, struct sockaddr_in* addr)
{
    struct hostent* h = gethostbyname (host);
    if (h == NULL) return 0;

    bzero (addr, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);

    // demmer: rewrote the following as a memcpy to avoid -Wcast-align bugs
    //addr->sin_addr = *((struct in_addr *) h->h_addr);
    memcpy(&addr->sin_addr, h->h_addr, sizeof(struct in_addr));
    return 1;
}


// try to open connection to given dht node by addr
CLIENT*
get_connection(struct sockaddr_in* addr)
{
    int sockp = RPC_ANYSOCK;
    CLIENT * c = clnttcp_create(addr, BAMBOO_DHT_GATEWAY_PROGRAM,
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
int
do_null_op(CLIENT* c)
{
    void* null_args = NULL;
    void* res = bamboo_dht_proc_null_2(&null_args, c);
    return (res != NULL);
}


// test dht node, printing status messages
// if successful, addr contains a valid sockaddr_in
int
test_node(const char* hostname, struct sockaddr_in* addr)
{
    printf("   testing dht node %s... ", hostname);

    // try to get host addr
    if (!lookup_host(hostname, DHT_PORT, addr))
    {
        printf("lookup_host failed\n");
        return 0;
    }

    // try to connect to node
    // Note: This step seems to be insanely slow when it fails. Is there
    // a way to timeout faster?
    CLIENT* p_client = get_connection(addr);
    if (p_client == NULL)
    {
        printf("get_connection failed\n");
        return 0;
    }

    // try a null op
    if (!do_null_op(p_client))
    {
        printf("null_op failed\n");
        clnt_destroy(p_client);
        return 0;
    }

    printf("succeeded.\n");
    clnt_destroy(p_client);
    return 1;
}
