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
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include "dtn_api.h"

char *progname;
dtn_bundle_payload_location_t 
        payload_type    = 0;    // the type of data source for the bundle

// global options
int verbose             = 0;
int arg_source_port       = 0;
int sender               = 0;
int arg_dest_port         = 0;
char* arg_dest_host       = NULL;
char* arg_dtn_peer        = NULL;

dtn_reg_id_t regid      = DTN_REGID_NONE;

void parse_options(int, char**);
dtn_endpoint_id_t * parse_eid(dtn_handle_t handle,
                          dtn_endpoint_id_t * eid, 
                          char * str);
void print_usage();

int
main(int argc, char** argv)
{
    int ret;
    dtn_handle_t handle;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid;
    dtn_bundle_spec_t bundle_spec;
    dtn_bundle_payload_t send_payload;
    int sock;

    struct sockaddr_in  local_addr, remote_addr;
    
    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
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
    if (verbose) fprintf(stdout, "Got connection to local DTN daemon\n");


    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0) {
        fprintf(stderr, "Error opening socket \n");
        exit(1);
    }

    if(sender) {
        char buf[2500];
        int rc;

        printf("Sender mode.\n");

        
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = htons(INADDR_ANY);
        local_addr.sin_port = htons(arg_source_port);

        if(bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
            perror("dtntunnel");
            fprintf(stderr, "Error binding socket to port %d \n",arg_source_port);
            exit(1);
        }

        while((rc=recv(sock, buf, 2500, 0))) {
            
            fprintf(stdout, "Got %d byte UDP packet, forwarding to DTN router.\n", rc);

            // initialize bundle spec
            memset(&bundle_spec, 0, sizeof(bundle_spec));
            
            if (verbose) fprintf(stdout, "DTN peer: %s\n", arg_dtn_peer);
            parse_eid(handle, &bundle_spec.dest, arg_dtn_peer);         
            
            // assign a standard source eid for now
            dtn_build_local_eid(handle, &bundle_spec.source, "dtntunnel");
            dtn_build_local_eid(handle, &bundle_spec.replyto, "dtntunnel");

            // set a default expiration time of one hour
            bundle_spec.expiration = 3600;
            
            // fill in a payload
            memset(&send_payload, 0, sizeof(send_payload));
            
            dtn_set_payload(&send_payload, DTN_PAYLOAD_MEM, buf, rc);
            
            if ((ret = dtn_send(handle, &bundle_spec, &send_payload)) != 0) {
                fprintf(stderr, "error sending bundle: %d (%s)\n",
                        ret, dtn_strerror(dtn_errno(handle)));
                exit(1);
            }
        }
    }
    else
    {
        struct hostent *he;
        dtn_endpoint_id_t local_eid;
        dtn_build_local_eid(handle, &local_eid, "dtntunnel");


        printf("Receiver mode.\n");

        if (verbose) fprintf(stdout,"Registering with DTN daemon.");
        memset(&reginfo, 0, sizeof(reginfo));

        dtn_copy_eid(&reginfo.endpoint, &local_eid);
        reginfo.failure_action = DTN_REG_DEFER;
        reginfo.regid = DTN_REGID_NONE;
        reginfo.expiration = 30;
        if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
            fprintf(stderr, "error creating registration: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }
        if (verbose) fprintf(stdout, "Registered with DTN daemon");
                
        // bind the current handle to the new registration
        dtn_bind(handle, regid);

/*        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_port = htons(36293);

        if(bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
            perror("binding sender UDP socket");
            fprintf(stderr, "Error binding sender UDP socket port %d \n",arg__port);
            exit(1);
        }
*/
        remote_addr.sin_family = AF_INET;

        he=gethostbyname(arg_dest_host);
        memcpy(&remote_addr.sin_addr.s_addr, 
               he->h_addr_list[0], he->h_length);

        remote_addr.sin_port = htons(arg_dest_port);

        while ((ret = dtn_recv(handle, &bundle_spec,
                            DTN_PAYLOAD_MEM, &send_payload, -1)) >= 0)
        {
            printf("got packet from DTN, forwarding to UDP destination\n");
            if(sendto(sock, 
                      send_payload.dtn_bundle_payload_t_u.buf.buf_val,
                      send_payload.dtn_bundle_payload_t_u.buf.buf_len,
                      0,
                      (struct sockaddr*)&remote_addr, 
                      sizeof(remote_addr))<0)
            {
                perror("sending datagram");
                fprintf(stderr,"error sending UDP datagram\n");
                exit(1);
            }
        }

        printf("%d bytes from [%s]: transit time=%d ms\n",
               send_payload.dtn_bundle_payload_t_u.buf.buf_len,
               bundle_spec.source.uri, 0);
    }


    dtn_close(handle);
    return 0;
}

void print_usage()
{
    fprintf(stderr, "usage: %s "
            "-r <s/r> -s <source port> -d <dest host:port> -p <dtn peer>\n\n",
            progname);
    fprintf(stderr, "dtntunnel is used to create DTN tunnels for TCP/UDP traffic.\n");
    fprintf(stderr, " dtntunnel forwards data received from a specified source (DTN peer or source port)\n");
    fprintf(stderr, " to the specified destination (destination host:port or DTN peer)\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, " -r <r/s> Receiver or Sender role.");
    fprintf(stderr, " -p <EID> DTN tunnel peer\n");
    fprintf(stderr, " -d <host:port> TCP/UDP destination (if receiver role)\n");
    fprintf(stderr, " -s <port> TCP/UDP source (if sender role)\n");
//    fprintf(stderr, " -t <upd/tcp> IP protocol to use. TCP not implemented!\n");
    fprintf(stderr, " -v verbose\n");
    fprintf(stderr, " -h help\n");
    
    exit(1);
}

void parse_options(int argc, char**argv)
{
    char c, done = 0;

    progname = argv[0];

    while (!done)
    {
        c = getopt(argc, argv, "hHvp:d:s:r:");       
        switch (c)
        {
        case 'v':
            verbose = 1;
            break;
        case 'h':
        case 'H':
            print_usage();
            exit(0);
            return;
        case 'p':
            arg_dtn_peer = optarg;
            break;
        case 's':
            arg_source_port = atoi(optarg);
            break;
        case 'd':
            arg_dest_host = strtok(optarg,":");
            arg_dest_port = atoi(strtok(0,":"));
            break;
        case 'r':
            sender = (strcmp("s",optarg) == 0);
            break;
        case -1:
            done = 1;
            break;
        default:
            // getopt already prints an error message for unknown
            // option characters
            fprintf(stderr, "default case\n");
            print_usage();
            exit(1);
        }
    }

#define CHECK_SET(_arg, _what)                                          \
    if (_arg == 0) {                                                    \
        fprintf(stderr, "dtnsend: %s must be specified\n", _what);      \
        print_usage();                                                  \
        exit(1);                                                        \
    }
    
    CHECK_SET(arg_dtn_peer,"source dtn peer");
    CHECK_SET(((arg_dest_host && arg_dest_port) || arg_source_port),"source or destination TCP/UDP endpoint");
}

dtn_endpoint_id_t * parse_eid(dtn_handle_t handle, 
                          dtn_endpoint_id_t* eid, char * str)
{
    
    // try the string as an actual dtn eid
    if (!dtn_parse_eid_string(eid, str)) 
    {
        if (verbose) fprintf(stdout, "%s (literal)\n", str);
        return eid;
    }
    // build a local eid based on the configuration of our dtn
    // router plus the str as demux string
    else if (!dtn_build_local_eid(handle, eid, str))
    {
        if (verbose) fprintf(stdout, "%s (local)\n", str);
        return eid;
    }
    else
    {
        fprintf(stderr, "invalid eid string '%s'\n", str);
        exit(1);
    }
}
