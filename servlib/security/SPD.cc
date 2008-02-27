/*
 * Copyright 2007 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You
 * may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 */

/*
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include "SPD.h"
#include "Ciphersuite.h"
#include "Ciphersuite_BA1.h"
#include "Ciphersuite_PS2.h"
#include "Ciphersuite_C3.h"

namespace dtn {

template <>
SPD* oasys::Singleton<SPD, false>::instance_ = NULL;

static const char * log = "/dtn/bundle/security";

SPD::SPD()
    : global_policy_inbound_(SPD_USE_NONE),
      global_policy_outbound_(SPD_USE_NONE)
{
}

SPD::~SPD()
{
}

void
SPD::init()
{       
    if (instance_ != NULL) 
    {
        PANIC("SPD already initialized");
    }
    
    instance_ = new SPD();
	log_debug_p(log, "SPD::init() done");
}

void
SPD::set_global_policy(spd_direction_t direction, spd_policy_t policy)
{
    ASSERT(direction == SPD_DIR_IN || direction == SPD_DIR_OUT);
    ASSERT((policy & ~(SPD_USE_BAB | SPD_USE_CB | SPD_USE_PSB)) == 0);
    if (direction == SPD_DIR_IN)
        instance()->global_policy_inbound_ = policy;
    else
        instance()->global_policy_outbound_ = policy;
	log_debug_p(log, "SPD::set_global_policy() done");
}

void
SPD::prepare_out_blocks(const Bundle* bundle, const LinkRef& link,
                    BlockInfoVec* xmit_blocks)
{
    spd_policy_t policy = find_policy(SPD_DIR_OUT, bundle);
    
    if (policy & SPD_USE_PSB) {
        Ciphersuite* bp =
            Ciphersuite::find_suite(Ciphersuite_PS2::CSNUM_PS2);
        ASSERT(bp != NULL);
        bp->prepare(bundle, xmit_blocks, NULL, link,
                    BlockInfo::LIST_NONE);
    }

    if (policy & SPD_USE_CB) {
        Ciphersuite* bp =
            Ciphersuite::find_suite(Ciphersuite_C3::CSNUM_C3);
        ASSERT(bp != NULL);
        bp->prepare(bundle, xmit_blocks, NULL, link,
                    BlockInfo::LIST_NONE);
    }

    if (policy & SPD_USE_BAB) {
        Ciphersuite* bp =
            Ciphersuite::find_suite(Ciphersuite_BA1::CSNUM_BA1);
        ASSERT(bp != NULL);
        bp->prepare(bundle, xmit_blocks, NULL, link,
                    BlockInfo::LIST_NONE);
    }
	log_debug_p(log, "SPD::prepare_out_blocks() done");
}

bool
SPD::verify_in_policy(const Bundle* bundle)
{
    spd_policy_t policy = find_policy(SPD_DIR_IN, bundle);
    const BlockInfoVec* recv_blocks = &bundle->recv_blocks();

	log_debug_p(log, "SPD::verify_in_policy() 0x%x", policy);

    if (policy & SPD_USE_BAB) {
        if ( !Ciphersuite::check_validation(bundle, recv_blocks, Ciphersuite_BA1::CSNUM_BA1 )) {
        	log_debug_p(log, "SPD::verify_in_policy() no BP_TAG_BAB_IN_DONE");
            return false;
        }
    }
    
    if (policy & SPD_USE_CB) {
        if ( !Ciphersuite::check_validation(bundle, recv_blocks, Ciphersuite_C3::CSNUM_C3 )) {
        	log_debug_p(log, "SPD::verify_in_policy() no BP_TAG_CB_IN_DONE");
            return false;
        }
    }
    
    if (policy & SPD_USE_PSB) {
        if ( !Ciphersuite::check_validation(bundle, recv_blocks, Ciphersuite_PS2::CSNUM_PS2 )) {
        	log_debug_p(log, "SPD::verify_in_policy() no BP_TAG_PSB_IN_DONE");
            return false;
        }
    }
            
    return true;
}

SPD::spd_policy_t
SPD::find_policy(spd_direction_t direction, const Bundle* bundle)
{
    ASSERT(direction == SPD_DIR_IN || direction == SPD_DIR_OUT);

    (void)bundle;
	log_debug_p(log, "SPD::find_policy()");

    return (direction == SPD_DIR_IN ? instance()->global_policy_inbound_
            : instance()->global_policy_outbound_);
}

} // namespace dtn

#endif  /* BSP_ENABLED */
