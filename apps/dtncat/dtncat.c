/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2006 Intel Corporation. All rights reserved. 
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

/*
 * dtncat: move stdin to bundles and vice-versa
 * resembles the nc (netcat) unix program
 * - kfall Apr 2006
 *
 *   Usage: dtncat [-options] EID
 *   dtncat -l EID
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "dtn_api.h"

char *progname;

// global options
int copies              = 1;    // the number of copies to send
int verbose             = 0;

// bundle options
int expiration          = 3600; // expiration timer (default one hour)
int delivery_receipts   = 0;    // request end to end delivery receipts
int forwarding_receipts = 0;    // request per hop departure
int custody             = 0;    // request custody transfer
int custody_receipts    = 0;    // request per custodian receipts
int receive_receipts    = 0;    // request per hop arrival receipt
int overwrite           = 0;    // queue overwrite option
int wait_for_report     = 0;    // wait for bundle status reports

// specified options for bundle eids
char * arg_replyto      = NULL;
char * arg_source       = NULL;
char * arg_dest         = NULL;

dtn_reg_id_t regid      = DTN_REGID_NONE;

void parse_options(int, char**);
dtn_endpoint_id_t * parse_eid(dtn_handle_t handle,
                              dtn_endpoint_id_t * eid,
                              char * str);
void print_usage();
void print_eid(FILE*, char * label, dtn_endpoint_id_t * eid);
int fill_payload(dtn_bundle_payload_t* payload);

FILE* info;
#define REG_EXPIRE (60 * 60)
char payload_buf[DTN_MAX_BUNDLE_MEM];

int
main(int argc, char** argv)
{
    int ret;
    dtn_handle_t handle;
    dtn_reg_info_t reginfo;
    dtn_bundle_spec_t bundle_spec;
    dtn_bundle_spec_t reply_spec;
    dtn_bundle_payload_t send_payload;
    dtn_bundle_payload_t reply_payload;
    struct timeval start, end;

    info = stderr;
    
    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file -- why?
    // setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
    parse_options(argc, argv);

    // open the ipc handle
    if (verbose)
	fprintf(info, "Opening connection to local DTN daemon\n");

    handle = dtn_open();
    if (handle == 0) {
        fprintf(stderr, "%s: fatal error opening dtn handle: %s\n",
                progname, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // initialize bundle spec
    memset(&bundle_spec, 0, sizeof(bundle_spec));

    // initialize/parse bundle src/dest/replyto eids
    if (verbose)
	fprintf(info, "Destination: %s\n", arg_dest);
    parse_eid(handle, &bundle_spec.dest, arg_dest);

    if (verbose) fprintf(info, "Source: %s\n", arg_source);
    parse_eid(handle, &bundle_spec.source, arg_source);
    if (arg_replyto == NULL) {
        if (verbose)
		fprintf(info, "Reply To: same as source\n");
        dtn_copy_eid(&bundle_spec.replyto, &bundle_spec.source);
    } else {
        if (verbose)
		fprintf(info, "Reply To: %s\n", arg_replyto);
        parse_eid(handle, &bundle_spec.replyto, arg_replyto);
    }

    if (verbose) {
        print_eid(info, "source_eid", &bundle_spec.source);
        print_eid(info, "replyto_eid", &bundle_spec.replyto);
        print_eid(info, "dest_eid", &bundle_spec.dest);
    }

    if (wait_for_report) {
        // create a new dtn registration to receive bundle status reports
        memset(&reginfo, 0, sizeof(reginfo));
        dtn_copy_eid(&reginfo.endpoint, &bundle_spec.replyto);
        reginfo.failure_action = DTN_REG_DROP;
        reginfo.regid = regid;
        reginfo.expiration = REG_EXPIRE;
	if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
            fprintf(stderr, "%s: error creating registration (id=0x%x): %d (%s)\n",
                    progname, regid, ret, dtn_strerror(dtn_errno(handle)));
            exit(EXIT_FAILURE);
        }
    
        if (verbose)
		fprintf(info, "dtn_register succeeded, regid 0x%x\n", regid);

        // bind the current handle to the new registration
        dtn_bind(handle, regid);
    }
    
    // set the dtn options
    bundle_spec.expiration = expiration;
    
    if (delivery_receipts) {
        // set the delivery receipt option
        bundle_spec.dopts |= DOPTS_DELIVERY_RCPT;
    }

    if (forwarding_receipts) {
        // set the forward receipt option
        bundle_spec.dopts |= DOPTS_FORWARD_RCPT;
    }

    if (custody) {
        // request custody transfer
        bundle_spec.dopts |= DOPTS_CUSTODY;
    }

    if (custody_receipts) {
        // request custody transfer
        bundle_spec.dopts |= DOPTS_CUSTODY_RCPT;
    }

    if (receive_receipts) {
        // request receive receipt
        bundle_spec.dopts |= DOPTS_RECEIVE_RCPT;
    }
    
    if (gettimeofday(&start, NULL) < 0) {
		fprintf(stderr, "%s: gettimeofday(start) returned error %s\n",
			progname, strerror(errno));
		exit(EXIT_FAILURE);
    }

	    if (fill_payload(&send_payload) < 0) {
		fprintf(stderr, "%s: error reading bundle data\n",
		    progname);
		exit(EXIT_FAILURE);
	    }
		
	    if ((ret = dtn_send(handle, &bundle_spec, &send_payload)) != 0) {
		fprintf(stderr, "error sending bundle: %d (%s)\n",
		    ret, dtn_strerror(dtn_errno(handle)));
		exit(EXIT_FAILURE);
	    }

	    if (wait_for_report) {
		memset(&reply_spec, 0, sizeof(reply_spec));
		memset(&reply_payload, 0, sizeof(reply_payload));
		
		// now we block waiting for any replies
		if ((ret = dtn_recv(handle, &reply_spec,
				    DTN_PAYLOAD_MEM, &reply_payload, -1)) < 0) {
		    fprintf(stderr, "%s: error getting reply: %d (%s)\n",
			    progname, ret, dtn_strerror(dtn_errno(handle)));
		    exit(EXIT_FAILURE);
		}
		if (gettimeofday(&end, NULL) < 0) {
		    fprintf(stderr, "%s: gettimeofday(end) returned error %s\n",
			    progname, strerror(errno));
		    exit(EXIT_FAILURE);
		}
	    
		fprintf(info, "got %d byte report from [%s]: time=%.1f ms\n",
		       reply_payload.dtn_bundle_payload_t_u.buf.buf_len,
		       reply_spec.source.uri,
		       ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
			(double)(end.tv_usec - start.tv_usec)/1000.0));
	    }

    dtn_close(handle);
    return (EXIT_SUCCESS);
}

void print_usage()
{
    fprintf(stderr, "usage: %s [opts] "
            "-s <source_eid> -d <dest_eid>\n",
            progname);
    fprintf(stderr, "options:\n");
    fprintf(stderr, " -v verbose\n");
    fprintf(stderr, " -h help\n");
    fprintf(stderr, " -H help\n");
    fprintf(stderr, " -s <eid|demux_string> source eid)\n");
    fprintf(stderr, " -d <eid|demux_string> destination eid)\n");
    fprintf(stderr, " -r <eid|demux_string> reply to eid)\n");
    fprintf(stderr, " -e <time> expiration time in seconds (default: one hour)\n");
    fprintf(stderr, " -i <regid> registration id for reply to\n");
    fprintf(stderr, " -c request custody transfer\n");
    fprintf(stderr, " -C request custody transfer receipts\n");
    fprintf(stderr, " -D request for end-to-end delivery receipt\n");
    fprintf(stderr, " -R request for bundle reception receipts\n");
    fprintf(stderr, " -F request for bundle forwarding receipts\n");
    fprintf(stderr, " -w wait for bundle status reports\n");
    
    return;
}

void
parse_options(int argc, char**argv)
{
    char c, done = 0;
    char arg_type = 0;

    progname = argv[0];

    while (!done) {
        c = getopt(argc, argv, "vhHr:s:d:e:wDFRcCi:");
        switch (c) {
        case 'v':
            verbose = 1;
            break;
        case 'h':
        case 'H':
            print_usage();
            exit(EXIT_SUCCESS);
            return;
        case 'r':
            arg_replyto = optarg;
            break;
        case 's':
            arg_source = optarg;
            break;
        case 'd':
            arg_dest = optarg;
            break;
        case 'e':
            expiration = atoi(optarg);
            break;
        case 'w':
            wait_for_report = 1;
            break;
        case 'D':
            delivery_receipts = 1;
            break;
        case 'F':
            forwarding_receipts = 1;
            break;
        case 'R':
            receive_receipts = 1;
            break;
        case 'c':
            custody = 1;
            break;
        case 'C':
            custody_receipts = 1;
            break;
        case 'i':
            regid = atoi(optarg);
            break;
        case -1:
            done = 1;
            break;
        default:
            // getopt already prints an error message for unknown
            // option characters
            print_usage();
            exit(EXIT_FAILURE);
        }
    }

#define CHECK_SET(_arg, _what)                                          \
    if (_arg == 0) {                                                    \
        fprintf(stderr, "%s: %s must be specified\n", progname, _what); \
        print_usage();                                                  \
        exit(EXIT_FAILURE);                                             \
    }
    
    CHECK_SET(arg_source,   "source eid");
    CHECK_SET(arg_dest,     "destination eid");
}

dtn_endpoint_id_t *
parse_eid(dtn_handle_t handle, dtn_endpoint_id_t* eid, char * str)
{
    // try the string as an actual dtn eid
    if (!dtn_parse_eid_string(eid, str)) {
        if (verbose)
		fprintf(info, "%s (literal)\n", str);
        return eid;
    } else if (!dtn_build_local_eid(handle, eid, str)) {
        // build a local eid based on the configuration of our dtn
        // router plus the str as demux string
        if (verbose) fprintf(info, "%s (local)\n", str);
        return eid;
    } else {
        fprintf(stderr, "invalid eid string '%s'\n", str);
        exit(EXIT_FAILURE);
    }
}

void
print_eid(FILE *dest, char *  label, dtn_endpoint_id_t * eid)
{
    fprintf(dest, "%s [%s]\n", label, eid->uri);
}

/*
 * read from stdin to get the payload
 */

int
fill_payload(dtn_bundle_payload_t* payload)
{

   char buf[BUFSIZ];
   char *p = payload_buf;
   size_t n, total = 0;
   int maxread = sizeof(buf);

   while (1) {
       if ((p + maxread) > ((char *)(payload_buf + 1)))
	       maxread = ((char *)(payload_buf+1)) - p;
       if (((n = fread(buf, maxread, 1, stdin))) == 0)
	       break;
       memcpy(p, buf, n);
       p += n;
       total += n;
   }
   if (ferror(stdin))
       return (-1); 

   if (dtn_set_payload(payload, DTN_PAYLOAD_MEM, payload_buf, total) == DTN_ESIZE)
	   return (-1);
   return(total);
}
