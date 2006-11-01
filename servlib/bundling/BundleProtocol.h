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

#ifndef _BUNDLE_PROTOCOL_H_
#define _BUNDLE_PROTOCOL_H_

#include <sys/types.h>

namespace dtn {

class BlockInfo;
class BlockProcessor;
class Bundle;
class BundleTimestamp;
class EndpointID;

/**
 * Centralized class used to convert a Bundle to / from the bundle
 * protocol specification for the "on-the-wire" representation.
 *
 * The actual implementation of this is mostly encapsulated in the
 * BlockProcessor class hierarchy.
 */
class BundleProtocol {
public:
    //----------------------------------------------------------------------
    //
    // XXX: NEW INTERFACE TO THE BUNDLE PROTOCOL CLASS
    //

    /**
     * Register a new BlockProcessor handler to handle the given block
     * type code when received off the wire.
     */
    static void register_processor(BlockProcessor* bp);

    /**
     * Find the appropriate BlockProcessor for the given block type
     * code.
     */
    static BlockProcessor* find_processor(u_int8_t type);

    /**
     * Initialize the default set of block processors.
     */
    static void init_default_processors();

    /**
     * Parse the supplied chunk of arriving data and append it to the
     * rcvd_blocks_ list in the given bundle, finding the appropriate
     * BlockProcessor element and calling its receive() handler.
     *
     * When called repeatedly for arriving chunks of data, this
     * properly fills in the entire bundle, including the in_blocks_
     * record of the arriving blocks and the payload (which is stored
     * externally).
     *
     * @return the length of data consumed or -1 on protocol error,
     * plus sets *last to true if the bundle is complete.
     */
    static int consume(Bundle* bundle, u_char* data, size_t len, bool* last);

private:
    /**
     * Array of registered BlockProcessor handlers -- fixed size since
     * there can be at most one handler per protocol type
     */
    static BlockProcessor* processors_[256];
    
    //
    //----------------------------------------------------------------------
public:

    /**
     * Fill in the given buffer with the formatted bundle blocks that
     * should go before the payload data, including the payload block
     * header (i.e. typecode, flags, and length) but none of the
     * payload data.
     *
     * @return the total length of the formatted blocks or -1 on error
     * (i.e. too small of a buffer)
     */
    static int format_header_blocks(const Bundle* bundle,
                                    u_char* buf, size_t len);
    
    /**
     * Parse the received blocks, filling in the bundle with the
     * relevant data if successful. This succeeds iff the buffer
     * contains a payload block header, and returns the length of all
     * blocks up to the first byte of payload data.
     *
     * Note that this implementation doesn't support sizes bigger than
     * 4GB for the headers.
     *
     * @return the length of the header blocks or -1 on error (i.e.
     * not enough data in buffer)
     */
    static int parse_header_blocks(Bundle* bundle,
                                   u_char* buf, size_t len);

    /**
     * Fill in the given buffer with the formatted bundle blocks that
     * should go after the payload data.
     *
     * @return the total length of the formatted blocks or -1 on error
     * (i.e. too small of a buffer)
     */
    static int format_tail_blocks(const Bundle* bundle,
                                  u_char* buf, size_t len)
    {
        (void)bundle;
        (void)buf;
        (void)len;
            
        return 0; // no trailing blocks implemented yet
    }
    
    /**
     * Parse the blocks received after the payload data, filling in
     * the bundle with the relevant data if successful.
     *
     * @return the length of the header blocks or -1 on error (i.e.
     * not enough data in buffer)
     */
    static int parse_tail_blocks(Bundle* bundle,
                                 u_char* buf, size_t len)
    {
        (void)bundle;
        (void)buf;
        (void)len;
        return 0;
    }
    
    /**
     * Return the length of the header blocks.
     */
    static size_t header_block_length(const Bundle* bundle);
    
    /**
     * Return the length of the tail blocks.
     */
    static size_t tail_block_length(const Bundle* bundle)
    {
        (void)bundle;
        return 0;
    }
    
    /**
     * Return the length of the on-the-wire bundle protocol format for
     * the whole bundle, i.e. 
     */
    static size_t formatted_length(const Bundle* bundle);
    
    /**
     * Format the whole bundle, stuffing it in the supplied buffer.
     *
     * @return the total length of the formatted bundle or -1 on error
     * (i.e. too small of a buffer)
     */
    static int format_bundle(const Bundle* bundle, u_char* buf, size_t len);

    /**
     * Parse a whole bundle.
     *
     * @return the total length of the parsed bundle or -1 on error
     * (i.e. too small of a buffer)
     */
    static int parse_bundle(Bundle* bundle, u_char* buf, size_t len);

    /**
     * Store a DTN timestamp into a 64-bit value suitable for
     * transmission over the network.
     */
    static void set_timestamp(u_char* bp, const BundleTimestamp* tv);

    /**
     * Store a DTN timestamp into a 64-bit value suitable for
     * transmission over the network. The implementation doesn't
     * require the 64-bit destination to be word-aligned so it is safe
     * to cast here.
     */
    static void set_timestamp(u_int64_t* bp, const BundleTimestamp* tv) {
        set_timestamp((u_char *) bp, tv);
    }

    /**
     * Retrieve a DTN timestamp from a 64-bit value that was
     * transmitted over the network. This does not require the
     * timestamp to be word-aligned.
     */
    static void get_timestamp(BundleTimestamp* tv, const u_char* bp);

    /**
     * Retrieve a DTN timestamp from a 64-bit value that was
     * transmitted over the network. This does not require the
     * timestamp to be word-aligned.
     */
    static void get_timestamp(BundleTimestamp* tv, const u_int64_t* bp) {
        get_timestamp(tv, (u_char *) bp);
    }

    /**
     * The current version of the bundling protocol.
     */
    static const int CURRENT_VERSION = 0x04;

    /**
     * Valid type codes for bundle blocks.
     */
    typedef enum {
        PRIMARY_BLOCK          = 0x00,	///< INTERNAL USE ONLY -- NOT IN SPEC
        PAYLOAD_BLOCK          = 0x01, 
        PREVIOUS_HOP_BLOCK     = 0x05
    } bundle_block_type_t;

    /**
     * Values for block processing flags that appear in all blocks
     * except the primary block.
     */
    typedef enum {
        BLOCK_FLAG_REPLICATE		= 1 << 0,
        BLOCK_FLAG_REPORT_ONERROR	= 1 << 1,
        BLOCK_FLAG_DISCARD_ONERROR	= 1 << 2,
        BLOCK_FLAG_LAST_BLOCK		= 1 << 3,
    } block_flag_t;

    /**
     * The basic block preamble that's common to all blocks
     * (including the payload block but not the primary block).
     */
    struct BlockPreamble {
        u_int8_t type;
        u_int8_t flags;
        u_char   length[0]; // SDNV
    } __attribute__((packed));

    /**
     * Administrative Record Type Codes
     */
    typedef enum {
        ADMIN_STATUS_REPORT     = 0x01,
        ADMIN_CUSTODY_SIGNAL    = 0x02,
        ADMIN_ANNOUNCE          = 0x05,   // NOT IN BUNDLE SPEC
    } admin_record_type_t;

    /**
     * Administrative Record Flags.
     */
    typedef enum {
        ADMIN_IS_FRAGMENT       = 0x01,
    } admin_record_flags_t;

    /**
     * Bundle Status Report Status Flags
     */
    typedef enum {
        STATUS_RECEIVED         = 0x01,
        STATUS_CUSTODY_ACCEPTED = 0x02,
        STATUS_FORWARDED        = 0x04,
        STATUS_DELIVERED        = 0x08,
        STATUS_DELETED		= 0x10,
        STATUS_ACKED_BY_APP	= 0x20,
        STATUS_UNUSED		= 0x40,
        STATUS_UNUSED2		= 0x80,
    } status_report_flag_t;

    /**
     * Bundle Status Report "Reason Code" flags
     */
    typedef enum {
	REASON_NO_ADDTL_INFO              = 0x00,
	REASON_LIFETIME_EXPIRED           = 0x01,
	REASON_FORWARDED_UNIDIR_LINK      = 0x02,
	REASON_TRANSMISSION_CANCELLED     = 0x03,
	REASON_DEPLETED_STORAGE           = 0x04,
	REASON_ENDPOINT_ID_UNINTELLIGIBLE = 0x05,
	REASON_NO_ROUTE_TO_DEST           = 0x06,
	REASON_NO_TIMELY_CONTACT          = 0x07,
	REASON_BLOCK_UNINTELLIGIBLE       = 0x08,
    } status_report_reason_t;

    /**
     * Custody Signal Reason Codes
     */
    typedef enum {
        CUSTODY_NO_ADDTL_INFO	           = 0x00,
        CUSTODY_REDUNDANT_RECEPTION        = 0x03,
        CUSTODY_DEPLETED_STORAGE           = 0x04,
        CUSTODY_ENDPOINT_ID_UNINTELLIGIBLE = 0x05,
        CUSTODY_NO_ROUTE_TO_DEST           = 0x06,
        CUSTODY_NO_TIMELY_CONTACT          = 0x07,
        CUSTODY_BLOCK_UNINTELLIGIBLE	   = 0x08
    } custody_signal_reason_t;

    /**
     * Assuming the given bundle is an administrative bundle, extract
     * the admin bundle type code from the bundle's payload.
     *
     * @return true if successful
     */
    static bool get_admin_type(const Bundle* bundle,
                               admin_record_type_t* type);
};

} // namespace dtn

#endif /* _BUNDLE_PROTOCOL_H_ */
