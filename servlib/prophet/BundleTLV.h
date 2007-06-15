/*
 *    Copyright 2007 Baylor University
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

#ifndef _PROPHET_BUNDLE_TLV_H_
#define _PROPHET_BUNDLE_TLV_H_

#include <sys/types.h>
#include "BaseTLV.h"
#include "BundleTLVEntry.h"
#include "BundleTLVEntryList.h"

namespace prophet
{

class BundleTLV : public BaseTLV
{
public:
    /**
     * Bundle Offer/Response Header<br>
     * p. 30, 4.4.5<br>
     * <br>
     * After the routing information has been passed, the node will ask the
     * other node to review available bundles and determine which bundles it
     * will accept for relay.  The source relay will determine which bundles
     * to offer based on relative delivery predictabilities as explained in
     * Section 3.6.  The Bundle Offer TLV also lists the bundles that a
     * PRoPHET acknowledgement has been issued for.  Those bundles have the
     * PRoPHET ACK flag set in their entry in the list.  When a node
     * receives a PRoPHET ACK for a bundle, it MUST remove any copies of
     * that bundle from its buffers, but SHOULD keep an entry of the
     * acknowledged bundle to be able to further propagate the PRoPHET ACK.
     * <br><br>
     * The Response message is identical to the request message with the
     * exception that the flag indicate acceptance of the bundle.
     */
    struct BundleTLVHeader {
        u_int8_t type; ///< defined as 0xA2
        u_int8_t flags; ///< TBD
        /**
         * Length of the TLV in octets, including the TLV header and any
         * nested TLVs.
         */
        u_int16_t length;
        /**
         * Number of bundle offer entries.
         */
        u_int16_t offer_count;
        u_int16_t unused__;
    } __attribute__((packed));

    /**
     * Bundle Offer/Response Entry
     * p. 30, 4.4.5
     */
    struct BundleEntry {
        /**
         * ID string of the destination of the bundle as predefined in the
         * dictionary TLV.
         */
        u_int16_t dest_string_id;
        /**
         * The encoding of the B_Flags in the request are: <br>
         * <br>
         *      Flag 0: Custody Offered     0b1 <br>
         *      Flag 1: Reserved            0b1 <br>
         *      Flag 2: Reserved            0b1 <br>
         *      Flag 3: Reserved            0b1 <br>
         *      Flag 4: Reserved            0b1 <br>
         *      Flag 5: Reserved            0b1 <br>
         *      Flag 6: Reserved            0b1 <br>
         *      Flag 7: PRoPHET ACK         0b1 <br>
         * <br>
         * The encoding of the B_flag values in the response are: <br>
         * <br>
         *      Flag 0: Custody Accepted    0b1 <br>
         *      Flag 1: Bundle Accepted     0b1 <br>
         *      Flag 2: Reserved            0b1 <br>
         *      Flag 3: Reserved            0b1 <br>
         *      Flag 4: Reserved            0b1 <br>
         *      Flag 5: Reserved            0b1 <br>
         *      Flag 6: Reserved            0b1 <br>
         *      Flag 7: Reserved            0b1 <br>
         * <br> 
         */
        u_int8_t b_flags;
        u_int8_t unused__;
        u_int32_t creation_timestamp; ///< This bundle's creation timestamp
        u_int32_t seqno; ///< NOT IN SPEC
    } __attribute__((packed));

    /**
     * Flag values for Bundle Offer/Response Entry
     * p. 31, 4.4.5
     */
    typedef enum {
        CUSTODY  = 1 << 0, ///< custody offered or accepted on this bundle
        ACCEPTED = 1 << 1, ///< this bundle is accepted for relay
        ACK      = 1 << 7  ///< Prophet ACK for this bundle
    } bundletlv_flags_t;

    static const size_t BundleTLVHeaderSize = sizeof(struct BundleTLVHeader);

    static const size_t BundleEntrySize = sizeof(struct BundleEntry);

    /**
     * Destructor.
     */
    virtual ~BundleTLV() {}

protected:
    /**
     * Constructor. Protected to force access through derived classes.
     */
    BundleTLV(BaseTLV::prophet_tlv_t type = BaseTLV::UNKNOWN_TLV,
              u_int8_t flags = 0, u_int16_t length = 0)
        : BaseTLV(type,flags,length) {}

    /**
     * Serialize BundleOfferTLVEntry out to no more than len bytes of
     * buffer; return bytes written
     */
    size_t write_bundle_entry(u_int32_t cts, u_int32_t seq, u_int16_t sid,
                             bool custody, bool accept, bool ack,
                             BundleTLVEntry::bundle_entry_t type,
                             u_char* bp, size_t len) const;

    /**
     * Deserialize struct BundleOfferTLVEntry from transport into memory,
     * reading no more than len bytes from buffer; return bytes read
     */
    size_t read_bundle_entry(u_int32_t *cts, u_int32_t *seq, u_int16_t *sid,
                             bool *custody, bool *accept, bool *ack,
                             BundleTLVEntry::bundle_entry_t *type,
                             const u_char* bp, size_t len );
}; // class BundleTLV

}; // namespace prophet

#endif // _PROPHET_BUNDLE_TLV_H_
