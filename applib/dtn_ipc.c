
#include "dtn_ipc.h"
#include "dtn_types.h"
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
 * Initialize the handle structure.
 */
int
dtnipc_open(dtnipc_handle_t* handle)
{
    int ret;
    
    // note that we leave four bytes free in the sender side buffer to
    // be used for the message type code
    xdrmem_create(&handle->xdr_encode, handle->buf + sizeof(u_int32_t),
                  DTN_MAX_API_MSG, XDR_ENCODE);
    
    xdrmem_create(&handle->xdr_decode, handle->buf,
                  DTN_MAX_API_MSG, XDR_DECODE);

    handle->sock = socket(PF_INET, SOCK_DGRAM, 0);

    if (handle->sock < 0)
    {
        handle->err = DTN_COMMERR;
        return -1;
    }

    // first send the session initiation to the server on the
    // handshake port
    memset(&handle->sa, 0, sizeof(handle->sa));
    handle->sa.sin_family = AF_INET;
    handle->sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    handle->sa.sin_port = htons(DTN_API_HANDSHAKE_PORT);

    u_int32_t handshake = DTN_OPEN;
    
    ret = sendto(handle->sock, &handshake, sizeof(handshake), 0,
                 (const struct sockaddr*)&handle->sa, sizeof(handle->sa));
    if (ret != sizeof(handshake)) {
        handle->err = DTN_COMMERR;
        return -1;
    }

    // wait for the handshake response, noting where it came from
    memset(&handle->sa, 0, sizeof(handle->sa));
    handle->sa_len = sizeof(handle->sa);
    ret = recvfrom(handle->sock, &handshake, sizeof(handshake), 0,
                   (struct sockaddr*)&handle->sa, &handle->sa_len);
    if (ret != sizeof(handshake)) {
        handle->err = DTN_COMMERR;
        return -1;
    }
    
    // now call connect to the session peer so can just use send and
    // recv from now on.
    if (connect(handle->sock, (struct sockaddr*)&handle->sa,
                handle->sa_len) < 0)
    {
        handle->err = DTN_COMMERR;
        return -1;
    }
    
    return 0;
}

/*
 * Send a message over the dtn ipc protocol.
 *
 * Returns 0 on success, -1 on error.
 */
int
dtnipc_send(dtnipc_handle_t* handle, dtnapi_message_type_t type)
{
    size_t len;
    u_int32_t typecode = type;

    // pack the message code in the first four bytes of the buffer,
    // and compute the total message length
    memcpy(handle->buf, &type, sizeof(typecode));
    len = xdr_getpos(&handle->xdr_encode) + sizeof(typecode);

    // send the message
    if (send(handle->sock, handle->buf, len, 0) != len)
    {
        handle->err = DTN_COMMERR;
        return -1;
    }

    return 0;
}

/*
 * Receive a message on the ipc channel. May block if there is no
 * pending message.
 *
 * Returns the length of the message on success, -1 on error.
 */
int
dtnipc_recv(dtnipc_handle_t* handle)
{
    int len;
    
    do {
        len = recv(handle->sock, handle->buf, DTN_MAX_API_MSG, 0);
    } while (len < 0 && errno == EINTR);

    if (len < 0) {
        handle->err = DTN_COMMERR;
        return -1;
    }

    return len;
}

