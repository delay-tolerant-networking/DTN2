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
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "dtn_api.h"

#define BUFSIZE 16
#define BUNDLE_DIR_DEFAULT "/var/dtn/dtncpd-incoming"

static const char *progname;

void
usage()
{
    fprintf(stderr, "usage: %s [ directory ]\n", progname);
    fprintf(stderr, "    optional directory parameter is where incoming "
                    "files will get put\n");
    fprintf(stderr, "    (defaults to: %s)\n", BUNDLE_DIR_DEFAULT);
    exit(1);
}

int
main(int argc, const char** argv)
{
    int i;
    int ret;
    dtn_handle_t handle;
    dtn_endpoint_id_t local_eid;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid;
    dtn_bundle_spec_t bundle;
    dtn_bundle_payload_t payload;
    char* endpoint;
    int debug = 1;

    char * bundle_dir = 0;

    char * region;
    char * host;
    char * dirpath;
    char * filename;
    char filepath[PATH_MAX];
    time_t current;

    char * buffer;
    char s_buffer[BUFSIZE + 1];

    int bufsize, marker, maxwrite;

    FILE * target;

    s_buffer[BUFSIZE] = '\0';

    progname = argv[0];
    
    if (argc > 2) {
        usage();
    }
    else if (argc == 2)
    {
        if (argv[1][0] == '-' && argv[1][1] == 'h') {
            usage();
        }
        bundle_dir = (char *) argv[1];
    }
    else
    {
        bundle_dir = BUNDLE_DIR_DEFAULT;
    }

    buffer = malloc(sizeof(char) * (strlen(bundle_dir) + 10));
    sprintf(buffer, "mkdir -p %s", bundle_dir);

    if (system(buffer) == -1)
    {
        fprintf(stderr, "Error opening bundle directory: %s\n", bundle_dir);
        exit(1);
    }
    free(buffer);

    // designated endpoint
    endpoint = "/dtncp/recv:*";

    // open the ipc handle
    if (debug) printf("opening connection to dtn router...\n");
    handle = dtn_open();
    if (handle == 0) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                strerror(errno));
        exit(1);
    }

    // build a local eid based on the configuration of our dtn
    // router plus the demux string
    dtn_build_local_eid(handle, &local_eid, endpoint);
    if (debug) printf("local_eid [%s]\n", local_eid.uri);

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
    while(1)
    {
        // change this to _MEM here to receive into memory then write the file
        // ourselves. (So this code shows both ways to do it.)
        // XXX/demmer better would be to have a cmd line option
        dtn_bundle_payload_location_t file_or_mem = DTN_PAYLOAD_FILE;

        memset(&bundle, 0, sizeof(bundle));
        memset(&payload, 0, sizeof(payload));
        
        if ((ret = dtn_recv(handle, &bundle,
                            file_or_mem, &payload, -1)) < 0)
        {
            fprintf(stderr, "error getting recv reply: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }

        // mark time received
        current = time(NULL);

        // grab the sending authority and service tag (i.e. the path)
        host = strstr(bundle.source.uri, ":") + 1;
        
        // extract directory from destination path (everything
        // following std demux)
        dirpath = strstr(bundle.dest.uri, "/dtncp/recv:");
        dirpath += 12; // skip /dtncp/recv:
        if (dirpath[0] == '/') dirpath++; // skip leading slash

        // filename is everything following last /
        filename = strrchr(dirpath, '/');
        if (filename == 0)
        {
            filename = dirpath;
            dirpath = "";
        }
        else
        {
            filename[0] = '\0'; // null terminate path
            filename++; // next char;
        }

        // recursively create full directory path
        // XXX/demmer system -- yuck!
        sprintf(filepath, "mkdir -p %s/%s/%s", bundle_dir, host, dirpath);
        system(filepath);
        
        // create file name
        sprintf(filepath, "%s/%s/%s/%s", bundle_dir, host, dirpath, filename);
        
        // bundle name is the name of the bundle payload file
        buffer = payload.dtn_bundle_payload_t_u.buf.buf_val;
        bufsize = payload.dtn_bundle_payload_t_u.buf.buf_len;

        printf ("======================================\n");
        printf (" File Received at %s\n", ctime(&current));
        printf ("   region : %s\n", region);
        printf ("   host   : %s\n", host);
        printf ("   path   : %s\n", dirpath);
        printf ("   file   : %s\n", filename);
        printf ("   size   : %d bytes\n", bufsize);
        printf ("   loc    : %s\n", filepath);
        
        if (debug) printf ("--------------------------------------\n");

        if (file_or_mem == DTN_PAYLOAD_FILE) {
            int cmdlen = 5 + strlen(buffer) + strlen(filepath);
            char *cmd = malloc(cmdlen);

            if (cmd) {
                snprintf(cmd, cmdlen, "mv %*s %s", bufsize, buffer,
                         filepath);
                printf("Moving payload to final filename: '%s'\n", cmd);
                system(cmd);
                free(cmd);
            } else {
                printf("Out of memory. Find file in %*s.\n", bufsize,
                        buffer);
            }
        } else {

            target = fopen(filepath, "w");

            if (target == NULL)
            {
                fprintf(stderr, "Error opening file for writing %s\n",
                         filepath);
                continue;
            }
        
            marker = 0;
            while (marker < bufsize)
            {
                // write 256 bytes at a time
                i=0;
                maxwrite = (marker + 256) > bufsize? bufsize-marker : 256;
                while (i < maxwrite)
                {
                    i += fwrite(buffer + marker + i, 1, maxwrite - i, target);
                }
            
                if (debug)
                {
                    for (i=0; i < maxwrite; i++)
                    {
                        if (buffer[marker] >= ' ' && buffer[marker] <= '~')
                            s_buffer[marker%BUFSIZE] = buffer[i];
                        else
                            s_buffer[marker%BUFSIZE] = '.';
                    
                        if (marker%BUFSIZE == 0) // new line every 16 bytes
                        {
                            printf("%07x ", marker);
                        }
                        else if (marker%2 == 0)
                        {
                            printf(" "); // space every 2 bytes
                        }
                    
                        printf("%02x", buffer[i] & 0xff);
                    
                        // print character summary (a la emacs hexl-mode)
                        if (marker%BUFSIZE == BUFSIZE-1)
                        {
                            printf(" |  %s\n", s_buffer);
                        }
                        marker ++;
                    }
                }
                else
                {
                    marker += maxwrite;
                }
            }
    
            fclose(target);
    
            // round off last line
            if (debug && marker % BUFSIZE != 0)
            {
                while (marker % BUFSIZE !=0)
                {
                    s_buffer[marker%BUFSIZE] = ' ';
    
                    if (marker%2 == 0)
                    {
                        printf(" "); // space every 2 bytes
                    }
                        
                    printf("  ");
                        
                    // print character summary (a la emacs hexl-mode)
                    if (marker%BUFSIZE == BUFSIZE-1)
                    {
                        printf(" |  %s\n", s_buffer);
                    }
                    marker ++;
                }
            }
        }
    
        printf ("======================================\n");
    }
    
    return 0;
}
