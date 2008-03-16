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
#  include <dtn-config.h>
#endif

#include <oasys/util/StringBuffer.h>
#include <oasys/util/OptParser.h>

#include "bundling/BundleDaemon.h"
#include "routing/BundleRouter.h"
#include "prophet/QueuePolicy.h"
#include "prophet/FwdStrategy.h"
#include "prophet/Params.h"
// default settings for ProphetRouter::params_ are set in prophet/Params.h
#include "routing/ProphetRouter.h"

#include "ProphetCommand.h"

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

    add_to_help("queue_policy=<policy>",
                "set queue policy to one of the following:\n"
                "\tfifo\tfirst in first out\n"
                "\tmofo\tevict most forwarded first\n"
                "\tmopr\tevict most favorably forwarded first\n"
                "\tlmopr\tevict most favorably forwarded first (linear increase)\n"
                "\tshli\tevict shortest lifetime first\n"
                "\tlepr\tevice least probable first\n");

    add_to_help("fwd_strategy=<strategy>",
                "set forwarding strategy to one of the following:\n"
                "\tgrtr\tforward if remote's P is greater\n"
                "\tgtmx\tforward if \"grtr\" and NF < NF_Max\n"
                "\tgrtr_plus\tforward if \"grtr\" and P > P_Max\n"
                "\tgtmx_plus\tforward if \"grtr_plus\" and NF < NF_Max\n"
                "\tgrtr_sort\tforward if \"grtr\" and sort desc by P_remote - P_local\n"
                "\tgrtr_max\tforward if \"grtr\" and sort desc by P_remote\n");

    add_to_help("hello_interval=<interval>",
                "maximum delay between protocol messages, in 100ms units,"
                " ranging from 1 to 255 (100 ms to 25.5s)");

    add_to_help("max_route=<number>",
                "maximum number of routes for Prophet to retain"
                " (set to 0 to disable quota)");
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
            {"grtr",      prophet::FwdStrategy::GRTR},
            {"gtmx",      prophet::FwdStrategy::GTMX},
            {"grtr_plus", prophet::FwdStrategy::GRTR_PLUS},
            {"gtmx_plus", prophet::FwdStrategy::GTMX_PLUS},
            {"grtr_sort", prophet::FwdStrategy::GRTR_SORT},
            {"grtr_max",  prophet::FwdStrategy::GRTR_MAX},
            {0, 0}
        };
        int fs_pass = ProphetRouter::params_.fs_;
        p.addopt(new oasys::EnumOpt("fwd_strategy",
                    FwdStrategyCases, &fs_pass, "",
                    "forwarding strategies"));
        if (! p.parse(argc,argv,&invalid))
        {
            resultf("bad parameter for fwd_strategy: %s",invalid);
            return TCL_ERROR;
        }

        ProphetRouter::params_.fs_ =
            (prophet::FwdStrategy::fwd_strategy_t)fs_pass;

        resultf("fwd_strategy set to %s",
                prophet::FwdStrategy::fs_to_str(ProphetRouter::params_.fs_));
    }
    else
    if (strncmp(cmd,"queue_policy",strlen("queue_policy")) == 0)
    {
        oasys::EnumOpt::Case QueuePolicyCases[] =
        {
            {"fifo",  prophet::QueuePolicy::FIFO},
            {"mofo",  prophet::QueuePolicy::MOFO},
            {"mopr",  prophet::QueuePolicy::MOPR},
            {"lmopr", prophet::QueuePolicy::LINEAR_MOPR},
            {"shli",  prophet::QueuePolicy::SHLI},
            {"lepr",  prophet::QueuePolicy::LEPR},
            {0, 0}
        };
        int qp_pass;
        p.addopt(new oasys::EnumOpt("queue_policy",
                    QueuePolicyCases, &qp_pass, "",
                    "queueing policies as put forth by Prophet, March 2006"));
        if (! p.parse(argc,argv,&invalid))
        {
            resultf("bad parameter for queue_policy: %s",invalid);
            return TCL_ERROR;
        }

        prophet::QueuePolicy::q_policy_t qp =
            (prophet::QueuePolicy::q_policy_t)qp_pass;

        ProphetRouter::params_.qp_ = qp;
        if (ProphetRouter::is_init())
        {
            ProphetRouter* r = dynamic_cast<ProphetRouter*>(
                                    BundleDaemon::instance()->router());
            if (r != NULL)
                r->set_queue_policy();
        }
        resultf("queue_policy set to %s", prophet::QueuePolicy::qp_to_str(qp));
    }
    else
    if (strncmp(cmd,"hello_interval",strlen("hello_interval")) == 0)
    {
        u_int8_t hello_interval;
        p.addopt(new oasys::UInt8Opt("hello_interval",
                 &hello_interval,"seconds",
                 "100s of milliseconds between HELLO beacons (between 1 "
                 "and 255)"));

        if (! p.parse(argc,argv,&invalid))
        {
            resultf("bad parameter for hello_interval: %s",invalid);
            return TCL_ERROR;
        }

        ProphetRouter::params_.hello_interval_ = hello_interval;
        if (ProphetRouter::is_init())
        {
            ProphetRouter* r = dynamic_cast<ProphetRouter*>(
                    BundleDaemon::instance()->router());
            if (r != NULL)
                r->set_hello_interval();
        }
        resultf("hello_interval set to %d",hello_interval);
    }
    else
    if (strncmp(cmd,"max_route",strlen("max_route")) == 0)
    {
        u_int max_route;
        p.addopt(new oasys::UIntOpt("max_route",
                 &max_route, "maximum number",
                 "maximum number of routes for Prophet to retain"));

        if (! p.parse(argc,argv,&invalid))
        {
            resultf("bad parameter for hello_interval: %s",invalid);
            return TCL_ERROR;
        }

        ProphetRouter::params_.max_table_size_ = max_route;
        if (ProphetRouter::is_init())
        {
            ProphetRouter* r = dynamic_cast<ProphetRouter*>(
                    BundleDaemon::instance()->router());
            if (r != NULL)
                r->set_max_route();
        }
        resultf("max_route set to %u",max_route);
    }

    return TCL_OK;
}

} // namespace dtn
