
#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/time.h>
#include "dtn_api.h"

void
usage()
{
    fprintf(stderr, "usage: dtn_recv [endpoint]\n");
    exit(1);
}

int
main(int argc, const char** argv)
{
    int i;
    int cnt = INT_MAX;
    int ret;
    dtn_handle_t handle;
    dtn_tuple_t local_tuple;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid;
    dtn_bundle_spec_t spec;
    dtn_bundle_payload_t payload;
    char* endpoint;
    int debug = 1;
    
    if (argc != 2) {
        usage();
    }

    endpoint = (char*)argv[1];

    // open the ipc handle
    debug && printf("opening connection to dtn router...\n");
    handle = dtn_open();
    if (handle == 0) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                strerror(errno));
        exit(1);
    }

    // build a local tuple based on the configuration of our dtn
    // router plus the demux string
    dtn_build_local_tuple(handle, &local_tuple, endpoint);
    debug && printf("local_tuple [%s %.*s]\n",
                    local_tuple.region,
                    local_tuple.admin.admin_len, local_tuple.admin.admin_val);

    // create a new registration based on this tuple
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
    
    printf("dtn_recv [%s %.*s]...\n",
           local_tuple.region,
           local_tuple.admin.admin_len, local_tuple.admin.admin_val);
    
    // loop waiting for bundles
    for (i = 0; i < cnt; ++i) {
        memset(&spec, 0, sizeof(spec));
        memset(&payload, 0, sizeof(payload));
        
        if ((ret = dtn_recv(handle, &spec,
                            DTN_PAYLOAD_MEM, &payload, -1)) < 0)
        {
            fprintf(stderr, "error getting recv reply: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }

        printf("%d bytes from [%s %.*s]: transit time=%d ms\n",
               payload.dtn_bundle_payload_t_u.buf.buf_len,
               spec.source.region,
               spec.source.admin.admin_len,
               spec.source.admin.admin_val,
               0);
        
        sleep(1);
    }
    
    return 0;
}
