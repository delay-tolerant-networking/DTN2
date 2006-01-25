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

const char *progname;

void
usage()
{
    fprintf(stderr, "usage: %s [-c count] [-i interval] [-e expiration] eid\n",
            progname);
    exit(1);
}

void doOptions(int argc, const char **argv);

int sleepVal = 1;
int count = 0;
int expiration = 30;
char dest_eid_str[DTN_MAX_ENDPOINT_ID] = "";
char source_eid_str[DTN_MAX_ENDPOINT_ID] = "";
char replyto_eid_str[DTN_MAX_ENDPOINT_ID] = "";
char* payload_str = "dtn_ping!";

int
main(int argc, const char** argv)
{
    int i;
    int ret;
    dtn_handle_t handle;
    dtn_endpoint_id_t source_eid;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid;
    dtn_bundle_spec_t ping_spec;
    dtn_bundle_spec_t reply_spec;
    dtn_bundle_payload_t ping_payload;
    dtn_bundle_payload_t reply_payload;
    int debug = 1;
    char demux[64];
    char payload_buf[1024];

    struct timeval start, end;
    
    doOptions(argc, argv);

    memset(&ping_spec, 0, sizeof(ping_spec));

    // open the ipc handle
    handle = dtn_open();
    if (handle == 0) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                strerror(errno));
        exit(1);
    }

    // make sure they supplied a valid destination eid or
    // "localhost", in which case we just use the local daemon
    if (strcmp(dest_eid_str, "localhost") == 0) {
        dtn_build_local_eid(handle, &ping_spec.dest, "");
        
    } else {
        if (dtn_parse_eid_string(&ping_spec.dest, dest_eid_str)) {
            fprintf(stderr, "invalid destination eid string '%s'\n",
                    dest_eid_str);
            exit(1);
        }
    }
    
    // if the user specified a source eid, register on it.
    // otherwise, build a local eid based on the configuration of
    // our dtn router plus the demux string
    snprintf(demux, sizeof(demux), "/ping.%d", getpid());
    if (source_eid_str[0] != '\0') {
        if (dtn_parse_eid_string(&source_eid, source_eid_str)) {
            fprintf(stderr, "invalid source eid string '%s'\n",
                    source_eid_str);
            exit(1);
        }
    } else {
        dtn_build_local_eid(handle, &source_eid, demux);
    }

    // set the source eid in the bundle spec
    if (debug) printf("source_eid [%s]\n", source_eid.uri);
    dtn_copy_eid(&ping_spec.source, &source_eid);
    
    // now parse the replyto eid, if specified. otherwise just use
    // the source eid
    if (replyto_eid_str[0] != '\0') {
        if (dtn_parse_eid_string(&ping_spec.replyto, replyto_eid_str)) {
            fprintf(stderr, "invalid replyto eid string '%s'\n",
                    replyto_eid_str);
            exit(1);
        }
        
    } else {
        dtn_copy_eid(&ping_spec.replyto, &source_eid);
    }

    if (debug) printf("replyto_eid [%s]\n", ping_spec.replyto.uri);
    
    // now create a new registration based on the source
    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_eid(&reginfo.endpoint, &source_eid);
    reginfo.failure_action = DTN_REG_DEFER;
    reginfo.regid = DTN_REGID_NONE;
    reginfo.expiration = 30;
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }    
    if (debug) printf("dtn_register succeeded, regid 0x%x\n", regid);

    // bind the current handle to the new registration
    dtn_bind(handle, regid);

    // check for any bundles queued for the registration
    if (debug) printf("checking for bundles already queued...\n");
    do {
        memset(&reply_spec, 0, sizeof(reply_spec));
        memset(&reply_payload, 0, sizeof(reply_payload));
        
        ret = dtn_recv(handle, &reply_spec,
                       DTN_PAYLOAD_MEM, &reply_payload, 0);

        if (ret == 0) {
            fprintf(stderr, "error: unexpected ping already queued... "
                    "discarding\n");
        } else if (dtn_errno(handle) != DTN_ETIMEOUT) {
            fprintf(stderr, "error: "
                    "unexpected error checking for queued bundles: %s\n",
                    dtn_strerror(dtn_errno(handle)));
            exit(1);
        }
    } while (ret == 0);
    
    // set the expiration time
    ping_spec.expiration = expiration;

    // fill in a payload of a single type code of 0x3 (echo request)
    // and no flags, followed by a short payload string to verify the
    // echo feature
    payload_buf[0] = 0x3 << 4;
    strcpy(&payload_buf[1], payload_str);

    memset(&ping_payload, 0, sizeof(ping_payload));
    dtn_set_payload(&ping_payload, DTN_PAYLOAD_MEM, payload_buf, 1 + strlen(payload_str));
    
    printf("PING [%s]...\n", ping_spec.dest.uri);
    
    // loop, sending pings and getting replies.
    for (i = 0; count == 0 || i < count; ++i) {
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

        printf("%d bytes from [%s]: '%.*s' time=%0.2f ms\n",
               reply_payload.dtn_bundle_payload_t_u.buf.buf_len,
               reply_spec.source.uri,
               reply_payload.dtn_bundle_payload_t_u.buf.buf_len - 1,
               reply_payload.dtn_bundle_payload_t_u.buf.buf_val + 1,
               ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
                (double)(end.tv_usec - start.tv_usec)/1000.0));
        fflush(stdout);
        
        sleep(sleepVal);
    }

    dtn_close(handle);
    
    return 0;
}

void
doOptions(int argc, const char **argv)
{
    int c;

    progname = argv[0];

    while ( (c=getopt(argc, (char **) argv, "hc:i:e:d:s:r:")) !=EOF ) {
        switch (c) {
        case 'c':
            count = atoi(optarg);
            break;
        case 'i':
            sleepVal = atoi(optarg);
            break;
        case 'e':
            expiration = atoi(optarg);
            break;
        case 'd':
            strcpy(dest_eid_str, optarg);
            break;
        case 's':
            strcpy(source_eid_str, optarg);
            break;
        case 'r':
            strcpy(replyto_eid_str, optarg);
            break;
        case 'h':
            usage();
            break;
        default:
            break;
        }
    }

    if ((optind < argc) && (strlen(dest_eid_str) == 0)) {
        strcpy(dest_eid_str, argv[optind++]);
    }

    if (optind < argc) {
        fprintf(stderr, "unsupported argument '%s'\n", argv[optind]);
        exit(1);
    }

    if (dest_eid_str[0] == '\0') {
        fprintf(stderr, "must supply a destination eid (or 'localhost')\n");
        exit(1);
    }
}

