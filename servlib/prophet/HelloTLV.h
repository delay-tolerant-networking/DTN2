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

#ifndef _PROPHET_HELLO_TLV_H_
#define _PROPHET_HELLO_TLV_H_

#include <string>
#include "BaseTLV.h"

namespace prophet
{

class HelloTLV : public BaseTLV
{
public:
    /**
     * Hello TLV header<br>
     * p. 25, 4.4.1<br>
     * <br>
     * The Hello TLV is used to set up and maintain a link between two
     * PRoPHET nodes.  Hello messages with the SYN function are transmitted
     * periodically as beacons.  The Hello TLV is the first TLV exchanged
     * between two PRoPHET nodes when they encounter each other.  No other
     * TLVs can be exchanged until the first Hello sequenece is completed.<br>
     * <br>
     * Once a communication link is established between two PRoPHET nodes,
     * the Hello TLV will be sent once for each interval as defined in the
     * interval timer.  If a node experiences the lapse of HELLO_DEAD Hello
     * intervals without receiving a Hello TLV on an ESTAB connection (as
     * defined in the state machine in Section 5.2), the connection SHOULD
     * be assumed broken.
     */
    struct HelloTLVHeader {
        u_int8_t type; ///< defined as 0x01
        u_int8_t unused__:5;
        /**
         * Specifies the function of the Hello TLV.  Four functions are
         * specified for the Hello TLV:
         * <br>
         *       SYN:     HF = 1<br>
         *       SYNACK:  HF = 2<br>
         *       ACK:     HF = 3<br>
         *       RSTACK:  HF = 4.<br>
         */
        u_int8_t HF:3;
        /**
         * Length of the TLV in octets, including the TLV header and any
         * nested TLVs.
         */
        u_int16_t length;
        /**
         * The Timer field is used to inform the receiver of the timer
         * value used in the Hello processing of the sender.  The timer
         * specifies the nominal time between periodic Hello messages.  It
         * is a constant for the duration of a session.  The timer field is
         * specified in units of 100ms.
         */
        u_int8_t timer;
        /**
         * The Name Length field is used to specify the length of the
         * Sender Name field in octets.  If the name has already been sent
         * at least once in a message with the current Sender Instance, a
         * node MAY choose to set this field to zero, omitting the Sender
         * Name from the Hello TLV.
         */
        u_int8_t name_length;
        /**
         * The Sender Name field specifies the routable DTN name of the
         * sender that is to be used in updating routing information and
         * making forwarding decisions.
         */
        u_char sender_name[0];
    } __attribute__((packed));

    static const size_t HelloTLVHeaderSize = sizeof(struct HelloTLVHeader);

    /**
     * Legal values for HelloTLVHeader.HF (hello function)
     * p. 25, 4.4.1
     */
    typedef enum {
        HF_UNKNOWN  = 0x0,
        SYN         = 0x1,
        SYNACK      = 0x2,
        ACK         = 0x3,
        RSTACK      = 0x4
    } hello_hf_t;

    static const char*
    hf_to_str(hello_hf_t hf)
    {
        switch(hf) {
            case SYN:    return "SYN";
            case SYNACK: return "SYNACK";
            case ACK:    return "ACK";
            case RSTACK: return "RSTACK";
            case HF_UNKNOWN:
            default:              return "Unrecognized prophet HF code";
        }
    }

    /**
     * Constructor.
     */
    HelloTLV(hello_hf_t hf,
             u_int8_t timer,
             const std::string& sender);
 
    /**
     * Destructor
     */
    virtual ~HelloTLV() {}

    /**
     * Virtual from BaseTLV.
     */
    size_t serialize(u_char* bp, size_t len) const;

    ///@{ Accessors
    hello_hf_t  hf()     const { return hf_; }
    const char* hf_str() const { return hf_to_str(hf_); }
    u_int8_t    timer()  const { return timer_; }
    std::string sender() const { return sender_; }
    ///@}

protected:
    friend class TLVFactory<HelloTLV>;

    /**
     * Constructor.  Protected to force access through TLVFactory.
     */
    HelloTLV();

    /**
     * Virtual from BaseTLV.
     */
    bool deserialize(const u_char* bp, size_t len);

    hello_hf_t hf_; ///< Hello function bytecode
    u_int8_t   timer_; ///< Units of 100ms between Hello messages
    std::string sender_; ///< endpoint id of sender
}; // class HelloTLV

}; // namespace prophet

#endif // _PROPHET_HELLO_TLV_H_
