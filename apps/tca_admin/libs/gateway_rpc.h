#ifndef _GATEWAY_RPC_H_
#define _GATEWAY_RPC_H_

#include <rpc/rpc.h>

#ifdef  __cplusplus
extern "C" {
#endif

extern int
lookup_host(const char* host, int port, struct sockaddr_in* addr);

extern CLIENT*
get_connection(struct sockaddr_in* addr);
    
extern int
do_null_op(CLIENT* c);

extern int
test_node(const char* hostname, struct sockaddr_in* addr);

#ifdef  __cplusplus
}
#endif

#endif /* _GATEWAY_RPC_H_ */
