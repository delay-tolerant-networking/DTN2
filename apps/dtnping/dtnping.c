/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
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
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/time.h>

#include "dtn_api.h"

void
usage()
{
    fprintf(stderr, "usage: ping [-c count] [-i interval] tuple\n");
    exit(1);
}

void doOptions(int argc, const char **argv);

int sleepVal = 1;
int count = 0;
char dest_tuple_str[DTN_MAX_TUPLE] = "";

int
main(int argc, const char** argv)
{
    int i;
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
    int debug = 1;

    struct timeval start, end;
    
    doOptions(argc, argv);

    // first off, make sure it's a valid destination tuple
    memset(&ping_spec, 0, sizeof(ping_spec));
    //dest_tuple_str = (char*)argv[1];
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
    for (i = 0; count==0 || i < count; ++i) {
        gettimeofday(&start, NULL);
        
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
        gettimeofday(&end, NULL);

        printf("%d bytes from [%s %.*s]: time=%0.2f ms\n",
               reply_payload.dtn_bundle_payload_t_u.buf.buf_len,
               reply_spec.source.region,
               reply_spec.source.admin.admin_len,
               reply_spec.source.admin.admin_val,
               ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
                (double)(end.tv_usec - start.tv_usec)/1000.0));
        
        sleep(sleepVal);
    }
    
    return 0;
}

void
doOptions(int argc, const char **argv)
{
    int c;
    while ( (c=getopt(argc, (char **) argv, "c:i:d:")) !=EOF ) {
        switch (c) {
        case 'c':
            count = atoi(optarg);
            break;
        case 'i':
            sleepVal = atoi(optarg);
            break;
        case 'd':
            strcpy(dest_tuple_str, optarg);
            break;
        }
    }
    if ( strlen(dest_tuple_str)==0 ) {
            strcpy(dest_tuple_str, argv[argc-1]);
    }
}

