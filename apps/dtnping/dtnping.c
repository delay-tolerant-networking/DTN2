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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "dtnping.h"
#include "dtn_api.h"

const char *progname;

// Daemon connection
int api_IP_set = 0;
char * api_IP = "127.0.0.1";
short api_port = 5010;

void
usage()
{
    fprintf(stderr, "usage: %s [-A api_IP] [-B api_port][-c count] [-i interval] [-e expiration] eid\n",
            progname);
    exit(1);
}

void doOptions(int argc, const char **argv);

int interval = 1;
int count = 0;
int reply_count = 0;
int expiration = 30;
char dest_eid_str[DTN_MAX_ENDPOINT_ID] = "";
char source_eid_str[DTN_MAX_ENDPOINT_ID] = "";
char replyto_eid_str[DTN_MAX_ENDPOINT_ID] = "";

#define MAX_PINGS_IN_FLIGHT 1024

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
    ping_payload_t payload_contents;
    ping_payload_t recv_contents;
    dtn_bundle_payload_t reply_payload;
    dtn_bundle_status_report_t* sr_data;
    dtn_bundle_id_t bundle_id;
    int debug = 1;
    char demux[64];
    int dest_len = 0;
    struct timeval send_times[MAX_PINGS_IN_FLIGHT];
    dtn_timestamp_t creation_times[MAX_PINGS_IN_FLIGHT];
    struct timeval now, recv_start, recv_end;
    u_int32_t nonce;
    u_int32_t seqno = 0;
    int timeout;

    // force stdout to always be line buffered, even if output is
    // redirected to a pipe or file
    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    
    doOptions(argc, argv);

    memset(&ping_spec, 0, sizeof(ping_spec));

    gettimeofday(&now, 0);
    srand(now.tv_sec);
    nonce = rand();
    
    // open the ipc handle
    int err = 0;
    if (api_IP_set) err = dtn_open_with_IP(api_IP,api_port,&handle);
    else err = dtn_open(&handle);

    if (err != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                dtn_strerror(err));
        exit(1);
    }

    // make sure they supplied a valid destination eid or
    // "localhost", in which case we just use the local daemon
    if (strcmp(dest_eid_str, "localhost") == 0) {
        dtn_build_local_eid(handle, &ping_spec.dest, "ping");
        
    } else {
        if (dtn_parse_eid_string(&ping_spec.dest, dest_eid_str)) {
            fprintf(stderr, "invalid destination eid string '%s'\n",
                    dest_eid_str);
            exit(1);
        }
    }

    dest_len = strlen(ping_spec.dest.uri);
    if ((dest_len < 4) ||
        (strcmp(ping_spec.dest.uri + dest_len - 4, "ping") != 0))
    {
        fprintf(stderr, "\nWARNING: ping destination does not end in \"ping\"\n\n");
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

    // set the source and replyto eids in the bundle spec
    if (debug) printf("source_eid [%s]\n", source_eid.uri);
    dtn_copy_eid(&ping_spec.source, &source_eid);
    dtn_copy_eid(&ping_spec.replyto, &source_eid);
    
    // now create a new registration based on the source
    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_eid(&reginfo.endpoint, &source_eid);
    reginfo.flags = DTN_REG_DEFER;
    reginfo.regid = DTN_REGID_NONE;
    reginfo.expiration = 0;
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }    
    if (debug) printf("dtn_register succeeded, regid %d\n", regid);

    // set the expiration time 
    ping_spec.expiration = expiration;

    printf("PING [%s] (expiration %u)...\n", ping_spec.dest.uri, expiration);
    if (interval == 0) {
        printf("WARNING: zero second interval will result in flooding pings!!\n");
    }
    
    // loop, sending pings and polling for activity
    for (i = 0; count == 0 || i < count; ++i) {
        gettimeofday(&send_times[seqno], NULL);
        
        // fill in a short payload string, a nonce, and a sequence number
        // to verify the echo feature and make sure we're not getting
        // duplicate responses or ping responses from another app
        memcpy(&payload_contents.ping, PING_STR, 8);
        payload_contents.seqno = seqno;
        payload_contents.nonce = nonce;
        payload_contents.time = send_times[seqno].tv_sec;
        
        memset(&ping_payload, 0, sizeof(ping_payload));
        dtn_set_payload(&ping_payload, DTN_PAYLOAD_MEM,
                        (char*)&payload_contents, sizeof(payload_contents));
        
        memset(&bundle_id, 0, sizeof(bundle_id));
        if ((ret = dtn_send(handle, regid, &ping_spec, &ping_payload,
                            &bundle_id)) != 0) {
            fprintf(stderr, "error sending bundle: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }
        
        creation_times[seqno] = bundle_id.creation_ts;

        memset(&reply_spec, 0, sizeof(reply_spec));
        memset(&reply_payload, 0, sizeof(reply_payload));

        // now loop waiting for replies / status reports until it's
        // time to send again, adding twice the expiration time if we
        // just sent the last ping
        timeout = interval * 1000;
        if (i == count - 1)
            timeout += expiration * 2000;
        
        do {
            gettimeofday(&recv_start, 0);
            if ((ret = dtn_recv(handle, &reply_spec,
                                DTN_PAYLOAD_MEM, &reply_payload, timeout)) < 0)
            {
                if (dtn_errno(handle) == DTN_ETIMEOUT) {
                    break; // time to send again
                }
                
                fprintf(stderr, "error getting ping reply: %d (%s)\n",
                        ret, dtn_strerror(dtn_errno(handle)));
                exit(1);
            }
            gettimeofday(&recv_end, 0);
            
            if (reply_payload.status_report != NULL)
            {
                sr_data = reply_payload.status_report;
                if (sr_data->flags != STATUS_DELETED) {
                    fprintf(stderr, "(bad status report from %s: flags 0x%x)\n",
                            reply_spec.source.uri, sr_data->flags);
                    goto next;
                }

                // find the seqno corresponding to the original
                // transmission time in the status report
                int j = 0;
                for (j = 0; j < MAX_PINGS_IN_FLIGHT; ++j) {
                    if (creation_times[j].secs  ==
                            sr_data->bundle_id.creation_ts.secs &&
                        creation_times[j].seqno ==
                            sr_data->bundle_id.creation_ts.seqno)
                    {
                        printf("bundle deleted at [%s] (%s): seqno=%d, time=%ld ms\n",
                               reply_spec.source.uri,
                               dtn_status_report_reason_to_str(sr_data->reason),
                               j, TIMEVAL_DIFF_MSEC(recv_end, send_times[j]));
                        goto next;
                    }
                }

                printf("bundle deleted at [%s] (%s): ERROR: can't find seqno\n",
                       reply_spec.source.uri, 
                       dtn_status_report_reason_to_str(sr_data->reason));
            }
            else {
                if (reply_payload.buf.buf_len != sizeof(ping_payload_t))
                {
                    printf("%d bytes from [%s]: ERROR: length != %zu\n",
                           reply_payload.buf.buf_len,
                           reply_spec.source.uri,
                           sizeof(ping_payload_t));
                    goto next;
                }

                memcpy(&recv_contents, reply_payload.buf.buf_val,
                       sizeof(recv_contents));
                
                if (recv_contents.seqno > MAX_PINGS_IN_FLIGHT)
                {
                    printf("%d bytes from [%s]: ERROR: invalid seqno %d\n",
                           reply_payload.buf.buf_len,
                           reply_spec.source.uri,
                           recv_contents.seqno);
                    goto next;
                }

                if (recv_contents.nonce != nonce)
                {
                    printf("%d bytes from [%s]: ERROR: invalid nonce %u != %u\n",
                           reply_payload.buf.buf_len,
                           reply_spec.source.uri,
                           recv_contents.nonce, nonce);
                    goto next;
                }

                if (recv_contents.time != (u_int32_t)send_times[recv_contents.seqno].tv_sec)
                {
                    printf("%d bytes from [%s]: ERROR: time mismatch -- "
                           "seqno %u reply time %u != send time %lu\n",
                           reply_payload.buf.buf_len,
                           reply_spec.source.uri,
                           recv_contents.seqno,
                           recv_contents.time,
                           (long unsigned int)send_times[recv_contents.seqno].tv_sec);
                    goto next;
                }
                
                printf("%d bytes from [%s]: '%.*s' seqno=%d, time=%ld ms\n",
                       reply_payload.buf.buf_len,
                       reply_spec.source.uri,
                       (u_int)strlen(PING_STR),
                       reply_payload.buf.buf_val,
                       recv_contents.seqno,
                       TIMEVAL_DIFF_MSEC(recv_end,
                                         send_times[recv_contents.seqno]));
                fflush(stdout);
            }
next:
            dtn_free_payload(&reply_payload);
            timeout -= TIMEVAL_DIFF_MSEC(recv_end, recv_start);

            // once we get status from all the pings we're supposed to
            // send, we're done
            reply_count++;
            if (count != 0 && reply_count == count) {
                break;
            }

        } while (timeout > 0);
        
        seqno++;
        seqno %= MAX_PINGS_IN_FLIGHT;
    }

    dtn_close(handle);

    return 0;
}

void
doOptions(int argc, const char **argv)
{
    int c;

    progname = argv[0];

    while ( (c=getopt(argc, (char **) argv, "A:B:hc:i:e:d:s:r:")) !=EOF ) {
        switch (c) {
        case 'A':
            api_IP_set = 1;
            api_IP = optarg;
            break;
        case 'B':
            api_port = atoi(optarg);
            break;    

        case 'c':
            count = atoi(optarg);
            break;
        case 'i':
            interval = atoi(optarg);
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

