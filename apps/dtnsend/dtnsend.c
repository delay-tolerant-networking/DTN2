
#include <stdio.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <time.h>

#include "dtn_api.h"

// global options
dtn_bundle_payload_location_t 
        payload_type    = 0;    // the type of data source for the bundle
int copies              = 1;    // the number of copies to send
int verbose             = 0;

// bundle options
int return_receipts     = 0;    // request end to end return receipts
int forwarding_receipts = 0;    // request per hop departure
int custody             = 0;    // request custody transfer
int custody_receipts    = 0;    // request per custodian receipts
int receive_receipts    = 0;    // request per hop arrival receipt
int overwrite           = 0;    // queue overwrite option
int wait                = 0;    // wait for bundle status reports

char * data_source      = NULL; // filename or message, depending on type

// specified options for bundle tuples
char * arg_replyto      = NULL;
char * arg_source       = NULL;
char * arg_dest         = NULL;

dtn_reg_id_t regid      = DTN_REGID_NONE;


void parse_options(int, char**);
dtn_tuple_t * parse_tuple(dtn_handle_t * handle, dtn_tuple_t * tuple, 
                          char * str);
void print_usage();
void print_tuple(char * label, dtn_tuple_t * tuple);

int
main(int argc, char** argv)
{
    int i;
    int ret;
    dtn_handle_t handle;
    dtn_reg_info_t reginfo;
    dtn_bundle_spec_t bundle_spec;
    dtn_bundle_spec_t reply_spec;
    dtn_bundle_payload_t send_payload;
    dtn_bundle_payload_t reply_payload;
    clock_t start, end;
    
    parse_options(argc, argv);

    // open the ipc handle
    handle = dtn_open();
    if (handle == 0) 
    {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                strerror(errno));
        exit(1);
    }

    // initialize bundle spec
    memset(&bundle_spec, 0, sizeof(bundle_spec));

    // initialize/parse bundle src/dest/replyto tuples
    parse_tuple(&handle, &bundle_spec.dest, arg_dest);
    parse_tuple(&handle, &bundle_spec.source, arg_source);
    if (arg_replyto == NULL) 
        dtn_copy_tuple(&bundle_spec.replyto, &bundle_spec.source);
     else
        parse_tuple(&handle, &bundle_spec.replyto, arg_replyto);

    if (verbose)
    {
        print_tuple("source_tuple", &bundle_spec.source);
        print_tuple("replyto_tuple", &bundle_spec.replyto);
        print_tuple("dest_tuple", &bundle_spec.dest);
    }

    if (wait)
    {
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
    }
    
    // set the dtn options
    if (return_receipts)
    {
        // set the return receipt option
        bundle_spec.dopts |= DOPTS_RETURN_RCPT;
    }

    if (forwarding_receipts)
    {
        // set the forward receipt option
        bundle_spec.dopts |= DOPTS_FWD_RCPT;
    }

    if (custody)
    {
        // request custody transfer
        bundle_spec.dopts |= DOPTS_CUSTODY;
    }        

    if (custody_receipts)
    {
        // request custody transfer
        bundle_spec.dopts |= DOPTS_CUST_RCPT;
    }        

    if (overwrite)
    {
        // queue overwrite option
        bundle_spec.dopts |= DOPTS_OVERWRITE;
    }

    if (receive_receipts)
    {
        // request receive receipt
        bundle_spec.dopts |= DOPTS_RECV_RCPT;
    }
    

    // fill in a payload
    memset(&send_payload, 0, sizeof(send_payload));

    dtn_set_payload(&send_payload, payload_type, data_source, strlen(data_source));
     
    // loop, sending sends and getting replies.
    for (i = 0; i < copies; ++i) {
        start = clock();

        if ((ret = dtn_send(handle, &bundle_spec, &send_payload)) != 0) {
            fprintf(stderr, "error sending bundle: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }

        if (wait)
        {
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
            end = clock();

            printf("%d bytes from [%s %.*s]: time=%.1f ms\n",
                   reply_payload.dtn_bundle_payload_t_u.buf.buf_len,
                   reply_spec.source.region,
                   reply_spec.source.admin.admin_len,
                   reply_spec.source.admin.admin_val,
                   ((double)end - (double)start) * 1000/CLOCKS_PER_SEC);
            
            sleep(1);
        }
    }
    
    return 0;
}

void print_usage()
{
    fprintf(stderr, "usage: dtn_send [-vhew] [-c <copies>] \n");
    fprintf(stderr, "                -s <source_tuple | local_demux_string>\n");
    fprintf(stderr, "                -d <dest_tuple | local_demux_string>\n");
    fprintf(stderr, "                [-r <replyto_tuple | local_demux_string>]\n");
    fprintf(stderr, "                [-i replyto_reg_id]\n");
    fprintf(stderr, "                -type <file|message|date> -a <file_to_send|message>\n");
    fprintf(stderr, " -v verbose\n");
    fprintf(stderr, " -h help\n");
    fprintf(stderr, " -c copies of the bundle to send\n");
    fprintf(stderr, " -e request for end-to-end return receipt\n");
    fprintf(stderr, " -f request for bundle forwarding receipts\n");
    fprintf(stderr, " -w wait for bundle status reports\n");
    fprintf(stderr, " -i registration id for reply to\n");
    fprintf(stderr, " -t file, message, or current date\n");
    
    exit(1);
}

void parse_options(int argc, char**argv)
{
    int c, done = 0;

    while (!done)
    {
        c = getopt(argc, argv, "hH?vn:oacpfyewt:p:r:d:s:i:");
        switch (c)
        {
        case 'v':
            verbose = 1;
            break;
        case 'h':
        case 'H':
        case '?':
            print_usage();
            exit(0);
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
        case 'n':
            copies = atoi(optarg);
            break;
        case 'e':
            return_receipts = 1;
            break;
        case 'w':
            wait = 1;
            break;
        case 'o':
            overwrite = 1;
            break;
        case 'f':
            forwarding_receipts = 1;
            break;
        case 'a':
            receive_receipts = 1;
            break;
        case 'c':
            custody = 1;
            break;
        case 'y':
            custody_receipts = 1;
            break;
        case 't':
            switch (optarg[0])
            {
            case 'f': payload_type = DTN_PAYLOAD_FILE; break;
            case 'm': payload_type = DTN_PAYLOAD_MEM; break;
            case 'd': 
                payload_type = DTN_PAYLOAD_MEM; 
                time_t current = time(NULL);
                data_source = ctime(&current);
                break;
            default:
                fprintf(stderr, "bundle type must be one of file, message, or date\n");
                exit(1);
            }
            break;
        case 'p':
            switch (payload_type)
            {
            case DTN_PAYLOAD_MEM:
            case DTN_PAYLOAD_FILE:
                data_source = optarg;
                break;
            default:
                fprintf(stderr, "-p argument must appear after -t <file|message|date>\n");
                exit(1);
            }
            break;
        case 'i':
            regid = atoi(optarg);
            break;
        case -1:
            done = 1;
        default:
            fprintf(stderr, "%c is an invalid option\n", c);
            print_usage();
            exit(1);
        }
    }

    if ((arg_source == NULL) ||
        (arg_dest == NULL) ||
        (payload_type == 0) ||
        (data_source == NULL))
    {
        fprintf(stderr, "missing required arguments\n");
        print_usage();
        exit(1);
    }
}

dtn_tuple_t * parse_tuple(dtn_handle_t * handle, 
                          dtn_tuple_t * tuple, char * str)
{
    
    // try the string as an actual dtn tuple
    if (dtn_parse_tuple_string(tuple, str)) 
    {
        return tuple;
    }
    // build a local tuple based on the configuration of our dtn
    // router plus the str as demux string
    else if (dtn_build_local_tuple(handle, tuple, str))
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
           tuple->admin.admin_len, tuple->admin.admin_val);
}
    



