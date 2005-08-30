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
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "dtn_api.h"

char *progname;
int verbose             = 1;

char data_source[1024]; // filename or message, depending on type

// specified options
char * arg_dest         = NULL;
char * arg_target       = NULL;

void parse_options(int, char**);
dtn_endpoint_id_t * parse_eid(dtn_handle_t handle, dtn_endpoint_id_t * eid, 
                          char * str);
void print_usage();
void print_eid(char * label, dtn_endpoint_id_t * eid);

int
main(int argc, char** argv)
{
    int ret;
    dtn_handle_t handle;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid = DTN_REGID_NONE;
    dtn_bundle_spec_t bundle_spec;
    dtn_bundle_spec_t reply_spec;
    dtn_bundle_payload_t send_payload;
    dtn_bundle_payload_t reply_payload;
    char demux[4096];
    struct timeval start, end;

/*     FILE * file; */
/*     //struct stat finfo; */
/*     char buffer[4096]; // max filesize to send is 4096 (temp) */
/*     int bufsize = 0; */
    
    parse_options(argc, argv);

    // open the ipc handle
    if (verbose) fprintf(stdout, "Opening connection to local DTN daemon\n");

    handle = dtn_open();
    if (handle == 0) 
    {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                strerror(errno));
        exit(1);
    }


    // ----------------------------------------------------
    // initialize bundle spec with src/dest/replyto
    // ----------------------------------------------------

    // initialize bundle spec
    memset(&bundle_spec, 0, sizeof(bundle_spec));

    // destination host is specified at run time, demux is hardcoded
    sprintf(demux, "%s/dtncp/recv:/%s", arg_dest, arg_target);
    parse_eid(handle, &bundle_spec.dest, demux);

    // source is local eid with file path as demux string
    sprintf(demux, "/dtncp/send:%s", data_source);
    parse_eid(handle, &bundle_spec.source, demux);

    // reply to is the same as the source
    dtn_copy_eid(&bundle_spec.replyto, &bundle_spec.source);


    if (verbose)
    {
        print_eid("source_eid", &bundle_spec.source);
        print_eid("replyto_eid", &bundle_spec.replyto);
        print_eid("dest_eid", &bundle_spec.dest);
    }


    // set the expiration time (one hour)
    bundle_spec.expiration = 3600;
    
    // set the return receipt option
    bundle_spec.dopts |= DOPTS_DELIVERY_RCPT;

    // fill in a payload
    memset(&send_payload, 0, sizeof(send_payload));

    dtn_set_payload(&send_payload, DTN_PAYLOAD_FILE,
        data_source, strlen(data_source));
     
    // send file and wait for reply

    // create a new dtn registration to receive bundle status reports
    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_eid(&reginfo.endpoint, &bundle_spec.replyto);
    reginfo.action = DTN_REG_ABORT;
    reginfo.regid = regid;
    reginfo.timeout = 60 * 60;
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration (id=%d): %d (%s)\n",
                regid, ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }
    
    if (verbose) printf("dtn_register succeeded, regid 0x%x\n", regid);

    // bind the current handle to the new registration
    dtn_bind(handle, regid);

    gettimeofday(&start, NULL); // timer

    if ((ret = dtn_send(handle, &bundle_spec, &send_payload)) != 0) {
        fprintf(stderr, "error sending file bundle: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }

    memset(&reply_spec, 0, sizeof(reply_spec));
    memset(&reply_payload, 0, sizeof(reply_payload));
            
    // now we block waiting for the echo reply
    if ((ret = dtn_recv(handle, &reply_spec,
                        DTN_PAYLOAD_MEM, &reply_payload, -1)) < 0)
    {
        fprintf(stderr, "error getting reply: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }
    gettimeofday(&end, NULL);


    printf("file sent successfully to [%s]: time=%.1f ms\n",
                   reply_spec.source.uri,
                   ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
                    (double)(end.tv_usec - start.tv_usec)/1000.0));
            
    return 0;
}

void print_usage()
{
    fprintf(stderr, "usage: %s filename bundles://region/host://admin "
                    "[remote-name]\n", progname);
    fprintf(stderr, "    Remote filename is optional; defaults to the "
                    "local filename.\n");
    
    exit(1);
}

void parse_options(int argc, char**argv)
{
    progname = argv[0];

    if (argc < 3)
    {
        print_usage();
        exit(1);
    }

    if (argv[1][0] == '/') sprintf(data_source, "%s", argv[1]);
    else sprintf(data_source, "%s/%s", getenv("PWD"), argv[1]);

    arg_dest = argv[2];
    if (argc > 3)
    {
        arg_target = argv[3];
    }
    else 
    {
        arg_target = strrchr(data_source, '/');
        if (arg_target == 0) arg_target = data_source;
    }
}

dtn_endpoint_id_t * parse_eid(dtn_handle_t handle, 
                          dtn_endpoint_id_t * eid, char * str)
{
    
    // try the string as an actual dtn eid
    if (!dtn_parse_eid_string(eid, str)) 
    {
        return eid;
    }
    // build a local eid based on the configuration of our dtn
    // router plus the str as demux string
    else if (!dtn_build_local_eid(handle, eid, str))
    {
        return eid;
    }
    else
    {
        fprintf(stderr, "invalid eid string '%s'\n", str);
        exit(1);
    }
}

void print_eid(char *  label, dtn_endpoint_id_t * eid)
{
    printf("%s [%s]\n", label, eid->uri);
}
    



