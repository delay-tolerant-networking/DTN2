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
     * The maximum number of iovecs needed to format a bundle.
     */
    static const int MAX_IOVCNT = 6;
    
    /**
     * Fill in an iovec suitable for a call to writev to send the
     * bundle on the "wire".
     *
     * @return false if iovcnt isn't big enough
     */
    static int fill_iov(const Bundle* bundle,
                        struct iovec* iov, int* iovcnt);
    
    /**
     * Free dynamic memory allocated in fill_iov.
     */
    static void free_iovmem(const Bundle* bundle,
                            struct iovec* iov, int iovcnt);

    /**
     * Parse the buffer into a new bundle and pass it to the routing
     * layer for consumption.
     */
    static bool process_buf(u_char* buf, size_t len, Bundle** bundlep = 0);

protected:
    /**
     * The current version of the bundling protocol.
     */
    static const int CURRENT_VERSION = 0x03;

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
        NONE 		= 0x00,
        PRIMARY 	= 0x01,
        DICTIONARY	= 0x02,
        RESERVED 	= 0x03,
        RESERVED2	= 0x04,
        PAYLOAD 	= 0x05,
        RESERVED3	= 0x06,
        AUTHENTICATION 	= 0x07,
        PAYLOAD_SECURITY= 0x08,
        FRAGMENT 	= 0x09,
    } header_type_t;

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
    };

    /**
     * The dictionary header.
     *
     * Note that the records field is variable length.
     */
    struct DictionaryHeader {
        u_int8_t  next_header_type;
        u_int8_t  string_count;
        u_char    records[0];
    };

    /**
     * The bundle payload header.
     *
     * Note that the data field is variable length application data.
     */
    struct PayloadHeader {
        u_int8_t  next_header_type;
        u_int32_t length;
        u_char    data[0];
    };


    static u_int8_t format_cos(const Bundle* bundle);
    static void parse_cos(Bundle* bundle, u_int8_t cos);

    static u_int8_t format_payload_security(const Bundle* bundle);
    static void parse_payload_security(Bundle* bundle,
                                       u_int8_t payload_security);
};

#endif /* _BUNDLE_PROTOCOL_H_ */
