/*
 *    Copyright 2004-2006 Intel Corporation
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

#include <oasys/util/ScratchBuffer.h>

#include "AdminRegistration.h"
#include "RegistrationTable.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "bundling/CustodySignal.h"
#include "routing/BundleRouter.h"

namespace dtn {

AdminRegistration::AdminRegistration()
    : Registration(ADMIN_REGID,
                   BundleDaemon::instance()->local_eid(),
                   Registration::DEFER,
                   Registration::NEW, 0, 0)
{
    logpathf("/dtn/reg/admin");
    set_active(true);
}

void
AdminRegistration::deliver_bundle(Bundle* bundle)
{
    u_char typecode;

    size_t payload_len = bundle->payload().length();
    oasys::ScratchBuffer<u_char*, 256> scratch(payload_len);
    const u_char* payload_buf = 
        bundle->payload().read_data(0, payload_len, scratch.buf(payload_len));
    
    log_debug("got %zu byte bundle", payload_len);
        
    if (payload_len == 0) {
        log_err("admin registration got 0 byte *%p", bundle);
        goto done;
    }

    if (!bundle->is_admin()) {
        log_warn("non-admin *%p sent to local eid", bundle);
        goto done;
    }

    /*
     * As outlined in the bundle specification, the first four bits of
     * all administrative bundles hold the type code, with the
     * following values:
     *
     * 0x1     - bundle status report
     * 0x2     - custodial signal
     * 0x3     - echo request
     * 0x4     - null request
     * 0x5     - announce
     * (other) - reserved
     */
    typecode = payload_buf[0] >> 4;
    
    switch(typecode) {
    case BundleProtocol::ADMIN_STATUS_REPORT:
    {
        log_err("status report *%p received at admin registration", bundle);
        break;
    }
    
    case BundleProtocol::ADMIN_CUSTODY_SIGNAL:
    {
        log_info("ADMIN_CUSTODY_SIGNAL *%p received", bundle);
        CustodySignal::data_t data;
        
        bool ok = CustodySignal::parse_custody_signal(&data, payload_buf, payload_len);
        if (!ok) {
            log_err("malformed custody signal *%p", bundle);
            break;
        }

        BundleDaemon::post(new CustodySignalEvent(data));

        break;
    }
    case BundleProtocol::ADMIN_ANNOUNCE:
    {
        log_info("ADMIN_ANNOUNCE from %s", bundle->source().c_str());
        break;
    }
        
    default:
        log_warn("unexpected admin bundle with type 0x%x *%p",
                 typecode, bundle);
    }    

 done:
    BundleDaemon::post(new BundleDeliveredEvent(bundle, this));
}


} // namespace dtn
