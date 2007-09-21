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

#ifndef _SPD_H_
#define _SPD_H_

#ifdef BSP_ENABLED

#include <oasys/util/Singleton.h>
#include "bundling/Bundle.h"
#include "bundling/BlockInfo.h"
#include "contacts/Link.h"

namespace dtn {

/**
 * Not a real (IPsec-like) SPD, just a placeholder that contains:
 *   - global BAB on/off setting
 *   - global PSB on/off setting
 *   - global CB on/off setting
 *   - preshared secret for BAB
 *   - public keys for PSB and CB
 */
class SPD : public oasys::Singleton<SPD, false> {
public:

    typedef enum {
        SPD_DIR_IN,
        SPD_DIR_OUT
    } spd_direction_t;

    typedef enum {
        SPD_USE_NONE  = 0,
        SPD_USE_BAB   = 1 << 0,
        SPD_USE_CB    = 1 << 1,
        SPD_USE_PSB   = 1 << 2,
    } spd_policy_t;

    /**
     * Constructor (called at startup).
     */
    SPD();

    /**
     * Destructor (called at shutdown).
     */
    ~SPD();

    /**
     * Boot time initializer.
     */
    static void init();

    /**
     * Set global policy to a bitwise-OR'ed combination of
     * SPD_USE_BAB, SPD_USE_PSB, and/or SPD_USE_CB.  SPD_USE_NONE can
     * also be specified to turn security features off entirely.
     */
    static void set_global_policy(spd_direction_t direction,
                                  spd_policy_t policy);

    /**
     * Add the security blocks required by security policy for the
     * given outbound bundle.
     */
    static void prepare_out_blocks(const Bundle* bundle,
                                   const LinkRef& link,
                                   BlockInfoVec* xmit_blocks);

    /**
     * Check whether sequence of BP_Tags created during input processing
     * meets the security policy for this bundle.
     */
    static bool verify_in_policy(const Bundle* bundle);

private:
    spd_policy_t global_policy_inbound_;
    spd_policy_t global_policy_outbound_;

    /**
     * Return the policy for the given bundle in the given direction.
     *
     * XXX For now this just returns the global policy regardless of
     * the value of the 'bundle' argument; in the future it should be
     * moddified to look up an SPD entry indexed by source and
     * destination EndpointIDPatterns.
     */
    static spd_policy_t find_policy(spd_direction_t direction,
                                    const Bundle* bundle);

};

} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _SPD_H_ */
