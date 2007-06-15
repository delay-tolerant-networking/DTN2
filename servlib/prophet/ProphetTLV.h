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

#ifndef _PROPHET_TLV_H_
#define _PROPHET_TLV_H_

#include <string>
#include <list>
#include "BaseTLV.h"

namespace prophet {

class ProphetTLV 
{
public:
    typedef std::list<BaseTLV*> List;
    typedef std::list<BaseTLV*>::iterator iterator;
    typedef std::list<BaseTLV*>::const_iterator const_iterator;

    /**
     * Header Definition
     * p. 21, 4.2
     */
    struct ProphetHeader {
        u_int8_t version; ///< This version of the PRoPHET Protocol = 1.
        u_int8_t flags; ///< TBD
        /**
         * Field  that is used to indicate whether a response is required
         * to the request message if the outcome is successful.  A value of
         * "NoSuccessAck" indicates that the request message does not
         * expect a response if the outcome is successful, and a value of
         * "AckAll" indicates that a response is expected if the outcome is
         * successful.  In both cases a failure response MUST be generated
         * if the request fails.<br><br>
         * In a response message, the result field can have two values:
         * "Success," and "Failure".  The "Success" results indicates a
         * success response.  All messages that belong to the same success
         * response will have the same Transaction Identifier.  The
         * "Success" result indicates a success response that may be
         * contained in a single message or the final message of a success
         * response spanning multiple messages.<br><br>
         * ReturnReceipt is a result field used to indicate that an
         * acknowledgement is required for the message.  The default for
         * Messages is that the controller will not acknowledge responses.
         * In the case where an acknowledgement is required, it will set
         * the Result Field to ReturnReceipt in the header of the Message.
         * <br><br>
         * The encoding of the result field is:<br>
         * <br>
         *            NoSuccessAck:       Result = 1<br>
         *            AckAll:             Result = 2<br>
         *            Success:            Result = 3<br>
         *            Failure:            Result = 4<br>
         *            ReturnReceipt       Result = 5<br>
         * <br>
         */
        u_int8_t result;
        /**
         * Field gives further information concerning the result in a
         * response message.  It is mostly used to pass an error code in a
         * failure response but can also be used to give further
         * information in a success response message or an event message.
         * In a request message, the code field is not used and is set to
         * zero.<br><br>
         * If the Code field indicates that the Error TLV is included in
         * the message, further information on the error will be found in
         * the Error TLV, which MUST be the the first TLV after the header.
         * <br><br>
         * The encoding is:<br>
         * <br>
         *     PRoPHET Error messages       0x000 - 0x099<br>
         *     Reserved                     0x0A0 - 0x0FE<br>
         *     Error TLV in message             0x0FF<br>
         * 
         */
        u_int8_t code;
        /**
         * For messages during the Hello phase with the Hello SYN, Hello
         * SYNACK, and Hello ACK functions, it is the sender's instance
         * number for the link.  It is used to detect when the link comes
         * back up after going down or when the identity of the entity at
         * the other end of the link changes.  The instance number is a 16-
         * bit number that is guaranteed to be unique within the recent
         * past and to change when the link or node comes back up after
         * going down.  Zero is not a valid instance number.  For the
         * RSTACK function, the Sender Instance field is set to the value
         * of the Receiver Instance field from the incoming message that
         * caused the RSTACK function to be generated.  Messages sent after
         * the Hello phase is completed should use the sender's instance
         * number for the link.
         */
        u_int16_t sender_instance;
        /**
         * For messages during the Hello phase with the Hello SYN, Hello
         * SYNACK, and Hello ACK functions, is what the sender believes is
         * the current instance number for the link, allocated by the
         * entity at the far end of the link.  If the sender of the message
         * does not know the current instance number at the far end of the
         * link, this field SHOULD be set to zero.  For the RSTACK message,
         * the Receiver Instance field is set to the value of the Sender
         * Instance field from the incoming message that caused the RSTACK
         * message to be generated.  Messages sent after the Hello phase is
         * completed should use what the sender believes is the current
         * instance number for the link, allocated by the entity at the far
         * end of the link.
         */
        u_int16_t receiver_instance;
        /**
         * Used to associate a message with its response message.  This
         * should be set in request messages to a value that is unique for
         * the sending host within the recent past.  Reply messages contain
         * the Transaction Indentifier of the request they are responding to.
         */
        u_int32_t transaction_id;
        /**
         * If S is set then the SubMessage Number field indicates the total
         * number of SubMessage segments that compose the entire message.
         * If it is not set then the SubMessage  Number field indicates the
         * sequence number of this SubMessage segment within the whole
         * message. the S field will only be set in the first sub-message
         * of a sequence.
         */
        u_int16_t submessage_flag:1;
        /**
         * When a message is segmented because it exceeds the MTU of the
         * link layer, each segment will include a submessage number to
         * indicate its position.  Alternatively, if it is the first
         * submessage in a sequence of submessages, the S flag will be set
         * and this field will contain the total count of submessage
         * segments.
         */
        u_int16_t submessage_num:15;
        /**
         * Length in octets of this message including headers and message
         * body.  If the message is fragmented, this field contains the
         * length of this submessage.
         */
        u_int16_t length;
    } __attribute__((packed));

    /**
     * Legal values for ProphetHeader.result field
     * p. 22, 4.2
     */
    typedef enum {
        UnknownResult = 0x0,
        NoSuccessAck  = 0x1,
        AckAll        = 0x2,
        Success       = 0x3,
        Failure       = 0x4,
        ReturnReceipt = 0x5
    } header_result_t;

    static const size_t ProphetHeaderSize = sizeof(struct ProphetHeader);

    /**
     * Pretty print function for header_result_t.
     */
    static const char* result_to_str(header_result_t hr)
    {
        switch(hr) {
            case NoSuccessAck:  return "NoSuccessAck";
            case AckAll:        return "AckAll";
            case Success:       return "Success";
            case Failure:       return "Failure";
            case ReturnReceipt: return "ReturnReceipt";
            case UnknownResult:
            default:            return "Unknown header result";
        }
    }

    /**
     * Constructor
     */
    ProphetTLV(const std::string& src,
               const std::string& dst,
               header_result_t result,
               u_int16_t local_instance,
               u_int16_t remote_instance,
               u_int32_t tid);

    /**
     * Copy constructor
     */
    ProphetTLV(const ProphetTLV& tlv);

    /**
     * Destructor
     */
    virtual ~ProphetTLV();

    /**
     * Write ProphetTLV out to no more than len bytes of buffer; return
     * bytes written
     */
    size_t serialize(u_char* bp, size_t len) const;

    /**
     * Read ProphetTLV in from no more than len bytes of buffer; return
     * bytes read
     */
    static ProphetTLV* deserialize(const std::string& src,
                                   const std::string& dst,
                                   const u_char* bp, size_t len);

    /**
     * Place TLV on list for serialization into next outbound ProphetTLV.
     * ProphetTLV assumes ownership of memory on submitted pointer,
     * on success.
     */
    bool add_tlv(BaseTLV* tlv);

    /**
     * Remove next TLV from list. Caller assumes ownership of memory
     * pointed to by returned pointer (if non-NULL).
     */
    BaseTLV* get_tlv();

    ///@{ Accessors
    const std::string& source()         const { return src_; }
    const std::string& destination()    const { return dst_; }
    header_result_t result()            const { return result_; }
    u_int16_t       sender_instance()   const { return sender_instance_; }
    u_int16_t       receiver_instance() const { return receiver_instance_; }
    u_int32_t       transaction_id()    const { return tid_; }
    u_int16_t       length()            const { return length_; }
    size_t          size()              const { return list_.size(); }
    const List&     list()              const { return list_; }
    ///@}

protected:
    ProphetTLV();

    std::string src_; ///< destination id for TLV source (from Bundle metadata)
    std::string dst_; ///< destination id for TLV destination (from Bundle ")
    header_result_t result_; ///< Disposition of this Prophet TLV 
    u_int16_t sender_instance_; ///< Local node's index for this encounter
    u_int16_t receiver_instance_; ///< Peer's index for this encounter
    u_int32_t tid_; ///< Transaction ID for this TLV
    mutable u_int16_t length_; ///< Serialized length of this TLV
    List list_; ///< Linked list of TLVs embedded in this Prophet TLV

}; // class ProphetTLV

}; // namespace prophet
#endif //  _PROPHET_TLV_H_
