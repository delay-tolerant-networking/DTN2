#ifndef _BUNDLE_PROTOCOL_H_
#define _BUNDLE_PROTOCOL_H_

#include <sys/uio.h>

class Bundle;

/**
 * Class used to convert a Bundle to / from the "on-the-wire"
 * representation.
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

protected:
    /**
     * The current version of the bundling protocol.
     */
    static const int CURRENT_VERSION = 0x04;

    /**
     * Enum to enumerate the valid type codes for headers.
     * 
     * DTN protocols use a chained header format reminiscent of IPv6
     * headers. Each bundle consists of at least a primary bundle
     * header and a dictionary header. Other header types can be
     * chained after the dictionary header to support additional
     * functionality.
     */
    typedef enum {
        NONE 			= 0x00,
        PRIMARY 		= 0x01,
        DICTIONARY		= 0x02,
        RESERVED 		= 0x03,
        RESERVED2		= 0x04,
        PAYLOAD 		= 0x05,
        RESERVED3		= 0x06,
        AUTHENTICATION 		= 0x07,
        PAYLOAD_SECURITY	= 0x08,
        FRAGMENT 		= 0x09,
        EXT1			= 0x10,
        EXT2			= 0x11,
        EXT4			= 0x12,
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


    static u_int8_t format_cos(const Bundle* bundle);
    static void parse_cos(Bundle* bundle, u_int8_t cos);

    static u_int8_t format_payload_security(const Bundle* bundle);
    static void parse_payload_security(Bundle* bundle,
                                       u_int8_t payload_security);
};

#endif /* _BUNDLE_PROTOCOL_H_ */
