/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */


#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "dtn_api.h"

#define BUFSIZE 16

const char *progname;

int   verbose           = 0;    	// verbose output
int   quiet             = 0;    	// quiet output
char* endpoint		= NULL; 	// endpoint for registration
dtn_reg_id_t regid	= DTN_REGID_NONE;// registration id
int   expiration	= 30; 		// registration expiration time
int   count             = 0;            // exit after count bundles received
int   failure_action	= DTN_REG_DEFER;// registration delivery failure action
char* failure_script	= "";	 	// script to exec
int   register_only	= 0;    	// register and quit
int   change		= 0;    	// change existing registration 
int   unregister	= 0;    	// remove existing registration 
int   recv_timeout	= -1;    	// timeout to dtn_recv call
int   no_find_reg	= 0;    	// omit call to dtn_find_registration

void
usage()
{
    fprintf(stderr, "usage: %s [opts] <endpoint> \n", progname);
    fprintf(stderr, "options:\n");
    fprintf(stderr, " -v verbose\n");
    fprintf(stderr, " -q quiet\n");
    fprintf(stderr, " -h help\n");
    fprintf(stderr, " -d <eid|demux_string> endpoint id\n");
    fprintf(stderr, " -r <regid> use existing registration regid\n");
    fprintf(stderr, " -n <count> exit after count bundles received\n");
    fprintf(stderr, " -e <time> registration expiration time in seconds "
            "(default: one hour)\n");
    fprintf(stderr, " -f <defer|drop|exec> failure action\n");
    fprintf(stderr, " -F <script> failure script for exec action\n");
    fprintf(stderr, " -x call dtn_register and immediately exit\n");
    fprintf(stderr, " -c call dtn_change_registration and immediately exit\n");
    fprintf(stderr, " -u call dtn_unregister and immediately exit\n");
    fprintf(stderr, " -N don't try to find an existing registration\n");
    fprintf(stderr, " -t <timeout> timeout value for call to dtn_recv\n");
}

void
parse_options(int argc, char**argv)
{
    char c, done = 0;

    progname = argv[0];

    while (!done)
    {
        c = getopt(argc, argv, "vqhHd:r:e:f:F:xn:cuNt:");
        switch (c)
        {
        case 'v':
            verbose = 1;
            break;
        case 'q':
            quiet = 1;
            break;
        case 'h':
        case 'H':
            usage();
            exit(0);
            return;
        case 'r':
            regid = atoi(optarg);
            break;
        case 'e':
            expiration = atoi(optarg);
            break;
        case 'f':
            if (!strcasecmp(optarg, "defer")) {
                failure_action = DTN_REG_DEFER;

            } else if (!strcasecmp(optarg, "drop")) {
                failure_action = DTN_REG_DROP;

            } else if (!strcasecmp(optarg, "exec")) {
                failure_action = DTN_REG_EXEC;

            } else {
                fprintf(stderr, "invalid failure action '%s'\n", optarg);
                usage();
                exit(1);
            }
        case 'F':
            failure_script = optarg;
            break;
        case 'x':
            register_only = 1;
            break;
        case 'n':
            count = atoi(optarg);
            break;
        case 'c':
            change = 1;
            break;
        case 'u':
            unregister = 1;
            break;
        case 'N':
            no_find_reg = 1;
            break;
        case 't':
            recv_timeout = atoi(optarg);
            break;
        case -1:
            done = 1;
            break;
        default:
            // getopt already prints an error message for unknown
            // option characters
            usage();
            exit(1);
        }
    }

    endpoint = argv[optind];
    if (!endpoint && (regid == DTN_REGID_NONE)) {
        fprintf(stderr, "must specify either an endpoint or a regid\n");
        usage();
        exit(1);
    }

    if ((change || unregister) && (regid == DTN_REGID_NONE)) {
        fprintf(stderr, "must specify regid when using -%c option\n",
                change ? 'c' : 'u');
        usage();
        exit(1);
    }

    if (failure_action == DTN_REG_EXEC && failure_script == NULL) {
        fprintf(stderr, "failure action EXEC must supply script\n");
        usage();
        exit(1);
    }
}


int
main(int argc, char** argv)
{
    int i;
    u_int k;
    int ret;
    int total_bytes = 0;
    dtn_handle_t handle;
    dtn_endpoint_id_t local_eid;
    dtn_reg_info_t reginfo;
    dtn_bundle_spec_t spec;
    dtn_bundle_payload_t payload;
    unsigned char* buffer;
    char s_buffer[BUFSIZE];
    int call_bind;

    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
    progname = argv[0];

    parse_options(argc, argv);

    if (count == 0) { 
        printf("dtnrecv (pid %d) starting up\n", getpid());
    } else {
        printf("dtnrecv (pid %d) starting up -- count %u\n", getpid(), count);
    }

    // open the ipc handle
    if (verbose) printf("opening connection to dtn router...\n");
    int err = dtn_open(&handle);
    if (err != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                dtn_strerror(err));
        exit(1);
    }
    if (verbose) printf("opened connection to dtn router...\n");

    // if we're not given a regid, or we're in change mode, we need to
    // build up the reginfo
    memset(&reginfo, 0, sizeof(reginfo));

    if ((regid == DTN_REGID_NONE) || change)
    {
        // if the specified eid starts with '/', then build a local
        // eid based on the configuration of our dtn router plus the
        // demux string. otherwise make sure it's a valid one
        if (endpoint[0] == '/') {
            if (verbose) printf("calling dtn_build_local_eid.\n");
            dtn_build_local_eid(handle, &local_eid, (char *) endpoint);
            if (verbose) printf("local_eid [%s]\n", local_eid.uri);
        } else {
            if (verbose) printf("calling parse_eid_string\n");
            if (dtn_parse_eid_string(&local_eid, endpoint)) {
                fprintf(stderr, "invalid destination endpoint '%s'\n",
                        endpoint);
                goto err;
            }
        }

        // create a new registration based on this eid
        dtn_copy_eid(&reginfo.endpoint, &local_eid);
        reginfo.regid             = regid;
        reginfo.expiration        = expiration;
        reginfo.failure_action    = failure_action;
        reginfo.script.script_val = failure_script;
        reginfo.script.script_len = strlen(failure_script) + 1;
    }

    if (change) {
        if ((ret = dtn_change_registration(handle, regid, &reginfo)) != 0) {
            fprintf(stderr, "error changing registration: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            goto err;
        }
        printf("change registration succeeded, regid %d\n", regid);
        goto done;
    }
    
    if (unregister) {
        if (dtn_unregister(handle, regid) != 0) {
            fprintf(stderr, "error in unregister regid %d: %s\n",
                    regid, dtn_strerror(dtn_errno(handle)));
            goto err;
        }
        
        printf("unregister succeeded, regid %d\n", regid);
        goto done;
    }
    
    // try to see if there is an existing registration that matches
    // the given endpoint, in which case we'll use that one.
    if (regid == DTN_REGID_NONE && ! no_find_reg) {
        if (dtn_find_registration(handle, &local_eid, &regid) != 0) {
            if (dtn_errno(handle) != DTN_ENOTFOUND) {
                fprintf(stderr, "error in find_registration: %s\n",
                        dtn_strerror(dtn_errno(handle)));
                goto err;
            }
        }
        printf("find registration succeeded, regid %d\n", regid);
        call_bind = 1;
    }
    
    // if the user didn't give us a registration to use, get a new one
    if (regid == DTN_REGID_NONE) {
        if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
            fprintf(stderr, "error creating registration: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            goto err;
        }

        printf("register succeeded, regid %d\n", regid);
        call_bind = 0;
    } else {
        call_bind = 1;
    }
    
    if (register_only) {
        goto done;
    }

    if (call_bind) {
        // bind the current handle to the found registration
        printf("binding to regid %d\n", regid);
        if (dtn_bind(handle, regid) != 0) {
            fprintf(stderr, "error binding to registration: %s\n",
                    dtn_strerror(dtn_errno(handle)));
            goto err;
        }
    }

    // loop waiting for bundles
    if (count == 0) {
        printf("looping forever to receive bundles\n");
    } else {
        printf("looping to receive %d bundles\n", count);
    }
    for (i = 0; (count == 0) || (i < count); ++i) {
        memset(&spec, 0, sizeof(spec));
        memset(&payload, 0, sizeof(payload));

        if (!quiet) {
            printf("dtn_recv [%s]...\n", local_eid.uri);
        }
    
        if ((ret = dtn_recv(handle, &spec,
                            DTN_PAYLOAD_MEM, &payload, recv_timeout)) < 0)
        {
            fprintf(stderr, "error getting recv reply: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            goto err;
        }

        total_bytes += payload.dtn_bundle_payload_t_u.buf.buf_len;

        if (quiet) {
            dtn_free_payload(&payload);
            continue;
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

        dtn_free_payload(&payload);
    }

    printf("dtnrecv (pid %d) exiting: %d bundles received, %d total bytes\n\n",
           getpid(), i, total_bytes);

 done:
    dtn_close(handle);
    return 0;

 err:
    dtn_close(handle);
    return 1;
}
