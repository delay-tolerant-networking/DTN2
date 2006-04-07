/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * University of Waterloo Open Source License
 * 
 * Copyright (c) 2005 University of Waterloo. All rights reserved. 
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
 *   Neither the name of the University of Waterloo nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY
 * OF WATERLOO OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/time.h>

#include <string>
#include <oasys/debug/Log.h>

#include "dtn_api.h"
#include "TcaController.h"

static const int debug = 1;

static const int MAX_TTL = 604800;      // 604800 seconds == 1 week

// parsed from command line args
static const char* progname;
static std::string node_type = "mobile";
static bool tidy = false;           // discard pending bundles on startup
static std::string link_id;         // published link id of node
static std::string ask_addr;        // clayer address to send ask to
static std::string adv_string;      // published adv string
static int registry_ttl = MAX_TTL;  // registry entry ttl
static int control_ttl = MAX_TTL;   // control bundle ttl


static TcaController::Role  role = TcaController::TCA_MOBILE;


void
print_usage()
{
    fprintf(stderr, "usage: %s [opts]\n"
            "options:\n"
            " -h help\n"
            " -n <node_type: mobile | router | gateway> (default = mobile)\n"
            " -l <link_addr> local contact addr (required for gateway only)\n"
            " -a <link_addr> send ask on startup\n"
            " -d <adv_string> string to send in response to ASK\n"
            " -r <time in seconds> TTL for Registry entries (default = 604800)\n"
            " -e <time in seconds> control bundle expiration time (default = 604800)\n"
            " -t tidy (discard pending bundles on startup)\n",
           progname);
    fprintf(stderr, "usage: %s [-h] -t <node_type: mobile | router |"
            " gateway>\n", progname);
    exit(1);
}


void
parse_options(int argc, const char **argv)
{
    bool done = false;
    int c;

    progname = argv[0];

    while (!done)
    {
        c = getopt(argc, (char **) argv, "htn:l:a:d:r:e:");

        switch (c)
        {
        case 'h':
            print_usage();
            exit(0);
            break;
        case 't':
            tidy = true;
            break;
        case 'n':
            {
                node_type = optarg;
                if (node_type == "mobile")
                    role = TcaController::TCA_MOBILE;
                else if (node_type == "router")
                    role = TcaController::TCA_ROUTER;
                else if (node_type == "gateway")
                    role = TcaController::TCA_GATEWAY;
                else
                    fprintf(stderr, "unknown node type '%s'\n",
                            node_type.c_str());
            }
            break;
        case 'l':
            link_id = optarg;
            break;
        case 'a':
            ask_addr = optarg;
            // TODO: some syntax checking would be a nice idea
            break;
        case 'd':
            adv_string = optarg;
            // TODO: some syntax checking would be a nice idea
            break;
        case 'r':
            {
                int n = atoi(optarg);
                if (n<=0 || n >MAX_TTL)
                {
                    fprintf(stderr, "registry TTL out of range (1..%d)\n",
                            MAX_TTL);
                    registry_ttl = MAX_TTL;
                }
                else
                {
                    registry_ttl = n;
                }
            }
            break;
        case 'e':
             {
                int n = atoi(optarg);
                if (n<=0 || n >MAX_TTL)
                {
                    fprintf(stderr, "control bundle TTL out of range (1..%d)\n",
                            MAX_TTL);
                    control_ttl = MAX_TTL;
                }
                else
                {
                    control_ttl = n;
                }
            }
            break;
        case -1:
            done = true;
            break;
        default:
            print_usage();
            break;
        }
    }

    if (optind < argc)
    {
        fprintf(stderr, "unsupported argument '%s'\n", argv[optind]);
        exit(1);
    }

    // echo args
    printf("using options:\n");
    printf("    node_type = '%s'\n", node_type.c_str());
    printf("    link_id = '%s'\n", link_id.c_str());
    printf("    ask_addr = '%s'\n", ask_addr.c_str());
    printf("    adv_string = '%s'\n", adv_string.c_str());
    printf("    registry_ttl = %d\n", registry_ttl);
    printf("    control_ttl = %d\n", control_ttl);
    if (tidy) printf("    tidy = true\n");
    else printf("    tidy = false\n");

}


// main
int
main(int argc, const char** argv)
{
    oasys::Log::init("-", oasys::LOG_NOTICE, "", "~/.tcadebug");
    log_notice("/tca/admin", "tca_admin starting up");
    
    parse_options(argc, argv);

    TcaController controller(role, link_id, ask_addr, adv_string,
                        registry_ttl, control_ttl);

    if (!controller.init(tidy))
    {
        exit(1);
    }

    controller.run();

    return 0;
}



