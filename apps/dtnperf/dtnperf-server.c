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
 *    dtnperf-server.c
 * ---------------------- */

/* -----------------------------------------------------------
 *  PLEASE NOTE: this software was developed
 *    by Piero Cornice <piero.cornice@gmail.com>
 *    at DEIS - University of Bologna, Italy.
 *  If you want to modify it, please contact me
 *  at piero.cornice(at)gmail.com. Thanks =)
 * -----------------------------------------------------------
 */

/* version 1.6.0 - 23/06/06
 *
 * - compatible with DTN 2.2.0 reference implementation
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "dtn_api.h"
#include <sys/stat.h>

#define BUFSIZE 16
#define BUNDLE_DIR_DEFAULT INSTALL_LOCALSTATEDIR "/dtn/dtnperf"
#define OUTFILE "dtnbuffer.rcv"

/* ------------------------------
 *  Global variables and options
 * ------------------------------ */

// values between [square braces] are default
const char *progname   ;
int use_file        = 1;        // if set to 0, memorize received bundles into memory (max 50000 bytes)
                                // if set to 1, memorize received bundles into a file (-f) [0]
int verbose         = 0;        // if set to 1, show verbose messages [0]
int aggregate       = 0;        // if > 0, print after aggregate arrivals
int debug           = 0;        // if set to 1, show debug messages [1]
char * endpoint     = "/dtnperf:/dest";     // endpoint (-e) ["/dtnperf:/dest"]
char * bundle_dir   = BUNDLE_DIR_DEFAULT;   // destination directory (-d)

/* ------------------------
 *  Function Prototypes
 * ------------------------ */
void print_usage(char*);
void parse_options(int, char**);
dtn_endpoint_id_t* parse_eid(dtn_handle_t, dtn_endpoint_id_t*, char*);


/* -------------------------------------------------
 * main
 * ------------------------------------------------- */
int main(int argc, char** argv)
{

    /* -----------------------
     *  variables declaration
     * ----------------------- */
    int k;
    int ret;
    dtn_handle_t        handle;
    dtn_endpoint_id_t   local_eid;
    dtn_reg_info_t      reginfo;
    dtn_reg_id_t        regid;
    dtn_bundle_spec_t   spec;
    dtn_bundle_payload_t payload;
    char *buffer;                   // buffer used for shell commands
    char s_buffer[BUFSIZE];
    time_t current;
    dtn_bundle_payload_location_t pl_type = DTN_PAYLOAD_FILE; // payload saved into memory or into file
//    char * source_region, dest_region;
    int source_eid_len, dest_eid_len;
    char *source_eid, *dest_eid;
    char * filepath;
    char * filename = OUTFILE;      // source filename [OUTFILE]
    int bufsize;
    int count = 0;
    int total = 0;

    /* -------
     *  begin
     * ------- */

    // print information header
    printf("\nDTNperf - SERVER - v 1.6.0");
    printf("\nwritten by Piero Cornice <piero.cornice@gmail.com>");
    printf("\nDEIS - University of Bologna, Italy");
    printf("\n");

    // parse command-line options
    parse_options(argc, argv);
    if (debug) printf("[debug] parsed command-line options\n");

    // show requested options (verbose)
    if (verbose) {
        printf("\nOptions:\n");
        printf("\tendpoint       : %s\n", endpoint);
        printf("\tsave bundles to: %s\n", use_file ? "file" : "memory");
        printf("\tdestination dir: %s\n", bundle_dir);
        printf("\n");
    }

    // initialize buffer with shell command ("mkdir -p " + bundle_dir)
    if (debug) printf("[debug] initializing buffer with shell command...");
    buffer = malloc(sizeof(char) * (strlen(bundle_dir) + 10));
    sprintf(buffer, "mkdir -p %s", bundle_dir);
    if (debug) printf(" done. Shell command = %s\n", buffer);

    // execute shell command
    if (debug) printf("[debug] executing shell command \"%s\"...", buffer);
    if (system(buffer) == -1)
    {
        fprintf(stderr, "Error opening bundle directory: %s\n", bundle_dir);
        exit(1);
    }
    free(buffer);
    if (debug) printf(" done\n");

    // open the ipc handle
    if (debug) printf("[debug] opening connection to dtn router...");
    int err = dtn_open(&handle);
    if (err != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                dtn_strerror(err));
        exit(1);
    }
    if (debug) printf(" done\n");

    // build a local tuple based on the configuration of local dtn router
    if (debug) printf("[debug] building local eid...");
    dtn_build_local_eid(handle, &local_eid, endpoint);
    if (debug) printf(" done\n");
    if (verbose) printf("local_eid = %s\n", local_eid.uri);

    // create a new registration based on this eid
    if (debug) printf("[debug] registering to local daemon...");
    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_eid(&reginfo.endpoint, &local_eid);
    reginfo.failure_action = DTN_REG_DEFER;
    reginfo.regid = DTN_REGID_NONE;
    reginfo.expiration = 0;
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }
    if (debug) printf(" done\n");
    if (verbose) printf("regid 0x%x\n", regid);

    // set bundle destination type
    if (debug) printf("[debug] choosing bundle destination type...");
    if (use_file)
        pl_type = DTN_PAYLOAD_FILE;
    else
        pl_type = DTN_PAYLOAD_MEM;
    if (debug) printf(" done. Bundles will be saved into %s\n", use_file ? "file" : "memory");

    if (debug) printf("[debug] entering infinite loop...\n");

    // infinite loop, waiting for bundles
    while (1) {

        // wait until receive a bundle
        memset(&spec, 0, sizeof(spec));
        memset(&payload, 0, sizeof(payload));

        if (debug) printf("[debug] waiting for bundles...");
        if ((ret = dtn_recv(handle, &spec, pl_type, &payload, -1)) < 0)
        {
            fprintf(stderr, "error getting recv reply: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }
        if (debug) printf(" bundle received\n");
        count++;

        size_t len;
        if (pl_type == DTN_PAYLOAD_MEM) {
            len = payload.buf.buf_len;
        } else {
            struct stat st;
            memset(&st, 0, sizeof(st));
            stat(payload.filename.filename_val, &st);
            len = st.st_size;
        }
        
        total += len;

        // mark current time
        if (debug) printf("[debug] marking time...");
        current = time(NULL);
        if (debug) printf(" done\n");

        if (aggregate == 0) {
            printf("%s : %zu bytes from %s\n",
                   ctime(&current),
                   len,
                   spec.source.uri);
        } else if (count % aggregate == 0) {
            printf("%s : %d bundles, total length %d bytes\n",
                   ctime(&current), count, total);
        }

        /* ---------------------------------------------------
         *  parse admin string to select file target location
         * --------------------------------------------------- */   

        // copy SOURCE eid
        if (debug) printf("[debug]\tcopying source eid...");
        source_eid_len = sizeof(spec.source.uri);
        source_eid = malloc(sizeof(char) * source_eid_len+1);
        memcpy(source_eid, spec.source.uri, source_eid_len);
        source_eid[source_eid_len] = '\0';
        if (debug) {
            printf(" done:\n");
            printf("\tsource_eid = %s\n", source_eid);
            printf("\n");
        }

        // copy DEST eid
        if (debug) printf("[debug]\tcopying dest eid...");
        dest_eid_len = sizeof(spec.dest.uri);
        dest_eid = malloc(sizeof(char) * dest_eid_len+1);
        memcpy(dest_eid, spec.dest.uri, dest_eid_len);
        dest_eid[dest_eid_len] = '\0';
        if (debug) {
            printf(" done:\n");
            printf("\tdest_eid = %s\n", dest_eid);
            printf("\n");
        }

        // recursively create full directory path
        filepath = malloc(sizeof(char) * dest_eid_len + 10);
        sprintf(filepath, "mkdir -p %s", bundle_dir);
        if (debug) printf("[debug] filepath = %s\n", filepath);
        system(filepath);

        // create file name
        sprintf(filepath, "%s/%s", bundle_dir, filename);
        if (debug) printf("[debug] filepath = %s\n", filepath);

        // bundle name is the name of the bundle payload file
        buffer = payload.buf.buf_val;
        bufsize = payload.buf.buf_len;

        if (debug) {
            printf ("======================================\n");
            printf (" Bundle received at %s\n", ctime(&current));
            printf ("  source: %s\n", spec.source.uri);
            if (use_file) {
                printf ("  saved into    : %s\n", filepath);
            }

            printf ("--------------------------------------\n");
        }

        if (pl_type == DTN_PAYLOAD_FILE) { // if bundle was saved into file

            if (debug) printf("[debug] renaming file %s -> %s...",
                              payload.filename.filename_val, filepath);
            if (rename(payload.filename.filename_val, filepath) != 0) {
                printf("[ERROR] Couldn't rename %s -> %s: %s\n",
                       payload.filename.filename_val, filepath, strerror(errno));
            }

        } else { // if bundle was saved into memory

            if (debug) {
                for (k=0; k < (int)payload.buf.buf_len; k++)
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
                } // for

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
                           (int)payload.buf.buf_len%BUFSIZE, s_buffer);
                } // if

                printf ("======================================\n");
 
            } // if (debug) 

            free(filepath);
            free(source_eid);
            free(dest_eid);

        }

        fflush(stdout);

    } // while(1)

    // if this was ever changed to gracefully shutdown, it would be good to call:
    dtn_close(handle);

    return 0;

} //main

/* -------------------------------
 *        Utility Functions
 * ------------------------------- */

/* -------------
 *  print_usage
 * ------------- */
void print_usage(char* progname)
{
    fprintf(stderr, "\n");
    fprintf(stderr, "SYNTAX: %s [options]\n", progname);
    fprintf(stderr, "\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, " -d <dir>: destination directory (if using -f) \n");
    fprintf(stderr, "           [default destination dir: %s]\n", BUNDLE_DIR_DEFAULT);
    fprintf(stderr, " -D: many debug messages are shown\n");
    fprintf(stderr, " -m: save received bundles into memory\n");
    fprintf(stderr, " -h: shows this help\n");
    fprintf(stderr, " -v: verbose\n");
    fprintf(stderr, " -a <n>: print message every n arrivals\n");
    fprintf(stderr, "\n");
    exit(1);
}

/* ---------------
 *  parse_options
 * --------------- */
void parse_options (int argc, char** argv) {
    char c, done = 0;

    while (!done) {
        c = getopt(argc, argv, "hvDfmd:e:a:");

        switch(c) {
            case 'h':           // show help
                print_usage(argv[0]);
                exit(1);
                return;
            case 'v':           // verbose mode
                verbose = 1;
                break;
            case 'D':           // debug messages
                debug = 1;
                break;
            case 'm':           // save received bundles into memory
                use_file = 0;
                break;
            case 'd':           // destination directory
                bundle_dir = optarg;
                break;

            case 'e':           // destination endpoint
                endpoint = optarg;
                break;

            case 'a':           // aggregate
                aggregate = atoi(optarg);
                break;

            case -1:
                done = 1;
                break;
            default:
                print_usage(argv[0]);
                exit(1);
        }
    }

#define CHECK_SET(_arg, _what)                                          \
    if (_arg == 0) {                                                    \
        fprintf(stderr, "\nSYNTAX ERROR: %s must be specified\n", _what);      \
        print_usage(argv[0]);                                                  \
        exit(1);                                                        \
    }

}

/* ----------------------------
 * parse_tuple
 * ---------------------------- */
dtn_endpoint_id_t* parse_eid(dtn_handle_t handle, dtn_endpoint_id_t* eid, char * str)
{
    // try the string as an actual dtn tuple
    if (!dtn_parse_eid_string(eid, str)) 
    {
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
} // end parse_tuple
