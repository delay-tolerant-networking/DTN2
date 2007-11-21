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

#ifndef _PROPHET_BASE_TLV_H_
#define _PROPHET_BASE_TLV_H_

#include <unistd.h>
#include <sys/types.h>


namespace prophet
{

template<class TLV>
struct TLVFactory
{
    /**
     * Factory method for deserializing over-the-wire TLV into
     * in-memory class representation
     */
    static TLV* deserialize(const u_char* bp, size_t len)
    {
        TLV* t = new TLV();
        if (t->deserialize(bp,len))
            return t;
        delete t;
        return NULL;
    }
};

/**
 * The Prophet I-D (March 2006) dictates five bytecodes for router
 * state exchange messages. This implementation introduces a sixth
 * to help differentiate Bundle offers from Bundle responses.
 *
 * Each Prophet router is a state machine. Each encounter with
 * another router requires an exchange of router state. This begins
 * with a synchronization phase (Hello), followed by an exchange
 * of route tables (Information Exchange).
 *
 * BaseTLV is the abstract base class from which each concrete
 * class derives its API.
 */
class BaseTLV
{
public:
    /**
     * Byte codes for TLV types
     */
    typedef enum {
        UNKNOWN_TLV  = 0x00,
        HELLO_TLV    = 0x01,
        ERROR_TLV    = 0x02,
        RIBD_TLV     = 0xA0,
        RIB_TLV      = 0xA1,
        OFFER_TLV    = 0XA2, 
        RESPONSE_TLV = 0XA3,  // not in spec
    } prophet_tlv_t;

    /**
     * Pretty print function for prophet_tlv_t.
     */
    static const char*
    tlv_to_str(prophet_tlv_t tlv)
    {
        switch(tlv) {
            case HELLO_TLV:    return "HELLO_TLV";
            case RIBD_TLV:     return "RIBD_TLV";
            case RIB_TLV:      return "RIB_TLV";
            case OFFER_TLV:    return "OFFER_TLV";
            case RESPONSE_TLV: return "RESPONSE_TLV";
            case ERROR_TLV:
            case UNKNOWN_TLV:
            default:         return "Unknown TLV type";
        }
    }

    /**
     * Destructor
     */
    virtual ~BaseTLV() {}

    /**
     * Write out TLV from in-memory representation into 
     * provided buffer, using no more than len bytes, returning
     * bytes written
     */
    virtual size_t serialize(u_char* bp, size_t len) const = 0;

    ///@{ Accessors
    prophet_tlv_t typecode() const { return typecode_; }
    u_int8_t      flags()    const { return flags_; }
    u_int16_t     length()   const { return length_; }
    ///@}

protected:

    /**
     * Constructor is protected to force use of factory
     */
    BaseTLV(prophet_tlv_t typecode = UNKNOWN_TLV,
            u_int8_t flags = 0, 
            u_int16_t length = 0)
        : typecode_(typecode), flags_(flags), length_(length) {}

    /**
     * Read a TLV in from transport and copy its contents into memory
     */
    virtual bool deserialize(const u_char* bp, size_t len) = 0;

    prophet_tlv_t     typecode_; ///< typecode for this TLV
    u_int8_t          flags_;    ///< TLV-specific flags
    mutable u_int16_t length_;   ///< serialized length of TLV, mutable so it 
                                 ///  can be assigned by serialize() const
}; // class BaseTLV

}; // namespace prophet

#endif // _PROPHET_BASE_TLV_H_
