#ifndef _BUNDLE_PROTOCOL_H_
#define _BUNDLE_PROTOCOL_H_

#include <sys/uio.h>

class Bundle;

/**
 * Class used to convert a Bundle to / from the bundle protocol
 * specification for the "on-the-wire" representation.
 */
class BundleProtocol {
public:
    /**
     * The maximum number of iovecs needed to format the bundle header.
     */
    static const int MAX_IOVCNT = 6;
    
    /**
     * Fill in the given iovec with the formatted bundle header.
     *
     * @return the total length of the header or -1 on error
     */
    static size_t format_headers(const Bundle* bundle,
                                 struct iovec* iov, int* iovcnt);
    
    /**
     * Free dynamic memory allocated in format_headers.
     */
    static void free_header_iovmem(const Bundle* bundle,
                                   struct iovec* iov, int iovcnt);

    /**
     * Parse the header data, filling in the bundle.
     *
     * @return the length consumed or -1 on error
     */
    static int parse_headers(Bundle* bundle, u_char* buf, size_t len);

    /**
     * The current version of the bundling protocol.
     */
    static const int CURRENT_VERSION = 0x04;

    /**
     * Valid type codes for bundle headers.
     * 
     * DTN protocols use a chained header format reminiscent of IPv6
     * headers. Each bundle consists of at least a primary bundle
     * header and a dictionary header. Other header types can be
     * chained after the dictionary header to support additional
     * functionality.
     */
    typedef enum {
        HEADER_NONE 		= 0x00,
        HEADER_PRIMARY 		= 0x01,
        HEADER_DICTIONARY	= 0x02,
        HEADER_RESERVED 	= 0x03,
        HEADER_RESERVED2	= 0x04,
        HEADER_PAYLOAD 		= 0x05,
        HEADER_RESERVED3	= 0x06,
        HEADER_AUTHENTICATION 	= 0x07,
        HEADER_PAYLOAD_SECURITY	= 0x08,
        HEADER_FRAGMENT 	= 0x09,
        HEADER_EXT1		= 0x10,
        HEADER_EXT2		= 0x11,
        HEADER_EXT4		= 0x12,
    } bundle_header_type_t;

    /**
     * The primary bundle header.
     */
    struct PrimaryHeader {
        u_int8_t  version;
        u_int8_t  next_header_type;
        u_int8_t  class_of_service;
        u_int8_t  payload_security;
        u_int8_t  dest_id;
        u_int8_t  source_id;
        u_int8_t  replyto_id;
        u_int8_t  custodian_id;
        u_int64_t creation_ts;
        u_int32_t expiration;
    } __attribute__((packed));

    /**
     * The dictionary header.
     *
     * Note that the records field is variable length.
     */
    struct DictionaryHeader {
        u_int8_t  next_header_type;
        u_int8_t  string_count;
        u_char    records[0];
    } __attribute__((packed));

    /**
     * The fragment header.
     */
    struct FragmentHeader {
        u_int8_t  next_header_type;
        u_int32_t orig_length;
        u_int32_t frag_offset;
    } __attribute__((packed));

    /**
     * The bundle payload header.
     *
     * Note that the data field is variable length application data.
     */
    struct PayloadHeader {
        u_int8_t  next_header_type;
        u_int32_t length;
        u_char    data[0];
    } __attribute__((packed));

    /**
     * Valid type codes for administrative payloads.
     */
    typedef enum {
        ADMIN_STATUS_REPORT	= 0x01,
        ADMIN_CUSTODY_SIGNAL 	= 0x02,
        ADMIN_ECHO		= 0x03,
        ADMIN_NULL		= 0x04,
    } admin_payload_type_t;

    /**
     * Valid status flags.
     */
    typedef enum {
        STATUS_RECEIVED		= 0x01,
        STATUS_CUSTODY_ACCEPTED	= 0x02,
        STATUS_FORWARDED	= 0x04,
        STATUS_DELIVERED	= 0x08,
        STATUS_TTL_EXPIRED	= 0x10,
        STATUS_UNAUTHENTIC	= 0x20,
        STATUS_UNUSED		= 0x40,
        STATUS_FRAGMENT		= 0x80,
    } status_report_flag_t;

    /**
     * Structure for a status report payload. Note that the tuple data
     * portion is variable length.
     *
     * See BundleStatusReport.cc for the formatting of status reports.
     */
    struct StatusReport {
        u_int8_t  admin_type;
        u_int8_t  status_flags;
        u_int64_t orig_ts;
        u_int64_t receipt_ts;
        u_int64_t forward_ts;
        u_int64_t delivery_ts;
        u_int64_t delete_ts;
        char tuple_data[0];
    } __attribute__((packed));

protected:
    static u_int8_t format_cos(const Bundle* bundle);
    static void parse_cos(Bundle* bundle, u_int8_t cos);

    static u_int8_t format_payload_security(const Bundle* bundle);
    static void parse_payload_security(Bundle* bundle,
                                       u_int8_t payload_security);
};

#endif /* _BUNDLE_PROTOCOL_H_ */
