
#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/time.h>

#include "dtn_api.h"

void
usage()
{
    fprintf(stderr, "usage: ping [tuple]\n");
    exit(1);
}

int
main(int argc, const char** argv)
{
    int i;
    int cnt = INT_MAX;
    int ret;
    char b;
    dtn_handle_t handle;
    dtn_tuple_t local_tuple;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid;
    dtn_bundle_spec_t ping_spec;
    dtn_bundle_spec_t reply_spec;
    dtn_bundle_payload_t ping_payload;
    dtn_bundle_payload_t reply_payload;
    char* dest_tuple_str;
    int debug = 1;
    
    if (argc != 2) {
        usage();
    }

    // first off, make sure it's a valid destination tuple
    memset(&ping_spec, 0, sizeof(ping_spec));
    dest_tuple_str = (char*)argv[1];
    if (dtn_parse_tuple_string(&ping_spec.dest, dest_tuple_str)) {
        fprintf(stderr, "invalid destination tuple string '%s'\n",
                dest_tuple_str);
        exit(1);
    }

    // open the ipc handle
    handle = dtn_open();
    if (handle == 0) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                strerror(errno));
        exit(1);
    }

    // build a local tuple based on the configuration of our dtn
    // router plus the demux string
    dtn_build_local_tuple(handle, &local_tuple, "/ping");
    debug && printf("local_tuple [%s %.*s]\n",
                    local_tuple.region,
                    local_tuple.admin.admin_len, local_tuple.admin.admin_val);

    // create a new dtn registration based on this tuple
    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_tuple(&reginfo.endpoint, &local_tuple);
    reginfo.action = DTN_REG_ABORT;
    reginfo.regid = DTN_REGID_NONE;
    reginfo.timeout = 60 * 60;
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }
    
    debug && printf("dtn_register succeeded, regid 0x%x\n", regid);

    // bind the current handle to the new registration
    dtn_bind(handle, regid, &local_tuple);
    
    // format the bundle header
    dtn_copy_tuple(&ping_spec.source, &local_tuple);
    dtn_copy_tuple(&ping_spec.replyto, &local_tuple);

    // set the return receipt option
    ping_spec.dopts |= DOPTS_RETURN_RCPT;

    // fill in a payload of a single type code of 0x3 (echo request)
    b = 0x3;
    memset(&ping_payload, 0, sizeof(ping_payload));
    dtn_set_payload(&ping_payload, DTN_PAYLOAD_MEM, &b, 1);
    
    printf("PING [%s %.*s]...\n",
           ping_spec.dest.region,
           ping_spec.dest.admin.admin_len, ping_spec.dest.admin.admin_val);
    
    // loop, sending pings and getting replies.
    for (i = 0; i < cnt; ++i) {
        if ((ret = dtn_send(handle, &ping_spec, &ping_payload)) != 0) {
            fprintf(stderr, "error sending bundle: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }

        memset(&reply_spec, 0, sizeof(reply_spec));
        memset(&reply_payload, 0, sizeof(reply_payload));
        
        // now we block waiting for the echo reply
        if ((ret = dtn_recv(handle, &reply_spec,
                            DTN_PAYLOAD_MEM, &reply_payload, -1)) < 0)
        {
            fprintf(stderr, "error getting ping reply: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }

        printf("%d bytes from [%s %.*s]: time=%d ms\n",
               reply_payload.dtn_bundle_payload_t_u.buf.buf_len,
               reply_spec.source.region,
               reply_spec.source.admin.admin_len,
               reply_spec.source.admin.admin_val,
               0);
        
        sleep(1);
    }
    
    return 0;
}
