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

#define BUFSIZE 16

const char *progname;

void
usage()
{
    fprintf(stderr, "usage: %s [endpoint]\n", progname);
    exit(1);
}

int
main(int argc, const char** argv)
{
    int i, k;
    int cnt = INT_MAX;
    int ret;
    dtn_handle_t handle;
    dtn_endpoint_id_t local_eid;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid;
    dtn_bundle_spec_t spec;
    dtn_bundle_payload_t payload;
    unsigned char* endpoint, *buffer;
    char s_buffer[BUFSIZE];
    int debug = 1;

    progname = argv[0];    
    if (argc != 2 || (argc == 2 && strncmp(argv[1], "-h", 2) == 0)) {
        usage();
    }

    endpoint = (unsigned char*)argv[1];

    // open the ipc handle
    if (debug) printf("opening connection to dtn router...\n");
    handle = dtn_open();
    if (debug) printf("opened connection to dtn router...\n");
    if (handle == 0) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                strerror(errno));
        exit(1);
    }

    // if the specified eid starts with '/', then build a local eid
    // based on the configuration of our dtn router plus the demux
    // string. otherwise make sure it's a valid one
    if (endpoint[0] == '/') {
        if (debug) printf("calling dtn_build_local_eid.\n");
        dtn_build_local_eid(handle, &local_eid, (char *) endpoint);
        if (debug) printf("local_eid [%s]\n", local_eid.uri);
    } else {
        if (debug) printf("calling parse_eid_string\n");
        if (dtn_parse_eid_string(&local_eid, endpoint)) {
            fprintf(stderr, "invalid destination endpoint '%s'\n",
                    endpoint);
            exit(1);
        }
    }

    // create a new registration based on this eid
    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_eid(&reginfo.endpoint, &local_eid);
    reginfo.action = DTN_REG_ABORT;
    reginfo.regid = DTN_REGID_NONE;
    reginfo.timeout = 60 * 60;
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }
    
    if (debug) printf("dtn_register succeeded, regid 0x%x\n", regid);

    // bind the current handle to the new registration
    dtn_bind(handle, regid);
    
    printf("dtn_recv [%s]...\n", local_eid.uri);
    
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

        printf("%d bytes from [%s]: transit time=%d ms\n",
               payload.dtn_bundle_payload_t_u.buf.buf_len,
               spec.source.uri, 0);

        buffer = (unsigned char *) payload.dtn_bundle_payload_t_u.buf.buf_val;
        for (k=0; k < payload.dtn_bundle_payload_t_u.buf.buf_len; k++)
        {
            if (buffer[k] >= ' ' && buffer[k] <= '~')
                s_buffer[k%BUFSIZE] = buffer[k];
            else
                s_buffer[k%BUFSIZE] = '.';

            if (k%BUFSIZE == 0) // new line every 16 bytes
            {
                printf("%07x ", k);
            }
            else if (k%2 == 0)
            {
                printf(" "); // space every 2 bytes
            }
                    
            printf("%02x", buffer[k] & 0xff);
                    
            // print character summary (a la emacs hexl-mode)
            if (k%BUFSIZE == BUFSIZE-1)
            {
                printf(" |  %.*s\n", BUFSIZE, s_buffer);
            }
        }

        // print spaces to fill out the rest of the line
	if (k%BUFSIZE != BUFSIZE-1) {
            while (k%BUFSIZE != BUFSIZE-1) {
                if (k%2 == 0) {
                    printf(" ");
                }
                printf("  ");
                k++;
            }
            printf("   |  %.*s\n",
              (int)payload.dtn_bundle_payload_t_u.buf.buf_len%BUFSIZE, 
                   s_buffer);
        }
	printf("\n");
    }
    
    return 0;
}
