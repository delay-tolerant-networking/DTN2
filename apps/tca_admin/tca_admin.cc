/*
 *    Copyright 2005-2006 University of Waterloo
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
    log_notice_p("/tca/admin", "tca_admin starting up");
    
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



