
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
    char b;
    dtn_handle_t handle;
    dtn_tuple_t local_tuple;
    dtn_bundle_spec_t spec;
    dtn_bundle_payload_t payload;
    char* dest_tuple_str;
    
    if (argc != 2) {
        usage();
    }

    // first off, make sure it's a valid destination tuple
    memset(&spec, 0, sizeof(spec));
    dest_tuple_str = (char*)argv[1];
    if (dtn_parse_tuple_string(&spec.dest, dest_tuple_str)) {
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

    // construct a local tuple based on the configuration of our dtn
    // router plus the demux string
    dtn_build_local_tuple(handle, &local_tuple, "/ping");

    // format the bundle header
    dtn_copy_tuple(&spec.source, &local_tuple);
    dtn_copy_tuple(&spec.replyto, &local_tuple);

    spec.dopts |= DOPTS_RETURN_RCPT;

    // and a payload of just the admin type code of 0x3
    b = 0x3;
    memset(&payload, 0, sizeof(payload));
    dtn_set_payload(&payload, DTN_PAYLOAD_MEM, &b, 1);

    if ((ret = dtn_send(handle, &spec, &payload)) != 0) {
        fprintf(stderr, "error sending bundle: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }

    printf("successfully sent bundle\n");
    
    return 0;
}
