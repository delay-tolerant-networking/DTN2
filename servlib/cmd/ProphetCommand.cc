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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <oasys/util/StringBuffer.h>
#include <oasys/util/OptParser.h>

#include "ProphetCommand.h"
#include "routing/BundleRouter.h"
#include "routing/Prophet.h"
// default settings for ProphetRouter::params_ are set in ProphetLists.h
#include "routing/ProphetRouter.h"

namespace dtn {

ProphetCommand::ProphetCommand()
    : TclCommand("prophet")
{
    bind_var(new oasys::DoubleOpt("encounter",
                                  &ProphetRouter::params_.encounter_,
                                  "val",
                                  "predictability initialization constant "
                                  "(between 0 and 1)"));

    bind_var(new oasys::DoubleOpt("beta", &ProphetRouter::params_.beta_, 
                                  "val",
                                  "weight factor for transitive predictability "
                                  "(between 0 and 1)"));

    bind_var(new oasys::DoubleOpt("gamma", &ProphetRouter::params_.gamma_, 
                                  "val",
                                  "weight factor for predictability aging "
                                  "(between 0 and 1)"));

    bind_var(new oasys::UIntOpt("kappa", &ProphetRouter::params_.kappa_, 
                                "val",
                                "scaling factor for aging equation"));

    bind_var(new oasys::UIntOpt("hello_dead", &ProphetRouter::params_.hello_dead_,
                                "num",
                                "number of HELLO intervals before "
                                "peer considered unreachable"));

    bind_var(new oasys::UIntOpt("max_forward",
                                &ProphetRouter::params_.max_forward_,
                                "num",
                                "max times to forward bundle using GTMX"));

    bind_var(new oasys::UIntOpt("min_forward",
                                &ProphetRouter::params_.min_forward_,
                                "num",
                                "min times to forward bundle using LEPR"));

    bind_var(new oasys::UIntOpt("age_period", &ProphetRouter::params_.age_period_,
                                "val",
                                "timer setting for aging algorithm and "
                                "Prophet ACK expiry"));

    bind_var(new oasys::BoolOpt("relay_node",
                                &ProphetRouter::params_.relay_node_,
                                "whether this node forwards bundles "
                                "to other Prophet nodes"));

    bind_var(new oasys::BoolOpt("internet_gw",
                                &ProphetRouter::params_.internet_gw_,
                                "whether this node forwards bundles to "
                                "Internet domain"));

    // smallest double that can fit into 8 bits:  0.0039 (1/255)
    bind_var(new oasys::DoubleOpt("epsilon", &ProphetRouter::params_.epsilon_,
                                  "val",
                                  "lower limit on predictability before "
                                  "dropping route"));

    add_to_help("queue_policy <policy>",
                "set queue policy to one of the following:\n"
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

    add_to_help("max_usage","not used - superceded by \"storage payload_quota\"");
}

int
ProphetCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    
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
        int fs_pass = ProphetRouter::params_.fs_;
        p.addopt(new oasys::EnumOpt("fwd_strategy",
                    FwdStrategyCases, &fs_pass, "",
                    "forwarding strategies"));
        ProphetRouter::params_.fs_ = (Prophet::fwd_strategy_t)fs_pass;
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
        int qp_pass;
        p.addopt(new oasys::EnumOpt("queue_policy",
                    QueuePolicyCases, &qp_pass, "",
                    "queueing policies as put forth by Prophet, March 2006"));
        Prophet::q_policy_t qp = (Prophet::q_policy_t)qp_pass;
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
        resultf("\"prophet max_usage\" no longer supported, please refer to "
                "\"storage payload_quota\"");
        return TCL_ERROR;
    }

    return TCL_OK;
}

} // namespace dtn
