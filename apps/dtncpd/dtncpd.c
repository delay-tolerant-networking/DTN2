
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include "dtn_api.h"

#define BUFSIZE 16

void
usage()
{
    fprintf(stderr, "usage: dtn_recv\n");
    exit(1);
}

int
main(int argc, const char** argv)
{
    int i;
    int ret;
    dtn_handle_t handle;
    dtn_tuple_t local_tuple;
    dtn_reg_info_t reginfo;
    dtn_reg_id_t regid;
    dtn_bundle_spec_t spec;
    dtn_bundle_payload_t payload;
    char* endpoint;
    int debug = 1;

    char * region;
    int admin_len;
    char * admin;
    char * host;
    char * path;
    char * filename;
    time_t current;

/*     char buffer[BUFSIZE]; */
    char * buffer;
    char s_buffer[BUFSIZE + 1];
    s_buffer[BUFSIZE] = '\0';
    
    int bufsize, marker, maxwrite;
    FILE * target;
    
    if (argc > 1) {
        usage();
    }

    endpoint = "/recv_file";

    // open the ipc handle
    debug && printf("opening connection to dtn router...\n");
    handle = dtn_open();
    if (handle == 0) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                strerror(errno));
        exit(1);
    }

    // build a local tuple based on the configuration of our dtn
    // router plus the demux string
    dtn_build_local_tuple(handle, &local_tuple, endpoint);
    debug && printf("local_tuple [%s %.*s]\n",
                    local_tuple.region,
                    local_tuple.admin.admin_len, local_tuple.admin.admin_val);

    // create a new registration based on this tuple
    memset(&reginfo, 0, sizeof(reginfo));
    dtn_copy_tuple(&reginfo.endpoint, &local_tuple);
    reginfo.action = DTN_REG_ABORT;
    reginfo.regid = DTN_REGID_NONE;
    reginfo.timeout = 60 * 60;
    if ((ret = dtn_register(handle, &reginfo, &regid)) != 0) {
        fprintf(stderr, "error creating registration: %d (%s)\n",
                ret, dtn_strerror(dtn_errno(handle)));
        exit(1);
    }
    
    debug && printf("dtn_register succeeded, regid 0x%x\n", regid);

    // bind the current handle to the new registration
    dtn_bind(handle, regid, &local_tuple);
    
    printf("dtn_recv [%s %.*s]...\n",
           local_tuple.region,
           local_tuple.admin.admin_len, local_tuple.admin.admin_val);
    
    // loop waiting for bundles
    while(1)
    {
        memset(&spec, 0, sizeof(spec));
        memset(&payload, 0, sizeof(payload));
        
        if ((ret = dtn_recv(handle, &spec,
                            DTN_PAYLOAD_MEM, &payload, -1)) < 0)
        {
            fprintf(stderr, "error getting recv reply: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
            exit(1);
        }

        // mark time received
        current = time(NULL);

        // parse admin string to select file target location
        
        // copy out admin value into null-terminated string
        admin_len = spec.source.admin.admin_len;
        admin = malloc(sizeof(char) * (admin_len+1));
        memcpy(admin, spec.source.admin.admin_val, admin_len);
        admin[admin_len] = '\0';

        // region is the same
        region = spec.source.region;
        
        // extract hostname (ignore protocol)
        host = strstr(admin, "://");
        host += 3; // skip ://

        // extract directory path (everything following std demux)
        path = strstr(host, "/send_file:");
        path[0] = '\0'; // null-terminate host
        path += 11; // skip /send_file:

        // filename is everything following last /
        filename = strrchr(path, '/');
        if (filename == 0)
        {
            filename = path;
            path = NULL;
        }
        else
        {
            filename[0] = '\0'; // null terminate path
            filename++; // next char;
        }

        // bundle name is the name of the bundle payload file
        buffer = payload.dtn_bundle_payload_t_u.buf.buf_val;
        bufsize = payload.dtn_bundle_payload_t_u.buf.buf_len;

        printf ("======================================\n");
        printf (" File Received at %s\n", ctime(&current));
        printf ("   region : %s\n", region);
        printf ("   host   : %s\n", host);
        printf ("   path   : %s\n", path);
        printf ("   file   : %s\n", filename);
        printf ("   size   : %d bytes\n", bufsize);

        
        debug && printf ("--------------------------------------\n");

        target = fopen(filename, "w+");
        
        marker = 0;
        while (marker < bufsize)
        {
            // write 256 bytes at a time
            i=0;
            maxwrite = (marker + 256) > bufsize? bufsize-marker : 256;
            while (i < maxwrite)
            {
                i += fwrite(buffer + marker + i, 1, maxwrite - i, target);
            }
            
            if (debug)
            {
                for (i=0; i < maxwrite; i++)
                {
                    if (buffer[marker] >= ' ' && buffer[marker] <= '~')
                        s_buffer[marker%BUFSIZE] = buffer[i];
                    else
                        s_buffer[marker%BUFSIZE] = '.';
                    

                    if (marker%BUFSIZE == 0) // new line every 16 bytes
                    {
                        printf("%07x ", marker);
                    }
                    else if (marker%2 == 0)
                    {
                        printf(" "); // space every 2 bytes
                    }
                    
                    printf("%02x", buffer[i]);
                    
                    // print character summary (a la emacs hexl-mode)
                    if (marker%BUFSIZE == BUFSIZE-1)
                    {
                        printf(" |  %s\n", s_buffer);
                    }
                    marker ++;
                }
            }
            else
            {
                marker += maxwrite;
            }
        }

        // round off last line
        if (debug && marker % BUFSIZE != 0)
        {
            while (marker % BUFSIZE !=0)
            {
                s_buffer[marker%BUFSIZE] = ' ';

                if (marker%2 == 0)
                {
                    printf(" "); // space every 2 bytes
                }
                    
                printf("  ");
                    
                // print character summary (a la emacs hexl-mode)
                if (marker%BUFSIZE == BUFSIZE-1)
                {
                    printf(" |  %s\n", s_buffer);
                }
                marker ++;
            }
        }

        printf ("======================================\n");
        
        free(admin);
    }
    
    return 0;
}
