
#include <stdio.h>
#include <sys/errno.h>

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
    int ret;
    dtn_handle_t handle;
    dtn_tuple_t local_tuple;
    dtn_bundle_spec_t spec;
    dtn_bundle_payload_t payload;
    char* dest_tuple_str;
    
    if (argc != 2) {
        usage();
    }

    dest_tuple_str = (char*)argv[1];

    // open the ipc handle
    handle = dtn_open();
    if (handle == 0) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                strerror(errno));
        exit(1);
    }

    // get a local tuple
    dtn_build_local_tuple(handle, &local_tuple, "/ping");

    printf("local_tuple: [%s %s]\n",
           local_tuple.region, local_tuple.admin.admin_val);
    
    exit(1);

    // dtn_register(local_tuple);

    memset(&spec, 0, sizeof(spec));
    dtn_set_tuple_string(&spec.source, "dtn://test/source/ping");
    dtn_set_tuple_string(&spec.replyto, "dtn://test/source/ping");
    dtn_set_tuple_string(&spec.dest, "dtn://test/dest/ping");

    memset(&payload, 0, sizeof(payload));
    dtn_set_payload(&payload, DTN_PAYLOAD_MEM, "ping", 4);

    if ((ret = dtn_send(handle, &spec, &payload)) != 0) {
        fprintf(stderr, "error sending bundle: %d (%s)\n",
                ret, strerror(dtn_errno(handle)));
    }

    printf("successfully sent bundle\n");
    
    return 0;
}
