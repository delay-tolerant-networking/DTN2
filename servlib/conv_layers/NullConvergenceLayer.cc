/*
 *    Copyright 2006 Intel Corporation
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

#include "NullConvergenceLayer.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

/**
 * Open the given contact.
 */
bool
NullConvergenceLayer::open_contact(const ContactRef& contact)
{
    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    BundleDaemon::post(new ContactUpEvent(contact));
    return true;
}

void
NullConvergenceLayer::send_bundle(const ContactRef& contact, Bundle* bundle)
{
    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

    BlockInfoVec* blocks = bundle->xmit_blocks_.find_blocks(link);
    ASSERT(blocks != NULL);
    size_t total_len = BundleProtocol::total_length(blocks);
    
    log_debug("send_bundle *%p to *%p (total len %zu)",
              bundle, contact.object(), total_len);
    
    BundleDaemon::post(
        new BundleTransmittedEvent(bundle, contact, link, total_len, 0));
}

} // namespace dtn
