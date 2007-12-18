/*
 *    Copyright 2005-2006 Intel Corporation
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

/* ----------------------
 *    dtnperf-client.c
 * ---------------------- */

/* -----------------------------------------------------------
 *  PLEASE NOTE: this software was developed
 *    by Piero Cornice <piero.cornice@gmail.com>
 *    at DEIS - University of Bologna, Italy.
 *  If you want to modify it, please contact me
 *  at piero.cornice(at)gmail.com. Thanks =)
 * -----------------------------------------------------------
 */

/*
 * Modified slightly (and renamed) by Michael Demmer
 * <demmer@cs.berkeley.edu> to fit in with the DTN2
 * source distribution.
 */

/* version 1.6.0 - 23/06/06
 *
 * - compatible with DTN 2.2.0 reference implementation
 * - fixed measure units errors
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
#include <sys/file.h>
#include <time.h>
#include <assert.h>

#include "dtn_api.h"
#include "dtn_types.h"

#define MAX_MEM_PAYLOAD 50000 // max payload (in bytes) if bundles are stored into memory
#define ILLEGAL_PAYLOAD 0     // illegal number of bytes for the bundle payload
#define DEFAULT_PAYLOAD 50000 // default value (in bytes) for bundle payload

/* ---------------------------------------------
 * Values inside [square brackets] are defaults
 * --------------------------------------------- */

char *progname;

// global options
dtn_bundle_payload_location_t 
        payload_type    = DTN_PAYLOAD_FILE;    // the type of data source for the bundle [FILE]
int verbose             = 0;    // if set to 1, execution becomes verbose (-v option) [0]
char op_mode               ;    // operative mode (t = time_mode, d = data_mode)
int debug               = 0;    // if set to 1, many debug messages are shown [0]
int csv_out             = 0;    // if set to 1, a Comma-Separated-Values output is shown [0]
/* -----------------------------------------------------------------------
 * NOTE - CSV output shows the following columns:
 *  Time-Mode: BUNDLES_SENT, PAYLOAD, TIME, DATA_SENT, GOODPUT
 *  Data-Mode: BUNDLE_ID, PAYLOAD, TIME, GOODPUT
 * ----------------------------------------------------------------------- */

// bundle options
int expiration          = 3600; // expiration time (sec) [3600]
int delivery_receipts   = 1;    // request delivery receipts [1]
int forwarding_receipts = 0;    // request per hop departure [0]
int custody             = 0;    // request custody transfer [0]
int custody_receipts    = 0;    // request per custodian receipts [0]
int receive_receipts    = 0;    // request per hop arrival receipt [0]
//int overwrite           = 0;    // queue overwrite option [0]
int wait_for_report     = 1;    // wait for bundle status reports [1]

// specified options for bundle tuples
char * arg_replyto      = NULL; // replyto_tuple
char * arg_source       = NULL; // source_tuple
char * arg_dest         = NULL; // destination_tuple

dtn_reg_id_t regid      = DTN_REGID_NONE;   // registration ID (-i option)
long bundle_payload     = DEFAULT_PAYLOAD;  // quantity of data (in bytes) to send (-p option)
char * p_arg              ;                 // argument of -p option

// Time-Mode options
int transmission_time   = 0;    // seconds of transmission

// Data-Mode options
long data_qty           = 0;    // data to be transmitted (bytes)
char * n_arg               ;    // arguments of -n option
char * p_arg               ;    // arguments of -p option
int n_copies            = 1;    // number of trasmissions [1]
int sleepVal            = 0;    // seconds to sleep between transmissions in Data-Mode [0]
int use_file            = 1;    // if set to 1, a file is used instead of memory [1]
char data_unit             ;    // B = bytes, K = kilobytes, M = megabytes

// Data-Mode variables
int fd                     ;    // file descriptor, used with -f option
int data_written        = 0;    // data written on the file
int data_read           = 0;    // data read from the file
char * file_name_src    = INSTALL_LOCALSTATEDIR "/dtn/dtnperf/dtnbuffer.snd";    // name of the SOURCE file to be used

/* -------------------------------
 *       function interfaces
 * ------------------------------- */
void parse_options(int, char**);
dtn_endpoint_id_t* parse_eid(dtn_handle_t handle, dtn_endpoint_id_t * eid, char * str);
void print_usage(char* progname);
void print_eid(char * label, dtn_endpoint_id_t * eid);
void pattern(char *outBuf, int inBytes);
struct timeval set(double sec);
struct timeval add(double sec);
void show_report (u_int buf_len, char* eid, struct timeval start, struct timeval end, int data);
void csv_time_report(int b_sent, int payload, struct timeval start, struct timeval end);
void csv_data_report(int b_id, int payload, struct timeval start, struct timeval end);
long bundles_needed (long data, long pl);
void check_options();
void show_options();
void add_time(struct timeval *tot_time, struct timeval part_time);
long mega2byte(long n);
long kilo2byte(long n);
char findDataUnit(const char *inarg);

/* -------------------------------------------------
 * main
 * ------------------------------------------------- */
int main(int argc, char** argv)
{
    /* -----------------------
     *  variables declaration
     * ----------------------- */
    int ret;                        // result of DTN-registration
    struct timeval start, end,
                   p_start, p_end, now; // time-calculation variables

    int i, j;                       // loop-control variables
    const char* time_report_hdr = "BUNDLE_SENT,PAYLOAD,TIME,DATA_SENT,GOODPUT";
    const char* data_report_hdr = "BUNDLE_ID,PAYLOAD,TIME,GOODPUT";
    int n_bundles = 0;              // number of bundles needed (Data-Mode)
    
    // DTN variables
    dtn_handle_t        handle;
    dtn_reg_info_t      reginfo;
    dtn_bundle_spec_t   bundle_spec;
    dtn_bundle_spec_t   reply_spec;
    dtn_bundle_id_t     bundle_id;
    dtn_bundle_payload_t send_payload;
    dtn_bundle_payload_t reply_payload;
    char demux[64];

    // buffer specifications
    char* buffer = NULL;            // buffer containing data to be transmitted
    int bufferLen;                  // lenght of buffer
    int bundles_sent;               // number of bundles sent in Time-Mode
    
    /* -------
     *  begin
     * ------- */

    // print information header
    printf("\nDTNperf - CLIENT - v 1.6.0");
    printf("\nwritten by piero.cornice@gmail.com");
    printf("\nDEIS - University of Bologna, Italy");
    printf("\n");

    // parse command-line options
    parse_options(argc, argv);
    if (debug) printf("[debug] parsed command-line options\n");

    // check command-line options
    if (debug) printf("[debug] checking command-line options...");
    check_options();
    if (debug) printf(" done\n");

    // show command-line options (if verbose)
    if (verbose) {
        show_options();
    }

    // open the ipc handle
    if (debug) fprintf(stdout, "Opening connection to local DTN daemon...");
    int err = dtn_open(&handle);
    if (err != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n", dtn_strerror(err));
        exit(1);
    }
    if (debug) printf(" done\n");


    /* ----------------------------------------------------- *
     *   initialize and parse bundle src/dest/replyto EIDs   *
     * ----------------------------------------------------- */

    // initialize bundle spec
    if (debug) printf("[debug] memset for bundle_spec...");
    memset(&bundle_spec, 0, sizeof(bundle_spec));
    if (debug) printf(" done\n");

    // SOURCE is local eid + demux string (optionally with file path)
    sprintf(demux, "/dtnperf:/src");
    dtn_build_local_eid(handle, &bundle_spec.source, demux);
    if (verbose) printf("\nSource     : %s\n", bundle_spec.source.uri);

    // DEST host is specified at run time, demux is hardcoded
    sprintf(demux, "/dtnperf:/dest");
    strcat(arg_dest, demux);
    parse_eid(handle, &bundle_spec.dest, arg_dest);
    if (verbose) printf("Destination: %s\n", bundle_spec.dest.uri);

    // REPLY-TO (if none specified, same as the source)
    if (arg_replyto == NULL) {
        if (debug) printf("[debug] setting replyto = source...");
        dtn_copy_eid(&bundle_spec.replyto, &bundle_spec.source);
        if (debug) printf(" done\n");
    }
    else {
        sprintf(demux, "/dtnperf:/src");
        strcat(arg_replyto, demux);
        parse_eid(handle, &bundle_spec.dest, arg_replyto);
    }
    if (verbose) printf("Reply-to   : %s\n\n", bundle_spec.replyto.uri);

    /* ------------------------
     * set the dtn options
     * ------------------------ */
    if (debug) printf("[debug] setting the DTN options: ");

    // expiration
    bundle_spec.expiration = expiration;

    // set the delivery receipt option
    if (delivery_receipts) {
        bundle_spec.dopts |= DOPTS_DELIVERY_RCPT;
        if (debug) printf("DELIVERY_RCPT ");
    }

    // set the forward receipt option
    if (forwarding_receipts) {
        bundle_spec.dopts |= DOPTS_FORWARD_RCPT;
        if (debug) printf("FORWARD_RCPT ");
    }

    // request custody transfer
    if (custody) {
        bundle_spec.dopts |= DOPTS_CUSTODY;
        if (debug) printf("CUSTODY ");
    }

    // request custody transfer
    if (custody_receipts) {
        bundle_spec.dopts |= DOPTS_CUSTODY_RCPT;
        if (debug) printf("CUSTODY_RCPT ");
    }

    // request receive receipt
    if (receive_receipts) {
        bundle_spec.dopts |= DOPTS_RECEIVE_RCPT;
        if (debug) printf("RECEIVE_RCPT ");
    }

/*
    // overwrite
    if (overwrite) {
        bundle_spec.dopts |= DOPTS_OVERWRITE;
        if (debug) printf("OVERWRITE ");
    }
*/
    if (debug) printf("option(s) set\n");

    /* ----------------------------------------------
     * create a new registration based on the source
     * ---------------------------------------------- */
    if (debug) printf("[debug] memset for reginfo...");
    memset(&reginfo, 0, sizeof(reginfo));
    if (debug) printf(" done\n");

    if (debug) printf("[debug] copying bundle_spec.replyto to reginfo.endpoint...");
    dtn_copy_eid(&reginfo.endpoint, &bundle_spec.replyto);
    if (debug) printf(" done\n");

    if (debug) printf("[debug] setting up reginfo...");
    reginfo.failure_action = DTN_REG_DEFER;
    reginfo.regid = regid;
    reginfo.expiration = 30;
    if (debug) printf(" done\n");

    if (debug) printf("[debug] registering to local daemon...");
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }    
    if (debug) printf(" done: regid 0x%x\n", regid);

    // if bundle_payload > MAX_MEM_PAYLOAD, transfer a file
    if (bundle_payload > MAX_MEM_PAYLOAD)
        use_file = 1;
    else
        use_file = 0;
    
    /* ------------------------------------------------------------------------------
     * select the operative-mode (between Time_Mode and Data_Mode)
     * ------------------------------------------------------------------------------ */

    if (op_mode == 't') {
    /* ---------------------------------------
     * Time_Mode
     * --------------------------------------- */
        if (verbose) printf("Working in Time_Mode\n");
        if (verbose) printf("requested %d second(s) of transmission\n", transmission_time);

        if (debug) printf("[debug] bundle_payload %s %d bytes\n",
                            use_file ? ">=" : "<",
                            MAX_MEM_PAYLOAD);
        if (verbose) printf(" transmitting data %s\n", use_file ? "using a file" : "using memory");

        // reset data_qty
        if (debug) printf("[debug] reset data_qty and bundles_sent...");
        data_qty = 0;
        bundles_sent = 0;
        if (debug) printf(" done\n");

        // allocate buffer space
        if (debug) printf("[debug] malloc for the buffer...");
        buffer = malloc(bundle_payload);
        if (debug) printf(" done\n");
        
        // initialize buffer
        if (debug) printf("[debug] initialize the buffer with a pattern...");
        pattern(buffer, bundle_payload);
        if (debug) printf(" done\n");
        bufferLen = strlen(buffer);
        if (debug) printf("[debug] bufferLen = %d\n", bufferLen);

        if (use_file) {
            // create the file
            if (debug) printf("[debug] creating file %s...", file_name_src);
            fd = open(file_name_src, O_CREAT|O_TRUNC|O_WRONLY|O_APPEND, 0666);
            if (fd < 0) {
                fprintf(stderr, "ERROR: couldn't create file %s [fd = %d]: %s\n",
                        file_name_src, fd, strerror(errno));
                exit(2);
            }
            if (debug) printf(" done\n");

            // fill in the file with a pattern
            if (debug) printf("[debug] filling the file (%s) with the pattern...", file_name_src);
            data_written += write(fd, buffer, bufferLen);
            if (debug) printf(" done. Written %d bytes\n", data_written);

            // close the file
            if (debug) printf("[debug] closing file (%s)...", file_name_src);
            close(fd);
            if (debug) printf(" done\n");
        }

        // memset for payload
        if (debug) printf("[debug] memset for payload...");
        memset(&send_payload, 0, sizeof(send_payload));
        if (debug) printf(" done\n");

        // fill in the payload
        if (debug) printf("[debug] filling payload...");
        if (use_file)
            dtn_set_payload(&send_payload, DTN_PAYLOAD_FILE, file_name_src, strlen(file_name_src));
        else
            dtn_set_payload(&send_payload, DTN_PAYLOAD_MEM, buffer, bufferLen);
        if (debug) printf(" done\n");

        // initialize timer
        if (debug) printf("[debug] initializing timer...");
        gettimeofday(&start, NULL);
        if (debug) printf(" start.tv_sec = %d sec\n", (u_int)start.tv_sec);

        // calculate end-time
        if (debug) printf("[debug] calculating end-time...");
        end = set (0);
        end.tv_sec = start.tv_sec + transmission_time;
        if (debug) printf(" end.tv_sec = %d sec\n", (u_int)end.tv_sec);

        // loop
        if (debug) printf("[debug] entering loop...\n");
        for (now.tv_sec = start.tv_sec; now.tv_sec <= end.tv_sec; gettimeofday(&now, NULL)) {

            if (debug) printf("\t[debug] now.tv_sec = %u sec of %u\n", (u_int)now.tv_sec, (u_int)end.tv_sec);

            // send the bundle
            if (debug) printf("\t[debug] sending the bundle...");
            memset(&bundle_id, 0, sizeof(bundle_id));
            if ((ret = dtn_send(handle, &bundle_spec, &send_payload, &bundle_id)) != 0) {
                fprintf(stderr, "error sending bundle: %d (%s)\n",
                        ret, dtn_strerror(dtn_errno(handle)));
                exit(1);
            }
            if (debug) printf(" bundle sent\n");

            // increment bundles_sent
            bundles_sent++;
            if (debug) printf("\t[debug] now bundles_sent is %d\n", bundles_sent);
    
            // increment data_qty
            data_qty += bundle_payload;
            if (debug) printf("\t[debug] now data_qty is %lu\n", data_qty);

            // prepare memory for the reply
            if (wait_for_report)
            {
                if (debug) printf("\t[debug] memset for reply_spec...");
                memset(&reply_spec, 0, sizeof(reply_spec));
                if (debug) printf(" done\n");
                if (debug) printf("\t[debug] memset for reply_payload...");
                memset(&reply_payload, 0, sizeof(reply_payload));
                if (debug) printf(" done\n");
            }
            
            // wait for the reply
            if (debug) printf("\t[debug] waiting for the reply...");
            if ((ret = dtn_recv(handle, &reply_spec, DTN_PAYLOAD_MEM, &reply_payload, -1)) < 0)
            {
                fprintf(stderr, "error getting reply: %d (%s)\n", ret, dtn_strerror(dtn_errno(handle)));
                exit(1);
            }
            if (debug) printf(" reply received\n");

            // get the PARTIAL end time
            if (debug) printf("\t[debug] getting partial end-time...");
            gettimeofday(&p_end, NULL);
            if (debug) printf(" end.tv_sec = %u sec\n", (u_int)p_end.tv_sec);

            if (debug) printf("----- END OF THIS LOOP -----\n\n");
        } // -- for
        if (debug) printf("[debug] out from loop\n");

        // deallocate buffer memory
        if (debug) printf("[debug] deallocating buffer memory...");
        free((void*)buffer);
        if (debug) printf(" done\n");

        // get the TOTAL end time
        if (debug) printf("[debug] getting total end-time...");
        gettimeofday(&end, NULL);
        if (debug) printf(" end.tv_sec = %u sec\n", (u_int)end.tv_sec);
    
        // show the report
        if (csv_out == 0) {
            printf("%d bundles sent, each with a %ld bytes payload\n", bundles_sent, bundle_payload);
            show_report(reply_payload.buf.buf_len,
                        reply_spec.source.uri,
                        start,
                        end,
                        data_qty);
        }
        if (csv_out == 1) {
            printf("%s\n", time_report_hdr);
            csv_time_report(bundles_sent, bundle_payload, start, end);
        }

    } // -- time_mode

    else if (op_mode == 'd') {
    /* ---------------------------------------
     * Data_Mode
     * --------------------------------------- */
        if (verbose) printf("Working in Data_Mode\n");

        // initialize the buffer
        if (debug) printf("[debug] initializing buffer...");
        if (!use_file) {
            buffer = malloc( (data_qty < bundle_payload) ? data_qty : bundle_payload );
            memset(buffer, 0, (data_qty < bundle_payload) ? data_qty : bundle_payload );
            pattern(buffer, (data_qty < bundle_payload) ? data_qty : bundle_payload );
        }
        if (use_file) {
            buffer = malloc(data_qty);
            memset(buffer, 0, data_qty);
            pattern(buffer, data_qty);
        }
        bufferLen = strlen(buffer);
        if (debug) printf(" done. bufferLen = %d (should equal %s)\n",
                            bufferLen, use_file ? "data_qty" : "bundle_payload");

        if (use_file) {
            // create the file
            if (debug) printf("[debug] creating file %s...", file_name_src);
            fd = open(file_name_src, O_CREAT|O_TRUNC|O_WRONLY|O_APPEND, 0666);
            if (fd < 0) {
                fprintf(stderr, "ERROR: couldn't create file [fd = %d]. Maybe you don't have permissions\n", fd);
                exit(2);
            }
            if (debug) printf(" done\n");

            // fill in the file with a pattern
            if (debug) printf("[debug] filling the file (%s) with the pattern...", file_name_src);
            data_written += write(fd, buffer, bufferLen);
            if (debug) printf(" done. Written %d bytes\n", data_written);

            // close the file
            if (debug) printf("[debug] closing file (%s)...", file_name_src);
            close(fd);
            if (debug) printf(" done\n");
        }

        // fill in the payload
        if (debug) printf("[debug] filling the bundle payload...");
        memset(&send_payload, 0, sizeof(send_payload));
        if (use_file) {
            dtn_set_payload(&send_payload, DTN_PAYLOAD_FILE, file_name_src, strlen(file_name_src));
        } else {
            dtn_set_payload(&send_payload, DTN_PAYLOAD_MEM, buffer, bufferLen);
        }
        if (debug) printf(" done\n");

        // if CSV option is set, print the data_report_hdr
        if (csv_out == 1)
            printf("%s\n", data_report_hdr);

        // 1) If you're using MEMORY (-m option), the maximum data quantity is MAX_MEM_PAYLOAD bytes.
        //    So, if someone tries to send more data, you have to do multiple transmission
        //    in order to avoid daemon failure.
        //    This, however, doesn't affect the goodput measurement, since it is calculated
        //    for each transmission.
        // 2) If you are using FILE, you may want to send an amount of data
        //    using smaller bundles.
        // So it's necessary to calculate how many bundles are needed.
        if (debug) printf("[debug] calculating how many bundles are needed...");
        n_bundles = bundles_needed(data_qty, bundle_payload);
        if (debug) printf(" n_bundles = %d\n", n_bundles);

        // initialize TOTAL start timer
        if (debug) printf("[debug] initializing TOTAL start timer...");
        gettimeofday(&start, NULL);
        if (debug) printf(" start.tv_sec = %u sec\n", (u_int)start.tv_sec);

        if (debug) printf("[debug] entering n_copies loop...\n");
        // --------------- loop until all n_copies are sent
        for (i=0; i<n_copies; i++) {

                if (debug) printf("\t[debug] entering n_bundles loop...\n");
                for (j=0; j<n_bundles; j++) {

                    // initialize PARTIAL start timer
                    if (debug) printf("\t\t[debug] initializing PARTIAL start timer...");
                    gettimeofday(&p_start, NULL);
                    if (debug) printf(" p_start.tv_sec = %u sec\n", (u_int)p_start.tv_sec);

                    // send the bundle
                    if (debug) printf("\t\t[debug] sending copy %d...", i+1);
                    memset(&bundle_id, 0, sizeof(bundle_id));
                    if ((ret = dtn_send(handle, &bundle_spec, &send_payload, &bundle_id)) != 0) {
                        fprintf(stderr, "error sending bundle: %d (%s)\n",
                                ret, dtn_strerror(dtn_errno(handle)));
                        exit(1);
                    }
                    if (debug) printf(" bundle sent\n");

                    // prepare memory areas for the reply
                    if (wait_for_report)
                    {
                        if (debug) printf("\t\t[debug] setting memory for reply...");
                        memset(&reply_spec, 0, sizeof(reply_spec));
                        memset(&reply_payload, 0, sizeof(reply_payload));
                        if (debug) printf(" done\n");
                    }

                    // wait for the reply
                    if (debug) printf("\t\t[debug] waiting for the reply...");
                    if ((ret = dtn_recv(handle, &reply_spec, DTN_PAYLOAD_MEM, &reply_payload, -1)) < 0)
                    {
                        fprintf(stderr, "error getting reply: %d (%s)\n", ret, dtn_strerror(dtn_errno(handle)));
                        exit(1);
                    }
                    if (debug) printf(" reply received\n");

                    // get PARTIAL end time
                    if (debug) printf("\t\t[debug] stopping PARTIAL timer...");
                    gettimeofday(&p_end, NULL);
                    if (debug) printf(" p_end.tv_sec = %u sec\n", (u_int)p_end.tv_sec);

                    // show the PARTIAL report (verbose mode)
                    if (verbose) {
                        printf("[%d/%d] ", j+1, n_bundles);
                        show_report(reply_payload.buf.buf_len,
                                    bundle_spec.source.uri,
                                    p_start,
                                    p_end,
                                    ((bundle_payload <= data_qty)?bundle_payload:data_qty));
                    }
                } // end for(n_bundles)
                if (debug) printf("\t[debug] ...out from n_bundles loop\n");

            // calculate TOTAL end time
            if (debug) printf("\t[debug] calculating TOTAL end time...");
            gettimeofday(&end, NULL);
            if (debug) printf(" end.tv_sec = %u sec\n", (u_int)end.tv_sec);

            // show the TOTAL report
            if (csv_out == 0) {
                show_report(reply_payload.buf.buf_len,
                            reply_spec.source.uri,
                            start,
                            end,
                            data_qty);
            }
            if (csv_out == 1) {    
                csv_data_report(i+1, data_qty, start, end);
            }

            if (n_copies > 0)
                sleep(sleepVal);

        } // end for(n_copies)
        if (debug) printf("[debug] ...out from n_copies loop\n");
        // -------------------------- end of loop

        // deallocate buffer memory
        if (debug) printf("[debug] deallocating buffer memory...");
        free(buffer);
        if (debug) printf(" done\n");

    } // -- data_mode

    else {        // this should not be executed (written only for debug purpouse)
        fprintf(stderr, "ERROR: invalid operative mode! Specify -t or -n\n");
        exit(3);
    }

    // close dtn-handle -- IN DTN_2.1.1 SIMPLY RETURNS -1
    if (debug) printf("[debug] closing DTN handle...");
    if (dtn_close(handle) != DTN_SUCCESS)
    {
        fprintf(stderr, "fatal error closing dtn handle: %s\n",
                strerror(errno));
        exit(1);
    }
    if (debug) printf(" done\n");

    // final carriage return
    printf("\n");

    return 0;
} // end main



/* ----------------------------------------
 *           UTILITY FUNCTIONS
 * ---------------------------------------- */

/* ----------------------------
 * print_usage
 * ---------------------------- */
void print_usage(char* progname)
{
    fprintf(stderr, "\nSYNTAX: %s "
            "-d <dest_eid> "
            "[-t <sec> | -n <num>] [options]\n\n", progname);
    fprintf(stderr, "where:\n");
    fprintf(stderr, " -d <eid> destination eid (required)\n");
    fprintf(stderr, " -t <sec> Time-Mode: seconds of transmission\n");
    fprintf(stderr, " -n <num> Data-Mode: number of MBytes to send\n");
    fprintf(stderr, "Options common to both Time and Data Mode:\n");
    fprintf(stderr, " -p <size> size in KBytes of bundle payload\n");
    fprintf(stderr, " -r <eid> reply-to eid (if none specified, source tuple is used)\n");
    fprintf(stderr, "Data-Mode options:\n");
    fprintf(stderr, " -m use memory instead of file\n");
    fprintf(stderr, " -B <num> number of consecutive transmissions (default 1)\n");
    fprintf(stderr, " -S <sec> sleeping seconds between consecutive transmissions (default 1)\n");
    fprintf(stderr, "Other options:\n");
    fprintf(stderr, " -c CSV output (useful with redirection of the output to a file)\n");
    fprintf(stderr, " -h help: show this message\n");
    fprintf(stderr, " -v verbose\n");
    fprintf(stderr, " -D debug messages (many)\n");
    fprintf(stderr, " -F enables forwarding receipts\n");
    fprintf(stderr, " -R enables receive receipts\n");
    fprintf(stderr, "\n");
    exit(1);
} // end print_usage


/* ----------------------------
 * parse_options
 * ---------------------------- */
void parse_options(int argc, char**argv)
{
    int c, done = 0;

    while (!done)
    {
        c = getopt(argc, argv, "hvDcmr:d:i:t:p:n:S:B:FRf:");
        switch (c)
        {
        case 'v':
            verbose = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            exit(0);
            return;
        case 'c':
            csv_out = 1;
            break;
        case 'r':
            arg_replyto = optarg;
            break;
        case 'd':
            arg_dest = optarg;
            break;
        case 'i':
            regid = atoi(optarg);
            break;
        case 'D':
            debug = 1;
            break;
        case 't':
            op_mode = 't';
            transmission_time = atoi(optarg);
            break;
        case 'n':
            op_mode = 'd';
            n_arg = optarg;
            data_unit = findDataUnit(n_arg);
            switch (data_unit) {
                case 'B':
                    data_qty = atol(n_arg);
                    break;
                case 'K':
                    data_qty = kilo2byte(atol(n_arg));
                    break;
                case 'M':
                    data_qty = mega2byte(atol(n_arg));
                    break;
                default:
                    printf("\nWARNING: (-n option) invalid data unit, assuming 'M' (MBytes)\n\n");
                    data_qty = mega2byte(atol(n_arg));
                    break;
            }
            break;
        case 'p':
            p_arg = optarg;
            data_unit = findDataUnit(p_arg);
            switch (data_unit) {
                case 'B':
                    bundle_payload = atol(p_arg);
                    break;
                case 'K':
                    bundle_payload = kilo2byte(atol(p_arg));
                    break;
                case 'M':
                    bundle_payload = mega2byte(atol(p_arg));
                    break;
                default:
                    printf("\nWARNING: (-p option) invalid data unit, assuming 'K' (KBytes)\n\n");
                    bundle_payload = kilo2byte(atol(p_arg));
                    break;
            }
            break;
        case 'B':
            n_copies = atoi(optarg);
            break;
        case 'S':
            sleepVal = atoi(optarg);
            break;

        case 'f':
            use_file = 1;
            file_name_src = strdup(optarg);
            break;

        case 'm':
            use_file = 0;
            payload_type = DTN_PAYLOAD_MEM;
            break;

        case 'F':
            forwarding_receipts = 1;
            break;

        case 'R':
            receive_receipts = 1;
            break;

        case -1:
            done = 1;
            break;
        default:
            // getopt already prints an error message for unknown option characters
            print_usage(argv[0]);
            exit(1);
        } // --switch
    } // -- while

#define CHECK_SET(_arg, _what)                                          \
    if (_arg == 0) {                                                    \
        fprintf(stderr, "\nSYNTAX ERROR: %s must be specified\n", _what);      \
        print_usage(argv[0]);                                                  \
        exit(1);                                                        \
    }
    
    CHECK_SET(arg_dest,     "destination tuple");
    CHECK_SET(op_mode,      "-t or -n");
} // end parse_options

/* ----------------------------
 * check_options
 * ---------------------------- */
void check_options() {
    // checks on values
    if (n_copies <= 0) {
        fprintf(stderr, "\nSYNTAX ERROR: (-B option) consecutive retransmissions should be a positive number\n\n");
        exit(2);
    }
    if (sleepVal < 0) {
        fprintf(stderr, "\nSYNTAX ERROR: (-S option) sleeping seconds should be a positive number\n\n");
        exit(2);
    }
    if ((op_mode == 't') && (transmission_time <= 0)) {
        fprintf(stderr, "\nSYNTAX ERROR: (-t option) you should specify a positive time\n\n");
        exit(2);
    }
    if ((op_mode == 'd') && (data_qty <= 0)) {
        fprintf(stderr, "\nSYNTAX ERROR: (-n option) you should send a positive number of MBytes (%ld)\n\n", data_qty);
        exit(2);
    }
    // checks on options combination
    if ((use_file) && (op_mode == 't')) {
        if (bundle_payload <= ILLEGAL_PAYLOAD) {
            bundle_payload = DEFAULT_PAYLOAD;
            fprintf(stderr, "\nWARNING (a): bundle payload set to %ld bytes\n", bundle_payload);
            fprintf(stderr, "(use_file && op_mode=='t' + payload <= %d)\n\n", ILLEGAL_PAYLOAD);
        }
    }
    if ((use_file) && (op_mode == 'd')) {
        if ((bundle_payload <= ILLEGAL_PAYLOAD) || (bundle_payload > data_qty)) {
            bundle_payload = data_qty;
            fprintf(stderr, "\nWARNING (b): bundle payload set to %ld bytes\n", bundle_payload);
            fprintf(stderr, "(use_file && op_mode=='d' + payload <= %d or > %ld)\n\n", ILLEGAL_PAYLOAD, data_qty);
        }
    }
    if ((!use_file) && (bundle_payload <= ILLEGAL_PAYLOAD) && (op_mode == 'd')) {
        if (data_qty <= MAX_MEM_PAYLOAD) {
            bundle_payload = data_qty;
            fprintf(stderr, "\nWARNING (c1): bundle payload set to %ld bytes\n", bundle_payload);
            fprintf(stderr, "(!use_file + payload <= %d + data_qty <= %d + op_mode=='d')\n\n",
                            ILLEGAL_PAYLOAD, MAX_MEM_PAYLOAD);
        }
        if (data_qty > MAX_MEM_PAYLOAD) {
            bundle_payload = MAX_MEM_PAYLOAD;
            fprintf(stderr, "(!use_file + payload <= %d + data_qty > %d + op_mode=='d')\n",
                            ILLEGAL_PAYLOAD, MAX_MEM_PAYLOAD);
            fprintf(stderr, "\nWARNING (c2): bundle payload set to %ld bytes\n\n", bundle_payload);
        }
    }
    if ((!use_file) && (op_mode == 't')) {
        if (bundle_payload <= ILLEGAL_PAYLOAD) {
            bundle_payload = DEFAULT_PAYLOAD;
            fprintf(stderr, "\nWARNING (d1): bundle payload set to %ld bytes\n\n", bundle_payload);
            fprintf(stderr, "(!use_file + payload <= %d + op_mode=='t')\n\n", ILLEGAL_PAYLOAD);
        }
        if (bundle_payload > MAX_MEM_PAYLOAD) {
            fprintf(stderr, "\nWARNING (d2): bundle payload was set to %ld bytes, now set to %ld bytes\n",
                    bundle_payload, (long)DEFAULT_PAYLOAD);
            bundle_payload = DEFAULT_PAYLOAD;
            fprintf(stderr, "(!use_file + payload > %d)\n\n", MAX_MEM_PAYLOAD);
        }
    }
    if ((csv_out == 1) && ((verbose == 1) || (debug == 1))) {
        fprintf(stderr, "\nSYNTAX ERROR: (-c option) you cannot use -v or -D together with CSV output\n\n");
        exit(2);
    }
    if ((op_mode == 't') && (n_copies != 1)) {
        fprintf(stderr, "\nSYNTAX ERROR: you cannot use -B option in Time-Mode\n\n");
        exit(2);
    }
    if ((op_mode == 't') && (sleepVal != 0)) {
        fprintf(stderr, "\nSYNTAX ERROR: you cannot use -S option in Time-Mode\n\n");
        exit(2);
    }
} // end check_options


/* ----------------------------
 * show_options
 * ---------------------------- */
void show_options() {
    printf("\nRequested");
    if (op_mode == 't')
        printf(" %d second(s) of transmission\n", transmission_time);
    if (op_mode == 'd') {
        printf(" %ld byte(s) to be transmitted %d time(s) every %d second(s)\n", 
                data_qty, n_copies, sleepVal);
    }
    printf(" payload of each bundle = %ld byte(s)", bundle_payload);
    printf("\n\n");
}


/* ----------------------------
 * parse_eid
 * ---------------------------- */
dtn_endpoint_id_t* parse_eid(dtn_handle_t handle, dtn_endpoint_id_t* eid, char * str)
{
    // try the string as an actual dtn tuple
    if (!dtn_parse_eid_string(eid, str)) 
    {
//        if (verbose) fprintf(stdout, "%s (literal)\n", str);
        return eid;
    }
    // build a local tuple based on the configuration of our dtn
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
} // end parse_eid


/* ----------------------------
 * print_eid
 * ---------------------------- */
void print_eid(char *  label, dtn_endpoint_id_t * eid)
{
    printf("%s [%s]\n", label, eid->uri);
} // end print_eid


/* -------------------------------------------------------------------
 * pattern
 *
 * Initialize the buffer with a pattern of (index mod 10).
 * ------------------------------------------------------------------- */
void pattern(char *outBuf, int inBytes) {
    assert (outBuf != NULL);
    while (inBytes-- > 0) {
        outBuf[inBytes] = (inBytes % 10) + '0';
    }
} // end pattern


/* -------------------------------------------------------------------
 * Set timestamp to the given seconds
 * ------------------------------------------------------------------- */
struct timeval set( double sec ) {
    struct timeval mTime;

    mTime.tv_sec  = (long) sec;
    mTime.tv_usec = (long) ((sec - mTime.tv_sec) * 1000000);

    return mTime;
} // end set


/* -------------------------------------------------------------------
 * Add seconds to my timestamp.
 * ------------------------------------------------------------------- */
struct timeval add( double sec ) {
    struct timeval mTime;

    mTime.tv_sec  = (long) sec;
    mTime.tv_usec = (long) ((sec - ((long) sec )) * 1000000);

    // watch for overflow
    if ( mTime.tv_usec >= 1000000 ) {
        mTime.tv_usec -= 1000000;
        mTime.tv_sec++;
    }

    assert( mTime.tv_usec >= 0  && mTime.tv_usec <  1000000 );

    return mTime;
} // end add


/* --------------------------------------------------
 * show_report
 * -------------------------------------------------- */
void show_report (u_int buf_len, char* eid, struct timeval start, struct timeval end, int data) {
    double g_put;

    printf("got %d byte report from [%s]: time=%.1f ms - %d bytes sent",
                buf_len,
                eid,
                ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
                (double)(end.tv_usec - start.tv_usec)/1000.0),
                data);

    // report goodput (bits transmitted / time)
    g_put = (data*8) / ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
                      (double)(end.tv_usec - start.tv_usec)/1000.0);
    printf(" (goodput = %.2f Kbit/s)\n", g_put);

    if (debug) {
        // report start - end time
        printf("[debug] started at %u sec - ended at %u sec\n", (u_int)start.tv_sec, (u_int)end.tv_sec);
    }
} // end show_report


/* --------------------------------------------------
 * csv_time_report
 * -------------------------------------------------- */
void csv_time_report(int b_sent, int payload, struct timeval start, struct timeval end) {
    
    double g_put, data;

    data = b_sent * payload;
    
    g_put = (data*8) / ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
                      (double)(end.tv_usec - start.tv_usec)/1000.0);

    printf("%d,%d,%.1f,%d,%.2f\n", b_sent, 
                                   payload,
                                   ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
                                       (double)(end.tv_usec - start.tv_usec)/1000.0),
                                   (b_sent * payload),
                                   g_put);
} // end csv_time_report


/* --------------------------------------------------
 * csv_data_report
 * -------------------------------------------------- */
void csv_data_report(int b_id, int payload, struct timeval start, struct timeval end) {
    // const char* time_hdr = "BUNDLES_ID,PAYLOAD,TIME,GOODPUT";
    double g_put;
    
    g_put = (payload*8) / ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
                      (double)(end.tv_usec - start.tv_usec)/1000.0);

    printf("%d,%d,%.1f,%.2f\n", b_id,
                                payload,
                                ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
                                       (double)(end.tv_usec - start.tv_usec)/1000.0),
                                g_put);
} // end csv_data_report


/* ----------------------------------------------
 * bundles_needed
 * ---------------------------------------------- */
long bundles_needed (long data, long pl) {
    long res = 0;
    ldiv_t r;

    r = ldiv(data, pl);
    res = r.quot;
    if (r.rem > 0)
        res += 1;

    return res;
} // end bundles_needed


/* ------------------------------------------
 * add_time
 * ------------------------------------------ */
void add_time(struct timeval *tot_time, struct timeval part_time) {
    tot_time->tv_sec += part_time.tv_sec;
    tot_time->tv_usec += part_time.tv_sec;

    if (tot_time->tv_usec >= 1000000) {
        tot_time->tv_sec++;
        tot_time->tv_usec -= 1000000;
    }

} // end add_time

/* ------------------------------------------
 * mega2byte
 *
 * Converts MBytes into Bytes
 * ------------------------------------------ */
long mega2byte(long n) {
    return (n * 1048576);
} // end mega2byte

/* ------------------------------------------
 * kilo2byte
 *
 * Converts KBytes into Bytes
 * ------------------------------------------ */
long kilo2byte(long n) {
    return (n * 1024);
} // end kilo2byte

/* ------------------------------------------
 * findDataUnit
 *
 * Extracts the data unit from the given string.
 * If no unit is specified, returns 'Z'.
 * ------------------------------------------ */
char findDataUnit(const char *inarg) {
    // units are B (Bytes), K (KBytes) and M (MBytes)
    const char unitArray[] = {'B', 'K', 'M'};
    char * unit = malloc(sizeof(char));

    if ((unit = strpbrk(inarg, unitArray)) == NULL) {
        unit = "Z";
    }
    return unit[0];
} // end findUnit
