
#include "NetUtils.h"
#include "UDPServer.h"
#include "UDPClient.h"
#include "debug/Debug.h"
#include "debug/Log.h"
#include "conv_layers/IPConvergenceLayer.h"

#include <sys/errno.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

//Generous max. (w.r.t ethernet MTU)
#define MAX_UDP_PACKET_SIZE 1024 
//XXX/Nabeel: Needs to be set to max. 
//udp segment size (possibly specified
//in bundleNode.conf?)

UDPServer::UDPServer(char* logbase)
    : IPSocket(logbase, SOCK_DGRAM)
{
    params_.reuseaddr_ = 1;
}

int
UDPServer::RcvMessage(int *fd, in_addr_t *addr, u_int16_t *port, char** pt_payload, size_t* payload_len)
{

    struct sockaddr_in sa;
    socklen_t sl = sizeof(sa);
    memset(&sa, 0, sizeof(sa));   
    // allocate and fill a buffer with all of the packet data
    size_t buflen = MAX_UDP_PACKET_SIZE;
    (*pt_payload) = (char*)malloc(buflen);
    //to check for bit discarding...
    int flags = MSG_TRUNC;
    // read the whole packet into local buffer
    int ret = ::recvfrom(fd_, (*pt_payload), buflen, flags, (sockaddr*)&sa, &sl);

    if (ret == -1) {   
        if (errno != EINTR)
            logf(LOG_ERR, "error in RcvMessage(): %s", strerror(errno));
        return ret;
    }
    else if ( ret == 0 ) {
      logf(LOG_ERR, "error in RcvMessage(): peer host has shutdown normally\n");
      return ret;
    }   
    else if ( ret > buflen ) { //We have thrown away some bits
      logf(LOG_ERR, "error in RcvMessage(): message too large to fit in buffer\n");
      return -1;
    }
    
    // return the ACTUAL number of bytes read
    (*payload_len) = ret;
    *addr = sa.sin_addr.s_addr;
    *port = ntohs(sa.sin_port);

    return ret;
}

int
UDPServer::timeout_RcvMessage(int *fd, in_addr_t *addr, u_int16_t *port,char** pt_payload, 
			      size_t* payload_len, int timeout_ms)
{
    int ret = poll(POLLIN, NULL, timeout_ms);

    if (ret < 0)  return IOERROR;
    if (ret == 0) return IOTIMEOUT;
    ASSERT(ret == 1);

    ret = RcvMessage(fd, addr, port, pt_payload, payload_len);

    if (ret < 0) {
        return IOERROR;
    }

    return 1; // done!
}

void
UDPServerThread::run()
{
  int fd;
  in_addr_t addr;
  u_int16_t port;
  char* pt_payload = NULL;
  size_t payload_len = 0;    
  int ret;

  while (1) {
        if (should_stop())
            break;
        ret = RcvMessage(&fd, &addr, &port, &pt_payload, &payload_len);
	if (ret <= 0 ) {	  
	  if (errno == EINTR) {
	    if (pt_payload)
	      free(pt_payload);
	    continue;
	  }
            logf(LOG_ERR, "error in RcvMessage(): %d %s", errno, strerror(errno));
	    if (pt_payload)
	      free(pt_payload);
            close();
            break;
        }
       
	logf(LOG_DEBUG, "Received data on fd %d from %s:%d",
	     fd, intoa(addr), port);	         
	ProcessData(pt_payload, payload_len);	
	//ProcessData has done the cleaning for us
  }
}
