/*
 * IMPORTANT:  READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 * By downloading, copying, installing or using the software you agree to this
 * license.  If you do not agree to this license, do not download, install,
 * copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: 
 * 
 * 	Redistributions of source code must retain the above copyright notice,
 * 	this list of conditions and the following disclaimer. 
 * 
 * 	Redistributions in binary form must reproduce the above copyright
 * 	notice, this list of conditions and the following disclaimer in the
 * 	documentation and/or other materials provided with the distribution. 
 * 
 * 	Neither the name of the Intel Corporation nor the names of its
 * 	contributors may be used to endorse or promote products derived from
 * 	this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR ITS  CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * moteproxy: a DTN <-> mote proxying app, intended for use with GSK
 *   for MICA-2, use 57600 baud rate
 */

/* Modified to recognize Mote-PC protocol -- Mark Thomas (23/06/04) 
 * Now uses serialsource.c to communicate with serial port 
 * Files: crc16.c, misc.c, mote_io.c and mote_io.h are not used anymore */
 
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <string.h>
#include "serialsource.h"

#include <strings.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "dtn_api.h"
 
#define	dout	stderr

#include <ctype.h>

char *progname;

static char *msgs[] = {
  "unknown_packet_type",
  "ack_timeout"	,
  "sync"	,
  "too_long"	,
  "too_short"	,
  "bad_sync"	,
  "bad_crc"	,
  "closed"	,
  "no_memory"	,
  "unix_error"
};

typedef struct data_packet
{
    // MultiHop Header 
    uint16_t source_mote_id;
    uint16_t origin_mote_id;
    uint16_t seq_no;
    uint8_t hop_cnt;

    // Surge Sensor Header 
    uint8_t surge_pkt_type;
    uint16_t surge_reading;
    uint16_t surge_parent_addr;
    uint32_t surge_seq_no;
    uint8_t light;
    uint8_t temp;
    uint8_t magx;
    uint8_t magy;
    uint8_t accelx;
    uint8_t accely;

}DATAPACKET;

#define DATAPACKET_SIZE 22
#define SURGE_PKT	0x11
#define DEBUG_PKT	0x03

void parse_options(int, char**);
dtn_tuple_t * parse_tuple(dtn_handle_t handle, dtn_tuple_t * tuple, 
                          char * str);
void print_usage();
void print_tuple(char * label, dtn_tuple_t * tuple);
void init_motes();
void stderr_msg(serial_source_msg problem);
void usage(char *str1, char *str2);
void readCommandLineArgs(int argc, char **argv);
void hexdump();
void read_packet_file(char* filename);

// specified options
char arg_dest[128];
char arg_target[128];

char devname[128] = "/dev/ttyS0";
char baud[128] = "57600";
char directory[128]="send";
uint32_t debug = 0;             // higher values cause more info to print
serial_source src;

int g_argc;
char **g_argv;

int
main(int argc, char **argv)
{
    /* save in case of crash */
    g_argc = argc;
    g_argv = argv;

    readCommandLineArgs(argc, argv);
    init_motes();

    // NOTREACHED 
    return (EXIT_FAILURE);      // should never get here 
}

void stderr_msg(serial_source_msg problem)
{
  fprintf(stderr, "Note: %s\n", msgs[problem]);
}

int read_packet(char *buf, int *n)
{
    const char *buff;
    int i;
    
    if (debug > 0) fprintf(stdout, "Reading packet:\n");
    
    if(!(buff = read_serial_packet(src, n)))
	return 0;

    if (debug > 0) fprintf(stdout, " ==> : ");
    for (i = 0; i < buff[4]; i++)
        printf(" %02x", buff[i]);
    putchar('\n');

   
    // strip TOS header & copy to buf 
    memset(buf,0,BUFSIZ);
    memcpy(buf,buff,buff[4]+5);
    *n=buff[4] + 5;
    if(buff[2]==SURGE_PKT || buff[2]==DEBUG_PKT) return buff[2];
    return -1;
}

int
reader_thread(void *p)
{

    // loop reading from motes, writing to directory

    static int tcnt=0;
    DATAPACKET *dataPacket;

    // dtn api variables
    int ret;
    dtn_handle_t handle;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid = DTN_REGID_NONE;
    dtn_bundle_spec_t bundle_spec;
    dtn_bundle_payload_t send_payload;
    char demux[4096];

    p = NULL;

    // open the ipc handle
    if (debug > 0) fprintf(stdout, "Opening connection to local DTN daemon\n");

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
    sprintf(demux, "%s/dtnmoteproxy/recv", arg_dest);
    parse_tuple(handle, &bundle_spec.dest, demux);

    // source is local tuple with file path as demux string
    sprintf(demux, "/dtnmoteproxy/send");
    parse_tuple(handle, &bundle_spec.source, demux);

    // reply to is the same as the source
    dtn_copy_tuple(&bundle_spec.replyto, &bundle_spec.source);


    if (debug > 2)
    {
        print_tuple("source_tuple", &bundle_spec.source);
        print_tuple("replyto_tuple", &bundle_spec.replyto);
        print_tuple("dest_tuple", &bundle_spec.dest);
    }

    // set the return receipt option
    bundle_spec.dopts |= DOPTS_RETURN_RCPT;

    // send file and wait for reply

    // create a new dtn registration to receive bundle status reports
    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_tuple(&reginfo.endpoint, &bundle_spec.replyto);
    reginfo.action = DTN_REG_ABORT;
    reginfo.regid = regid;
    reginfo.timeout = 60 * 60;
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration (id=%d): %d (%s)\n",
                regid, ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }
    
    if (debug > 3) printf("dtn_register succeeded, regid 0x%x\n", regid);

    // bind the current handle to the new registration
    dtn_bind(handle, regid, &bundle_spec.replyto);

    while (1) {
        static unsigned char motedata[BUFSIZ];
	int length;
	int ret;

        if (debug > 1) fprintf(dout, "about to read from motes...\n");

	while((ret=read_packet(motedata,&length))){
	    if(ret==DEBUG_PKT)
		continue;
            if (debug > 0) {
                fprintf(dout, "\nreader loop... got [%d] bytes from motes\n", 
                        length);
                if (debug > 1) hexdump(motedata, length);
            }
	    
	    dataPacket=(DATAPACKET *)(motedata);
	    
	    // skip packets from base mote 
	    if(dataPacket->origin_mote_id == 0) continue;
	    
            // fill in a payload
            memset(&send_payload, 0, sizeof(send_payload));

            dtn_set_payload(&send_payload, DTN_PAYLOAD_MEM,
                            motedata, length);
 
            if ((ret = dtn_send(handle, &bundle_spec, &send_payload)) != 0) {
                fprintf(stderr, "error sending bundle: %d (%s)\n",
                        ret, dtn_strerror(dtn_errno(handle)));
            }   
            else fprintf(stderr, "motedata bundle sent");

	    printf("Mote ID = %u\n",dataPacket->origin_mote_id);
	    printf("Source Mote ID = %u\n",dataPacket->source_mote_id);
	    printf("Hop Count = %u\n",dataPacket->hop_cnt);
	    printf("Packet Type = %u\n",dataPacket->surge_pkt_type);
	    printf("Parent Address = %u\n",dataPacket->surge_parent_addr);
	    printf("Sequence Number = %u\n", (u_int)dataPacket->surge_seq_no);
	    printf("Light = %u\n",dataPacket->light);
	    printf("Temperature = %u\n\n",dataPacket->temp);	    

	    tcnt=(tcnt+1)%10000;
	  
        }
        if (debug > 0)
            fprintf(dout, "reader loop.... nothing to do? [shouldn't happen]\n");
    }
    return (1);
    // NOTREACHED 
}


void
readCommandLineArgs(int argc, char **argv)
{
    int c;

    while ((c = getopt(argc, argv, "hr:d:b:D:t:")) != EOF) {
        switch (c) {
        case 'h':
            usage("moteproxy", "");
            exit(0);
            break;
        case 'r':
            read_packet_file(optarg);
            exit(0);
        case 'd':
            debug = atoi(optarg);
            break;
        case 'b':
            strcpy(baud, optarg);
            break;
        case 't':
            strcpy(devname, optarg);
            break;
	case 'D':
	    strcpy(arg_dest,optarg);
	    break;		
        default:
            fprintf(stderr, "mfproxy: unknown option: '%c'\n", (char) c);
            usage("moteproxy", "");
            exit(EXIT_FAILURE);
        }
    }
}

void
usage(char *str1, char *str2)
{
    fprintf(stderr, "usage: %s\n", str1);
    fprintf(stderr, "  [-b baudrate]     - baud rate\n");
    fprintf(stderr, "  [-t devname]      - name of mote network dev tty\n");
    fprintf(stderr, "  [-d debugValue]\n");
    fprintf(stderr, "  [-D directory]\n");
    fprintf(stderr, "  [-h]              - print this message.\n");
    fprintf(stderr, "\n");
}


// initialize the motes
void
init_motes()
{
    src = open_serial_source(devname, atoi(baud), 0, stderr_msg);

    if(reader_thread(NULL) == 1) {
        fprintf(stderr, "couldn't start reader on mote network\n");
        exit(EXIT_FAILURE);
    }
    return;
}

void read_packet_file(char* filename)
{
    int fd = open(filename, O_RDONLY);
    static unsigned char buf[BUFSIZ];
    int n = read(fd, buf, BUFSIZ);
    hexdump(buf, n);
}

void
hexdump(unsigned char *buf, int n)
{
    int i;
    unsigned char *p = buf;

    fprintf(dout,"Packet contains %d:\n",n);
    for (i = 0; i < n; i++) {
        fprintf(dout,"%02x ", *p++);
        if ((i & 0x7) == 0x7)
            fprintf(dout,"\n");
    }
    printf("\n\n");
    fflush(stdout);
}

dtn_tuple_t * parse_tuple(dtn_handle_t handle, 
                          dtn_tuple_t * tuple, char * str)
{
    
    // try the string as an actual dtn tuple
    if (!dtn_parse_tuple_string(tuple, str)) 
    {
        return tuple;
    }
    // build a local tuple based on the configuration of our dtn
    // router plus the str as demux string
    else if (!dtn_build_local_tuple(handle, tuple, str))
    {
        return tuple;
    }
    else
    {
        fprintf(stderr, "invalid tuple string '%s'\n", str);
        exit(1);
    }
}

void print_tuple(char *  label, dtn_tuple_t * tuple)
{
    printf("%s [%s %.*s]\n", label, tuple->region, 
           (int) tuple->admin.admin_len, tuple->admin.admin_val);
}
    
