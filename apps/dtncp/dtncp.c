
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <time.h>

#include "dtn_api.h"

int verbose             = 1;

char * data_source      = NULL; // filename or message, depending on type

// specified options
char * arg_dest         = NULL;

void parse_options(int, char**);
dtn_tuple_t * parse_tuple(dtn_handle_t handle, dtn_tuple_t * tuple, 
                          char * str);
void print_usage();
void print_tuple(char * label, dtn_tuple_t * tuple);

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

    FILE * file;
    //struct stat finfo;
    char buffer[4096]; // max filesize to send is 4096 (temp)
    int bufsize = 0;
    
    parse_options(argc, argv);

    // open the ipc handle
    verbose && fprintf(stdout, "Opening connection to local DTN daemon\n");

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
    sprintf(demux, "%s/recv_file", arg_dest);
    verbose && fprintf(stdout, "Destination: %s\n", demux);
    parse_tuple(handle, &bundle_spec.dest, demux);

    // source is local tuple with file path as demux string
    sprintf(demux, "/send_file:%s", data_source);
    verbose && fprintf(stdout, "Source: %s\n", demux);
    parse_tuple(handle, &bundle_spec.source, demux);

    // reply to is the same as the source
    dtn_copy_tuple(&bundle_spec.replyto, &bundle_spec.source);


    if (verbose)
    {
        print_tuple("source_tuple", &bundle_spec.source);
        print_tuple("replyto_tuple", &bundle_spec.replyto);
        print_tuple("dest_tuple", &bundle_spec.dest);
    }

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
    
    verbose && printf("dtn_register succeeded, regid 0x%x\n", regid);

    // bind the current handle to the new registration
    dtn_bind(handle, regid, &bundle_spec.replyto);
    
    // set the return receipt option
    bundle_spec.dopts |= DOPTS_RETURN_RCPT;

    // fill in a payload
    memset(&send_payload, 0, sizeof(send_payload));

    file = fopen(data_source, "r");
    
    while (!feof(file) && bufsize < 4096)
    {
        bufsize += fread(buffer + bufsize, 1, 
                         bufsize > 3840? 4096 - bufsize: 256, file);
    }

    dtn_set_payload(&send_payload, DTN_PAYLOAD_MEM, buffer, bufsize);
    //    dtn_set_payload(&send_payload, DTN_PAYLOAD_FILE, data_source, strlen(data_source));
     
    // send file and wait for reply
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


    printf("file sent successfully to [%s %.*s]: time=%.1f ms\n",
                   reply_spec.source.region,
                   reply_spec.source.admin.admin_len,
                   reply_spec.source.admin.admin_val,
                   ((double)(end.tv_sec - start.tv_sec) * 1000.0 + 
                    (double)(end.tv_usec - start.tv_usec)/1000.0));
            
    return 0;
}

void print_usage()
{
    fprintf(stderr, "usage: dcp file bundles://region/host://admin/path\n");
    
    exit(1);
}

void parse_options(int argc, char**argv)
{
    if (argc < 3)
    {
        print_usage();
        exit(1);
    }

    data_source = argv[1];
    arg_dest = argv[2];
}

dtn_tuple_t * parse_tuple(dtn_handle_t handle, 
                          dtn_tuple_t * tuple, char * str)
{
    
    // try the string as an actual dtn tuple
    if (!dtn_parse_tuple_string(tuple, str)) 
    {
        verbose && fprintf(stdout, "%s (literal)\n", str);
        return tuple;
    }
    // build a local tuple based on the configuration of our dtn
    // router plus the str as demux string
    else if (!dtn_build_local_tuple(handle, tuple, str))
    {
        verbose && fprintf(stdout, "%s (local)\n", str);
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
           tuple->admin.admin_len, tuple->admin.admin_val);
}
    



