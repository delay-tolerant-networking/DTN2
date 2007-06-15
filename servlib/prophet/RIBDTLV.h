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

#ifndef _PROPHET_RIBD_TLV_H
#define _PROPHET_RIBD_TLV_H

#include "BaseTLV.h"
#include "Dictionary.h"

namespace prophet
{

class RIBDTLV : public BaseTLV
{
public:
    /**
     * Routing Information Base Dictionary TLV<br>
     * p. 27, 4.4.3<br>
     * <br>
     * The Routing Information Base Dictionary includes the list of
     * addresses used in making routing decisions.  The referents remain
     * constant for the duration of a session over a link where the instance
     * numbers remain the same and can be used by both the Routing
     * Information Base messages and the bundle offer messages.
     */
    struct RIBDTLVHeader {
        u_int8_t type; ///< defined as 0xA0
        u_int8_t flags; ///< TBD
        /**
         * Length of the TLV in octets, including the TLV header and any
         * nested TLVs.
         */
        u_int16_t length;
        u_int16_t entry_count; ///< Number of entries in the database
        u_int16_t unused__;
    } __attribute__((packed));

    /**
     * Routing Address String (entry in RIBD above)
     * p. 27, 4.4.3
     */
    struct RoutingAddressString {
        /**
         * 16 bit identifier that is constant for the duration of a
         * session.  String ID zero is predefined as the node initiating
         * the session through sending the Hello SYN message, and String ID
         * one is predefined as the node responding with the Hello SYNACK
         * message.
         */
        u_int16_t string_id;
        u_int8_t length; ///< Length of Address String.
        u_int8_t unused__;
        u_char ra_string[0];
    } __attribute__((packed));

    static const size_t RIBDTLVHeaderSize = sizeof(struct RIBDTLVHeader);

    static const size_t RoutingAddressStringSize =
        sizeof(struct RoutingAddressString);

    /**
     * Constructor.
     */
    RIBDTLV(const Dictionary& ribd);

    /**
     * Destructor
     */
    virtual ~RIBDTLV() {}

    /**
     * Virtual from BaseTLV
     */
    size_t serialize(u_char* bp, size_t len) const;

    /**
     * Accessor
     */
    const Dictionary& ribd(const std::string& sender,
                           const std::string& receiver);

protected:
    friend class TLVFactory<RIBDTLV>;

    /**
     * Constructor.  Protected to force access through TLVFactory.
     */
    RIBDTLV();

    /**
     * Serialize destination ID out to buffer bp, size len, using 
     * struct RoutingAddressString; return total bytes written
     */
    size_t write_ras_entry(u_int16_t sid, const std::string& dest_id,
                           u_char* bp, size_t len) const;

    /**
     * Deserialize RoutingAddressString struct from transport into
     * memory, setting *sid and dest_id; return total bytes read
     */
    size_t read_ras_entry(u_int16_t* sid, std::string& dest_id,
                          const u_char* bp, size_t len);

    /**
     * Virtual from BaseTLV.
     */
    bool deserialize(const u_char* bp, size_t len);

    Dictionary ribd_; ///< mapping from destination id string to 
                      ///  numeric id (sid)
                           
}; // class RIBDTLV

}; // namespace prophet

#endif // _PROPHET_RIBD_TLV_H
