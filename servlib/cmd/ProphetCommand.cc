/*
 *    Copyright 2006 Baylor University
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

#include <oasys/util/StringBuffer.h>
#include <oasys/util/OptParser.h>

#include "ProphetCommand.h"
#include "routing/BundleRouter.h"
#include "routing/Prophet.h"
#include "routing/ProphetRouter.h"

namespace dtn {

ProphetCommand::ProphetCommand()
    : TclCommand("prophet")
{

    // See Prophet.h for default values
    ProphetRouter::params_.hello_interval_ = Prophet::HELLO_INTERVAL;
    ProphetRouter::params_.max_usage_      = 0xffffffff;
                   fwd_strategy_           = "grtr";
    ProphetRouter::params_.fs_             = Prophet::GRTR;
                   q_policy_               = "fifo";
    ProphetRouter::params_.qp_             = Prophet::FIFO;

    bind_d("encounter", &ProphetRouter::params_.encounter_,
           Prophet::DEFAULT_P_ENCOUNTER,
           "predictability initialization constant (between 0 and 1)");

    bind_d("beta", &ProphetRouter::params_.beta_, Prophet::DEFAULT_BETA,
           "weight factor for transitive predictability (between 0 and 1)");

    bind_d("gamma", &ProphetRouter::params_.gamma_, Prophet::DEFAULT_GAMMA,
           "weight factor for predictability aging (between 0 and 1)");

    bind_i("kappa", &ProphetRouter::params_.kappa_, Prophet::DEFAULT_KAPPA,
           "scaling factor for aging equation");

    bind_i("hello_dead", &ProphetRouter::params_.hello_dead_,
           Prophet::HELLO_DEAD,
           "number of HELLO intervals before peer considered unreachable");

    bind_i("max_forward", &ProphetRouter::params_.max_forward_,
           Prophet::DEFAULT_NUM_F_MAX,
           "max times to forward bundle using GTMX");

    bind_i("min_forward", &ProphetRouter::params_.min_forward_,
           Prophet::DEFAULT_NUM_F_MIN,
           "min times to forward bundle using LEPR");

    bind_i("age_period", &ProphetRouter::params_.age_period_,
           Prophet::AGE_PERIOD,
           "timer setting for aging algorithm and Prophet ACK expiry");

    bind_b("relay_node", &ProphetRouter::params_.relay_node_, false,
           "whether this node forwards bundles outside Prophet domain");

    bind_b("custody_node", &ProphetRouter::params_.custody_node_, false,
           "whether this node will accept custody transfers");

    bind_b("internet_gw", &ProphetRouter::params_.internet_gw_, false,
           "whether this node forwards bundles to Internet domain");

    // smallest double that can fit into 8 bits:  0.0039
    bind_d("epsilon", &ProphetRouter::params_.epsilon_, 0.0039,
            "lower limit on predictability before dropping route");

    add_to_help("queue_policy <policy>","set queue policy to one of the following:\n"
                "\tfifo\tfirst in first out\n"
                "\tmofo\tevict most forwarded first\n"
                "\tmopr\tevict most favorably forwarded first\n"
                "\tlmopr\tevict most favorably forwarded first (linear increase)\n"
                "\tshli\tevict shortest lifetime first\n"
                "\tlepr\tevice least probable first\n");

    add_to_help("fwd_strategy <strategy>",
                "set forwarding strategy to one of the following:\n"
                "\tgrtr\tforward if remote's P is greater\n"
                "\tgtmx\tforward if \"grtr\" and NF < NF_Max\n"
                "\tgrtr_plus\tforward if \"grtr\" and P > P_Max\n"
                "\tgtmx_plus\tforward if \"grtr_plus\" and NF < NF_Max\n"
                "\tgrtr_sort\tforward if \"grtr\" and sort desc by P_remote - P_local\n"
                "\tgrtr_max\tforward if \"grtr\" and sort desc by P_remote\n");

    add_to_help("max_usage <bytes>","set upper limit on bundle store utilization");
}

int
ProphetCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    
    /*
       wish list:
            - define a Prophet node: EID, pvalue, custody, relay, internet
            - define "show" command for each Prophet variable
     */

    if (argc != 2)
    {
        resultf("prophet: wrong number of arguments, got %d looking for 2",
                argc);
        return TCL_ERROR;
    }

    const char* cmd = argv[1];

    // scoot past "prophet foo" to the value
    argc -= 1;
    argv += 1;

    oasys::OptParser p;
    const char* invalid = NULL;

    if (strncmp(cmd,"fwd_strategy",strlen("fwd_strategy")) == 0)
    {
        oasys::EnumOpt::Case FwdStrategyCases[] =
        {
            {"grtr",      Prophet::GRTR},
            {"gtmx",      Prophet::GTMX},
            {"grtr_plus", Prophet::GRTR_PLUS},
            {"gtmx_plus", Prophet::GTMX_PLUS},
            {"grtr_sort", Prophet::GRTR_SORT},
            {"grtr_max",  Prophet::GRTR_MAX},
            {0, 0}
        };
        p.addopt(new oasys::EnumOpt("fwd_strategy",
                    FwdStrategyCases, (int*)&ProphetRouter::params_.fs_, "",
                    "forwarding strategies"));
        if (! p.parse(argc,argv,&invalid))
        {
            resultf("bad parameter for fwd_strategy: %s",invalid);
            return TCL_ERROR;
        }

        resultf("fwd_strategy set to %s",
                Prophet::fs_to_str(ProphetRouter::params_.fs_));
    }
    else
    if (strncmp(cmd,"queue_policy",strlen("queue_policy")) == 0)
    {
        oasys::EnumOpt::Case QueuePolicyCases[] =
        {
            {"fifo",        Prophet::FIFO},
            {"mofo",        Prophet::MOFO},
            {"mopr",        Prophet::MOPR},
            {"lmopr",       Prophet::LINEAR_MOPR},
            {"shli",        Prophet::SHLI},
            {"lepr",        Prophet::LEPR},
            {0, 0}
        };
        Prophet::q_policy_t qp;
        p.addopt(new oasys::EnumOpt("queue_policy",
                    QueuePolicyCases, (int*)&qp, "",
                    "queueing policies as put forth by Prophet, March 2006"));
        if (! p.parse(argc,argv,&invalid))
        {
            resultf("bad parameter for queue_policy: %s",invalid);
            return TCL_ERROR;
        }

        // if instance exists, post a change
        if (ProphetController::is_init())
            ProphetController::instance()->handle_queue_policy_change(qp);
        else 
        // else push to params where it will get picked up at init time
            ProphetRouter::params_.qp_ = qp;

        resultf("queue_policy set to %s", Prophet::qp_to_str(qp));
    }
    else
    if (strncmp(cmd,"hello_interval",strlen("hello_interval")) == 0)
    {
        u_int8_t hello_interval;
        p.addopt(new oasys::UInt8Opt("hello_interval",
                 &hello_interval,"100s of milliseconds",
                 "100ms time units between HELLO beacons (1 - 255)"));

        if (! p.parse(argc,argv,&invalid))
        {
            resultf("bad parameter for hello_interval: %s",invalid);
            return TCL_ERROR;
        }

        if (ProphetController::is_init())
            ProphetController::instance()->handle_hello_interval_change(
                hello_interval);
        else
            ProphetRouter::params_.hello_interval_ = hello_interval;

        resultf("hello_interval set to %d",hello_interval);
    }
    else
    if (strncmp(cmd,"max_usage",strlen("max_usage")) == 0)
    {
        u_int max_usage;
        p.addopt(new oasys::UIntOpt("max_usage",
                 &max_usage,"usage in bytes",
                 "upper limit on Bundle storage utilization"));

        if (! p.parse(argc,argv,&invalid))
        {
            resultf("bad parameter for max_usage: %s",invalid);
            return TCL_ERROR;
        }

        if (ProphetController::is_init())
            ProphetController::instance()->handle_max_usage_change(max_usage);
        else
            ProphetRouter::params_.max_usage_ = max_usage;

        resultf("max_usage set to %d",max_usage);
    }

    return TCL_OK;
}

} // namespace dtn
