/* ----------------------
 *      dtnperf-client.c
 * ---------------------- */

/* -----------------------------------------------------------
 *  PLEASE NOTE: this software was developed
 *    by Piero Cornice <piero.cornice@gmail.com>.
 *  If you want to modify it, please contact me
 *  at piero.cornice(at)gmail.com. Thanks =)
 * -----------------------------------------------------------
 */

/*
 * Modified slightly (and renamed) by Michael Demmer
 * <demmer@cs.berkeley.edu> to fit in with the DTN2
 * source distribution.
 */

/* version 1.2.1 - 14/07/05
 *
 * - cleared and optimized code
 * - deleted old (useless) command-line options
 * - possibility to set debug mode from command-line (-D)
 * - possibility to set payload dimension from command-line (-p)
 * - supports multiple sending (-B, -S)
 * - CSV output (-c)
 * - auto-generated source tuple
 * - enabled transmission > 50000 bytes (using multiple bundles)
 * - added file transfer (-f) in Data-Mode
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/file.h>
#include <time.h>
#include <assert.h>

#include "dtn_api.h"
#include "dtn_types.h"

#define MAXBUFFERSIZE 104857600 // maximum size of buffer (default: 1 MB)
#define BUNDLE_PAYLOAD 50000    // quantity of data (in bytes) to send in Time-Mode (max: 50000 byte)

char *progname;

// global options
dtn_bundle_payload_location_t 
        payload_type    = DTN_PAYLOAD_MEM;    // the type of data source for the bundle (default MEM)
int verbose             = 0;    // if set to 1, execution becomes verbose (-v option)
char op_mode               ;    // operative mode (t = time_mode, d = data_mode)
int debug               = 0;    // if set to 1, many debug messages are shown
int csv_out             = 0;    // if set to 1, a Comma-Separated-Values output is shown
/* -----------------------------------------------------------------------
 * NOTE - CSV output shows the following columns:
 *  Time-Mode: BUNDLES_SENT, PAYLOAD, TIME, DATA_SENT, GOODPUT
 *  Data-Mode: BUNDLE_ID, PAYLOAD, TIME, GOODPUT
 * ----------------------------------------------------------------------- */

// bundle options
int return_receipts     = 1;    // request end to end return receipts (default 1)
int forwarding_receipts = 0;    // request per hop departure
int custody             = 0;    // request custody transfer
int custody_receipts    = 0;    // request per custodian receipts
int receive_receipts    = 0;    // request per hop arrival receipt
int overwrite           = 0;    // queue overwrite option
int wait_for_report     = 1;    // wait for bundle status reports (default 1)

// specified options for bundle tuples
char * arg_replyto      = NULL; // replyto_tuple
char * arg_source       = NULL; // source_tuple
char * arg_dest         = NULL; // destination_tuple

dtn_reg_id_t regid      = DTN_REGID_NONE;   // registration ID (-i option)
int bundle_payload      = BUNDLE_PAYLOAD;   // quantity of data (in bytes) to send (-p option)

// Time-Mode options
int transmission_time   = 0;    // seconds of transmission

// Data-Mode options
int data_qty            = 0;    // data to be transmitted (in bytes)
int n_copies            = 1;    // number of trasmissions (default 1)
int sleepVal            = 0;    // seconds to sleep between transmissions in Data-Mode (default 0)
int use_file            = 0;    // if set to 1, a file is used instead of memory (default 0)
//char data_unit        = 'k';  // k = kilobytes (default), M = megabytes -- NOT IMPLEMENTED YET

// Data-Mode variables
int fd                     ;    // file descriptor, used with -f option
int data_written        = 0;    // data written on the file
int data_read           = 0;    // data read from the file
char * file_name        = "/dtnbuffer.snd";    // name of the file to be used

/* -------------------------------
 *       function interfaces
 * ------------------------------- */
void parse_options(int, char**);
dtn_tuple_t* parse_tuple(dtn_handle_t handle, dtn_tuple_t * tuple, char * str);
void print_usage();
void print_tuple(char * label, dtn_tuple_t * tuple);
void pattern(char *outBuf, int inBytes);
struct timeval set(double sec);
void show_report (u_int buf_len, char* region, int admin_len, char* admin_val, struct timeval start, struct timeval end, int data);
void csv_time_report(int b_sent, int payload, struct timeval start, struct timeval end);
void csv_data_report(int b_id, int payload, struct timeval start, struct timeval end);
int bundles_needed (int data, int pl);
void check_options();
void show_options();

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

    int i, j, k;                    // loop-control variables
    const char* time_report_hdr = "BUNDLE_SENT,PAYLOAD,TIME,DATA_SENT,GOODPUT";
    const char* data_report_hdr = "BUNDLE_ID,PAYLOAD,TIME,GOODPUT";
    int n_bundles = 0;              // number of bundles needed (Data-Mode)
    
    // DTN variables
    dtn_handle_t        handle;
    dtn_tuple_t         source_tuple;
    dtn_reg_info_t      reginfo;
    dtn_bundle_spec_t   bundle_spec;
    dtn_bundle_spec_t   reply_spec;
    dtn_bundle_payload_t send_payload;
    dtn_bundle_payload_t reply_payload;
    char demux[64];


    // buffer specifications
    char* buffer = NULL;            // buffer containing data to be transmitted for testing the DTN
    int bufferLen;                  // lenght of buffer

    int bundles_sent;               // number of bundles sent in Time-Mode
    
    /* -------
     *  begin
     * ------- */

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
    handle = dtn_open();
    if (handle == 0) 
    {
        fprintf(stderr, "fatal error opening dtn handle: %s\n", strerror(errno));
        exit(1);
    }
    if (debug) printf(" done\n");

    // initialize bundle spec
    memset(&bundle_spec, 0, sizeof(bundle_spec));
    if (debug) printf("[debug] memset for bundle_spec done\n");

    // initialize and parse bundle src/dest/replyto tuples
    snprintf(demux, sizeof(demux), "/src");

    if (debug) printf("[debug] checking if arg_source != NULL...\n");
    if (arg_source != NULL) {                                   // if user specified a source...
        if (debug) printf("[debug]\t\tsource specified (%s), now parsing...", arg_source);
        if (dtn_parse_tuple_string(&source_tuple, arg_source)) { // ...use that as the source_tuple...
            fprintf(stderr, "invalid source tuple string '%s'\n", arg_source);
            exit(1);
        }
        if (debug) printf(" done\n");
    } else {                                                     // ...otherwise...
        if (debug) printf("[debug]\t\tsource not specified, using local tuple... ");
        dtn_build_local_tuple(handle, &source_tuple, demux);     // ...use local tuple with demux string
        if (debug) printf(" done\n");
    }
    if (debug) printf("[debug] ...arg_source checked\n");

    if (debug) printf("[debug] Source tuple: bundles://%s/%.*s \n",
                      source_tuple.region,
                      (int) source_tuple.admin.admin_len, 
                      source_tuple.admin.admin_val);

    // set the source tuple in the bundle spec
    if (debug) printf("[debug] copying source tuple into bundle_spec...");
    dtn_copy_tuple(&bundle_spec.source, &source_tuple);
    if (debug) printf(" done\n");    

    if (verbose) fprintf(stdout, "Destination specified: %s\n", arg_dest);

    if (debug) printf("[debug] parse_tuple for destination...");
    parse_tuple(handle, &bundle_spec.dest, arg_dest);
    if (debug) printf(" done\n");

    if (arg_replyto == NULL) 
    {
        if (verbose) fprintf(stdout, "Reply-To specified: none, using Source\n");
        if (debug) printf("[debug] copying source_tuple to replyto_tuple...");
        dtn_copy_tuple(&bundle_spec.replyto, &bundle_spec.source);
        if (debug) printf(" done\n");
    }
    else
    {
        if (verbose) fprintf(stdout, "Reply-To specified: %s\n", arg_replyto);
        if (debug) printf("[debug] parsing replyto tuple...");
        parse_tuple(handle, &bundle_spec.replyto, arg_replyto);
        if (debug) printf(" done\n");
    }

    /* ------------------------
     * set the dtn options
     * ------------------------ */
    if (debug) printf("[debug] setting the DTN options: ");

    // default expiration time (one hour)
    bundle_spec.expiration = 3600;

    // return_receipts
    if (return_receipts) {
        bundle_spec.dopts |= DOPTS_RETURN_RCPT;
        if (debug) printf("RETURN_RCPT ");
    }
    // forwarding receipts
    if (forwarding_receipts) {
        bundle_spec.dopts |= DOPTS_FWD_RCPT;
        if (debug) printf("FDW_RCPT ");
    }
    // custody
    if (custody) {
        bundle_spec.dopts |= DOPTS_CUSTODY;
        if (debug) printf("CUSTODY ");
    }
    // custody receipts
    if (custody_receipts) {
        bundle_spec.dopts |= DOPTS_CUST_RCPT;
        if (debug) printf("CUST_RCPT ");
    }
    // overwrite
    if (overwrite) {
        bundle_spec.dopts |= DOPTS_OVERWRITE;
        if (debug) printf("OVERWRITE ");
    }
    // receive receipts
    if (receive_receipts) {
        bundle_spec.dopts |= DOPTS_RECV_RCPT;
        if (debug) printf("RECV_RCPT ");
    }
    if (debug) printf("option(s) set\n");

    // now create a new registration based on the source
    if (debug) printf("[debug] memset for reginfo...");
    memset(&reginfo, 0, sizeof(reginfo));
    if (debug) printf(" done\n");

    if (debug) printf("[debug] copying bundle_spec.replyto to reginfo.endpoint...");
    dtn_copy_tuple(&reginfo.endpoint, &bundle_spec.replyto);
    if (debug) printf(" done\n");

    if (debug) printf("[debug] setting up reginfo...");
    reginfo.action = DTN_REG_ABORT;
    reginfo.regid = regid;
    reginfo.timeout = 60 * 60;
    if (debug) printf(" done\n");

    if (debug) printf("[debug] registering to local daemon...");
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }    
    if (debug) printf(" done: regid 0x%x\n", regid);

    // bind the current handle to the new registration
    if (debug) printf("[debug] binding handle to registration...");
    dtn_bind(handle, regid, &bundle_spec.replyto);
//    dtn_bind(handle, regid, &source_tuple);
    if (debug) printf(" done\n");

    /* ------------------------------------------------------------------------------
     * select the operative-mode (between Time_Mode and Data_Mode)
     * ------------------------------------------------------------------------------ */

    if (op_mode == 't') {
    /* ---------------------------------------
     * Time_Mode
     * --------------------------------------- */
        if (verbose) printf("Working in Time_Mode\n");
        if (verbose) printf("requested %d second(s) of transmission\n", transmission_time);

        // reset data_qty
        if (debug) printf("[debug] reset data_qty and bundles_sent...");
        data_qty = 0;
        bundles_sent = 0;
        if (debug) printf(" done\n");

        // allocate buffer space
        if (debug) printf("[debug] malloc for the buffer...");
        buffer = malloc(MAXBUFFERSIZE);
        if (debug) printf(" done\n");
        
        // initialize buffer
        if (debug) printf("[debug] initialize the buffer with a pattern...");
        pattern(buffer, bundle_payload);
        if (debug) printf(" done\n");
        bufferLen = strlen(buffer);
        if (debug) printf("[debug] bufferLen = %d\n", bufferLen);

        // memset for payload
        if (debug) printf("[debug] memset for payload...");
        memset(&send_payload, 0, sizeof(send_payload));
        if (debug) printf(" done\n");

        // fill in the payload
        if (debug) printf("[debug] filling payload...");
        dtn_set_payload(&send_payload, payload_type, buffer, bufferLen);
        if (debug) printf(" done\n");

        // print CSV header (is csv_out == 1)
        if (csv_out)
            printf("%s\n", time_report_hdr);

        // for (n_copies)
        for (k=0; k<n_copies; k++) {

            // reset data counters
            bundles_sent = 0;
            data_qty = 0;

            // initialize timer
            if (debug) printf("[debug] initializing timer...");
            gettimeofday(&start, NULL);
            if (debug) printf(" start.tv_sec = %u sec\n", (u_int)start.tv_sec);
    
//        if (debug) printf("[debug] requested transmission_time = %u sec\n", transmission_time);

            // calculate end-time
            if (debug) printf("[debug] calculating end-time...");
            end = set (0);
            end.tv_sec = start.tv_sec + transmission_time;
            if (debug) printf(" end.tv_sec = %u sec\n", (u_int)end.tv_sec);
    
            // loop
            if (debug) printf("[debug] entering loop...\n");
            for (now.tv_sec = start.tv_sec; now.tv_sec <= end.tv_sec; gettimeofday(&now, NULL)) {
    
                if (debug) printf("\t[debug] now.tv_sec = %u sec of %d\n", (u_int)now.tv_sec, (u_int)end.tv_sec);
    
                // send the bundle
                if (debug) printf("\t[debug] sending the bundle...");
                if ((ret = dtn_send(handle, &bundle_spec, &send_payload)) != 0) {
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
                if (debug) printf("\t[debug] now data_qty is %d\n", data_qty);
    
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
    
            // get the TOTAL end time
            if (debug) printf("[debug] getting total end-time...");
	        gettimeofday(&end, NULL);
            if (debug) printf(" end.tv_sec = %u sec\n", (u_int)end.tv_sec);
	   
        	// show the report
            if (csv_out == 0) {
                printf("%d bundles sent, each with a %d bytes payload\n", bundles_sent, bundle_payload);
                show_report(reply_payload.dtn_bundle_payload_t_u.buf.buf_len,
                            reply_spec.source.region,
                            (int) reply_spec.source.admin.admin_len,
                            reply_spec.source.admin.admin_val,
                            start,
                            end,
                            data_qty);
            }
            if (csv_out == 1) {
                csv_time_report(bundles_sent, bundle_payload, start, end);
            }

            if (n_copies > 0)
                sleep(sleepVal);

        } // for (n_copies)

        // deallocate buffer memory
        if (debug) printf("[debug] deallocating buffer memory...");
        free((void*)buffer);
        if (debug) printf(" done\n");

    } // -- time_mode

    else if (op_mode == 'd') {
    /* ---------------------------------------
     * Data_Mode
     * --------------------------------------- */
        if (verbose) printf("Working in Data_Mode\n");

        // initialize the buffer
        if (debug) printf("[debug] initializing buffer...");
        buffer = malloc( (data_qty < bundle_payload) ? data_qty : bundle_payload );
        memset(buffer, 0, (data_qty < bundle_payload) ? data_qty : bundle_payload );
        pattern(buffer, (data_qty < bundle_payload) ? data_qty : bundle_payload );
        bufferLen = strlen(buffer);
        if (debug) printf(" done. bufferLen = %d (should equal %s)\n",
                            bufferLen,
                            (data_qty < bundle_payload) ? "data_qty" : "bundle_payload");

        if (use_file) {

            // set bundle_payload size
            if (debug) printf("[debug] setting bundle_payload...");
            bundle_payload = data_qty;
            if (debug) printf(" bundle_payload set to %d bytes\n", bundle_payload);

            // create the file
            if (debug) printf("[debug] creating file...");
            fd = open(file_name, O_CREAT|O_TRUNC|O_WRONLY|O_APPEND, 0666);
            if (fd < 0) {
                fprintf(stderr, "ERROR: couldn't create file [fd = %d]. \
                                 Maybe you don't have writing permission\n", fd);
                exit(2);
            }
            if (debug) printf(" done\n");

            // fill in the file with a pattern
            if (debug) printf("[debug] filling the file with the pattern...");
            data_written += write(fd, buffer, bufferLen);
            if (debug) printf(" done. Written %d bytes\n", data_written);

            // close the file
            if (debug) printf("[debug] closing file...");
            close(fd);
            if (debug) printf(" done\n");
        }

        // fill in the payload
        if (debug) printf("[debug] filling the bundle payload with buffer...");
        memset(&send_payload, 0, sizeof(send_payload));
        if (use_file) {
            dtn_set_payload(&send_payload, payload_type, file_name, strlen(file_name));
        } else {
            dtn_set_payload(&send_payload, payload_type, buffer, bufferLen);
        }
        if (debug) printf(" done\n");

        // if CSV option is set, print the data_report_hdr
        if (csv_out == 1)
            printf("%s\n", data_report_hdr);

        if (!use_file) {
            // calculate how many bundles are needed
            if (debug) printf("[debug] calculating how many bundles are needed...");
            n_bundles = bundles_needed(data_qty, bundle_payload);
            if (debug) printf(" n_bundles = %d\n", n_bundles);
        }

        // initialize TOTAL start timer
        if (debug) printf("[debug] initializing TOTAL start timer...");
        gettimeofday(&start, NULL);
        if (debug) printf(" start.tv_sec = %u sec\n", (u_int)start.tv_sec);

        if (debug) printf("[debug] entering n_copies loop...\n");
        // --------------- loop until all n_copies are sent
        for (i=0; i<n_copies; i++) {
    
            if (!use_file) {

                if (debug) printf("\t[debug] entering n_bundles loop...\n");
                for (j=0; j<n_bundles; j++) {
    
                    // initialize PARTIAL start timer
                    if (debug) printf("\t\t[debug] initializing PARTIAL start timer...");
                    gettimeofday(&p_start, NULL);
                    if (debug) printf(" p_start.tv_sec = %u sec\n", (u_int)p_start.tv_sec);
    
                    // send the bundle
                    if (debug) printf("\t\t[debug] sending copy %d...", i+1);
                    if ((ret = dtn_send(handle, &bundle_spec, &send_payload)) != 0) {
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
                        show_report(reply_payload.dtn_bundle_payload_t_u.buf.buf_len,
                                    reply_spec.source.region,
                                    (int) reply_spec.source.admin.admin_len,
                                    reply_spec.source.admin.admin_val,
                                    p_start,
                                    p_end,
                                    ((bundle_payload <= data_qty)?bundle_payload:data_qty));
                    }
                } // end for(n_bundles)
                if (debug) printf("\t[debug] ...out from n_bundles loop\n");
            } // end if(!use_file)

            else if (use_file) {
                // send the bundle
                if (debug) printf("[debug] sending the bundle...");
                if ((ret = dtn_send(handle, &bundle_spec, &send_payload)) != 0) {
                    fprintf(stderr, "error sending file bundle: %d (%s)\n",
                            ret, dtn_strerror(dtn_errno(handle)));
                    exit(1);
                }
                if (debug) printf(" done\n");

                // set memory for the answer
                if (debug) printf("[debug] setting memory for reply...");
                memset(&reply_spec, 0, sizeof(reply_spec));
                memset(&reply_payload, 0, sizeof(reply_payload));
                if (debug) printf(" done\n");
            
                // now we block waiting for the echo reply
                if (debug) printf("[debug] waiting fot the reply...");
                if ((ret = dtn_recv(handle, &reply_spec, DTN_PAYLOAD_MEM, &reply_payload, -1)) < 0)
                {
                    fprintf(stderr, "error getting reply: %d (%s)\n",
                            ret, dtn_strerror(dtn_errno(handle)));
                    exit(1);
                }
                if (debug) printf(" reply received\n");
            } // end if(use_file)

            else {      // this should not be executed (written only for debug purpouse)
                fprintf(stderr, "\n ERROR: this should not be executed!!!\n");
                exit(3);
            }

            // calculate TOTAL end time
            if (debug) printf("\t[debug] calculating TOTAL end time...");
            gettimeofday(&end, NULL);
            if (debug) printf(" end.tv_sec = %u sec\n", (u_int)end.tv_sec);

            // show the TOTAL report
            if (csv_out == 0) {
                show_report(reply_payload.dtn_bundle_payload_t_u.buf.buf_len,
                            reply_spec.source.region,
                            (int) reply_spec.source.admin.admin_len,
                            reply_spec.source.admin.admin_val,
                            start,
                            end,
                            data_qty);
            }
            if (csv_out == 1) {    
                csv_data_report(i+1, data_qty, p_start, p_end);
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
    if ( dtn_close(handle) != -1)
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
void print_usage()
{
    fprintf(stderr, "\nSYNTAX: %s "
            "-s <source_tuple> -d <dest_tuple> "
            "-t <sec> | -n <num> [options]\n\n", progname);
    fprintf(stderr, "options:\n");
    fprintf(stderr, " -s <tuple> source tuple\n");
    fprintf(stderr, " -d <tuple> destination tuple (required)\n");
    fprintf(stderr, " -r <tuple> reply to tuple\n");
    fprintf(stderr, " -t <sec> Time-Mode: seconds of transmission\n");
    fprintf(stderr, " -n <num> Data-Mode: number of bytes to send\n");
    fprintf(stderr, "common options:\n");
    fprintf(stderr, " -p <size> size in bytes of bundle payload (default 50000, max 50000)\n");
    fprintf(stderr, " -v verbose\n");
    fprintf(stderr, " -i registration id for reply to\n");
    fprintf(stderr, " -B <num> number of consecutive bundle transmissions (default 1)\n");
    fprintf(stderr, " -S <sec> sleeping seconds between consecutive bundle transmissions (default 1)\n");
    fprintf(stderr, "Data-Mode options:\n");
    fprintf(stderr, " -f transfer a file\n");
    fprintf(stderr, "other options:\n");
    fprintf(stderr, " -c CSV output (useful with redirection of the output to a file)\n");
    fprintf(stderr, " -h help: show this message\n");
    fprintf(stderr, " -D debug messages (many)\n");
    fprintf(stderr, "\n");
    exit(1);
} // end print_usage


/* ----------------------------
 * parse_options
 * ---------------------------- */
void parse_options(int argc, char**argv)
{
    char c, done = 0;

    progname = argv[0];

    while (!done)
    {
        c = getopt(argc, argv, "hvDcfr:d:s:i:t:p:n:S:B:");
        switch (c)
        {
        case 'v':
            verbose = 1;
            break;
        case 'h':
            print_usage();
            exit(0);
            return;
        case 'c':
            csv_out = 1;
            break;
        case 'r':
            arg_replyto = optarg;
            break;
        case 's':
            arg_source = optarg;
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
            payload_type = DTN_PAYLOAD_MEM;
            break;
    	case 'p':
	        bundle_payload = atoi(optarg);
            break;
        case 'n':
            op_mode = 'd';
            data_qty = atoi(optarg);
            // data_unit = ??? -- NOT IMPLEMENTED YET, using bytes
            break;
        case 'B':
            n_copies = atoi(optarg);
            break;
        case 'S':
            sleepVal = atoi(optarg);
            break;
        case 'f':
            use_file = 1;
            payload_type = DTN_PAYLOAD_FILE;
            break;
        case -1:
            done = 1;
            break;
        default:
            // getopt already prints an error message for unknown option characters
            print_usage();
            exit(1);
        } // --switch
    } // -- while

#define CHECK_SET(_arg, _what)                                          \
    if (_arg == 0) {                                                    \
        fprintf(stderr, "\nSYNTAX ERROR: %s must be specified\n", _what);      \
        print_usage();                                                  \
        exit(1);                                                        \
    }
    
//    CHECK_SET(arg_source,   "source tuple");
    CHECK_SET(arg_dest,     "destination tuple");
    CHECK_SET(op_mode,      "-t or -n");
} // end parse_options

/* ----------------------------
 * check_options
 * ---------------------------- */
void check_options() {
/*
    if ((op_mode == 't') && (sleepVal != 0)) {
        fprintf(stderr, "\nSYNTAX ERROR: you cannot use -S option in Time-Mode\n\n");
        exit(2);
    }
*/
/*
    if ((op_mode == 't') && (n_copies != 1)) {
        fprintf(stderr, "\nSYNTAX ERROR: you cannot use -B option in Time-Mode\n\n");
        exit(2);
    }
*/
    if ((op_mode == 'd') && (data_qty <= 0)) {
        fprintf(stderr, "\nSYNTAX ERROR: (-n option) you should send a positive number of bytes\n\n");
        exit(2);
    }
    if ((op_mode == 't') && (transmission_time <= 0)) {
        fprintf(stderr, "\nSYNTAX ERROR: (-t option) you should specify a positive time\n\n");
        exit(2);
    }
    if (bundle_payload <= 0) {
        fprintf(stderr, "\nSYNTAX ERROR: (-p option) bundle payload should be a positive number\n\n");
        exit(2);
    }
/*
    if (bundle_payload > 50000) {
        fprintf(stderr, "\nSYNTAX ERROR: (-p option) bundle payload maximum size is 50000\n\n");
        exit(2);
    }
*/
    if (n_copies <= 0) {
        fprintf(stderr, "\nSYNTAX ERROR: (-B option) consecutive retransmissions should be a positive number\n\n");
        exit(2);
    }
    if (sleepVal < 0) {
        fprintf(stderr, "\nSYNTAX ERROR: (-S option) sleeping seconds should be a positive number\n\n");
        exit(2);
    }
    if ((csv_out == 1) && ((verbose == 1) || (debug == 1))) {
        fprintf(stderr, "\nSYNTAX ERROR: (-c option) you cannot use -v or -D together with CSV output\n\n");
        exit(2);
    }
    if ((use_file) && (bundle_payload != 50000)) {
        fprintf(stderr, "\nWARNING: when using file transfer, -p option will be ignored\n\n");
    }
} // end check_options


/* ----------------------------
 * show_options
 * ---------------------------- */
void show_options() {
    printf("\nRequested ");
    if (op_mode == 't')
        printf("%u second(s) of transmission ", transmission_time);
    if (op_mode == 'd')
        printf("%d byte(s) to be transmitted ", data_qty);
    printf("%d time(s) every %u second(s)", n_copies, sleepVal);
    if (!use_file)
        printf("\npayload of each bundle = %d byte(s)", bundle_payload);
    printf("\n\n");
}


/* ----------------------------
 * parse_tuple
 * ---------------------------- */
dtn_tuple_t* parse_tuple(dtn_handle_t handle, dtn_tuple_t* tuple, char * str)
{
    // try the string as an actual dtn tuple
    if (!dtn_parse_tuple_string(tuple, str)) 
    {
//        if (verbose) fprintf(stdout, "%s (literal)\n", str);
        return tuple;
    }
    // build a local tuple based on the configuration of our dtn
    // router plus the str as demux string
    else if (!dtn_build_local_tuple(handle, tuple, str))
    {
        if (verbose) fprintf(stdout, "%s (local)\n", str);
        return tuple;
    }
    else
    {
        fprintf(stderr, "invalid tuple string '%s'\n", str);
        exit(1);
    }
} // end parse_tuple


/* ----------------------------
 * print_tuple
 * ---------------------------- */
void print_tuple(char *  label, dtn_tuple_t * tuple)
{
    printf("%s [%s %.*s]\n", label, tuple->region, 
           (int) tuple->admin.admin_len, tuple->admin.admin_val);
} // end print_tuple


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


/* --------------------------------------------------
 * show_report
 * -------------------------------------------------- */
void show_report (u_int buf_len, char* region, int admin_len, char* admin_val, struct timeval start, struct timeval end, int data) {
    double g_put;

    printf("got %d byte report from [%s %.*s]: time=%.1f ms - %d bytes sent",
                buf_len,
                region,
                admin_len,
                admin_val,
                ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
                (double)(end.tv_usec - start.tv_usec)/1000.0),
                data);

    // report goodput (bits transmitted / time)
    g_put = (data*8) / ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
                      (double)(end.tv_usec - start.tv_usec)/1000.0);
    printf(" (goodput = %.2f)\n", g_put);

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
int bundles_needed (int data, int pl) {
    int res = 0;
    div_t r;

    r = div(data, pl);
    res = r.quot;
    if (r.rem > 0)
        res += 1;

    return res;
} // end bundles_needed
