#ifndef _APISERVER_H_
#define _APISERVER_H_

#include "dtn_api.h"
#include "dtn_ipc.h"
#include "dtn_types.h"
#include "debug/Log.h"
#include "thread/Thread.h"

class UDPClient;

/**
 * Class that implements the main server side handling of the DTN
 * application IPC.
 */
class APIServer {
public:
    APIServer();
    
protected:
    dtnipc_handle_t handle;

    char* buf_;
    size_t buflen_;
    UDPClient* sock_;
    XDR* xdr_encode_;
    XDR* xdr_decode_;
};

/**
 * Class for the generic listening server that just accepts open
 * messages and creates client servers.
 */
class MasterAPIServer : public APIServer, public Thread, public Logger {
public:
    MasterAPIServer();
    virtual void run();
};

/**
 * Class for the per-client connection server.
 */
class ClientAPIServer : public APIServer, public Thread, public Logger {
public:
    ClientAPIServer(in_addr_t remote_host, u_int16_t remote_port);
    virtual void run();

protected:
    int handle_getinfo();
    int handle_register();
    int handle_send();

    static const char* msgtoa(u_int32_t type);
};


#endif /* _APISERVER_H_ */
