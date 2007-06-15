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

#ifndef _PROPHET_RIB_TLV_H_
#define _PROPHET_RIB_TLV_H_

#include "PointerList.h"
#include "Node.h"
#include "BaseTLV.h"

namespace prophet
{

class RIBTLV : public BaseTLV
{
public:

    typedef RIBNodeList::iterator       iterator;
    typedef RIBNodeList::const_iterator const_iterator;

    /** 
     * RIB Header Flags
     * p. 28, 4.4.4
     */
    typedef enum {
        RELAY_NODE       = 1 << 0,
        CUSTODY_NODE     = 1 << 1,
        INTERNET_GW_NODE = 1 << 2
    } rib_header_flag_t;

    /**
     * Routing Information Base TLV <br>
     * p. 28, 4.4.4 <br>
     * <br>
     * The Routing Information Base lists the destinations a node knows of,
     * and the delivery predictabilities it has associated with them.  This
     * information is needed by the PRoPHET algorithm to make decisions on
     * routing and forwarding.
     */
    struct RIBTLVHeader {
        u_int8_t type; ///< defined as 0xA1
        /**
         * The encoding of the Header flag field relates to the
         * capabilities of the Source node sending the RIB:
         * <br>
         *       Flag 0: Relay Node          0b1 <br>
         *       Flag 1: Custody Node        0b1 <br>
         *       Flag 2: Internet GW Node    0b1 <br>
         *       Flag 3: Reserved            0b1 <br>
         *       Flag 4: Reserved            0b1 <br>
         *       Flag 5: Reserved            0b1 <br>
         *       Flag 6: Reserved            0b1 <br>
         *       Flag 7: Reserved            0b1 <br>
         */
        u_int8_t flags;
        /**
         * Length of the TLV in octets, including the TLV header and any
         * nested TLVs.
         */
        u_int16_t length;
        u_int16_t rib_string_count; ///< Number of routing entries in the TLV
        u_int16_t unused__;
    } __attribute__((packed));

    /** 
     * Routing Information Base entry
     * p. 28, 4.4.4
     */
    struct RIBEntry {
        /**
         * ID string as predefined in the dictionary TLV.
         */
        u_int16_t string_id;
        /**
         * Delivery predictability for the destination of this entry as
         * calculated according to the equations in Section 2.1.1.  The
         * encoding of this field is a linear mapping from [0,1] to [0,
         * 0xFF].
         */
        u_int8_t pvalue;
        /**
         * The encoding of the RIB flag field is<br>
         * <br>
         *       Flag 0: Relay Node          0b1 <br>
         *       Flag 1: Custody Node        0b1 <br>
         *       Flag 2: Internet GW Node    0b1 <br>
         *       Flag 3: Reserved            0b1 <br>
         *       Flag 4: Reserved            0b1 <br>
         *       Flag 5: Reserved            0b1 <br>
         *       Flag 6: Reserved            0b1 <br>
         *       Flag 7: Reserved            0b1 <br>
         */
        u_int8_t flags;
    } __attribute__((packed));

    static const size_t RIBTLVHeaderSize = sizeof(struct RIBTLVHeader);

    static const size_t RIBEntrySize = sizeof(struct RIBEntry);

    /**
     * Constructor.
     */
    RIBTLV(const RIBNodeList& nodes,
           bool relay,
           bool custody,
           bool internet=false);

    /**
     * Destructor.
     */
    virtual ~RIBTLV() {}

    /**
     * Virtual from BaseTLV.
     */
    size_t serialize(u_char* bp, size_t len) const;

    ///@{ Accessors
    const RIBNodeList& nodes() const { return nodes_; }
    bool relay() const { return relay_; }
    bool custody() const { return custody_; }
    bool internet() const { return internet_; }
    ///@}

protected:
    friend class TLVFactory<RIBTLV>;

    /**
     * Constructor.  Protected to force access through TLVFactory.
     */
    RIBTLV();

    /**
     * Utility function for decoding RIB header flags.
     */
    static void decode_flags(u_int8_t flags, bool* relay,
                             bool* custody, bool* internet);

    /**
     * Serialize routing information base entry out to no more than
     * len bytes of buffer, using struct RIBEntry; return bytes written
     */
    size_t write_rib_entry(u_int16_t sid, double pvalue, bool relay,
                           bool custody, bool internet,
                           u_char* bp, size_t len) const;

    /**
     * Deserialize RIBEntry struct from transport into memory, reading
     * no more than len bytes from buffer; return bytes read
     */
    size_t read_rib_entry(u_int16_t* sid, double* pvalue,
                          bool* relay, bool* custody, bool* internet,
                          const u_char* bp, size_t len);

    /**
     * Virtual from BaseTLV.
     */
    bool deserialize(const u_char* bp, size_t len);

    RIBNodeList nodes_; ///< List of RIB entries
    bool relay_; ///< Whether this node accepts bundles for relay to
                 ///  other nodes
    bool custody_; ///< Whether this node accepts custody transfers
    bool internet_; ///< Whether this node bridges to the Internet

}; // class RIBTLV

}; // namespace prophet

#endif // _PROPHET_RIB_TLV_H_
