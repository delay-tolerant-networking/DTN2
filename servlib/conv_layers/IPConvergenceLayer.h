#ifndef _IP_CONVERGENCE_LAYER_H_
#define _IP_CONVERGENCE_LAYER_H_

#include "ConvergenceLayer.h"

/**
 * Base class for the TCP and UDP convergence layers.
 */
class IPConvergenceLayer : public ConvergenceLayer {
public:
    /**
     * Constructor.
     */
    IPConvergenceLayer(const char* logpath)
        : ConvergenceLayer(logpath)
    {
    }
    
    /**
     * Hook to validate the admin part of a bundle tuple.
     *
     * @return true if the admin string is valid
     */
    virtual bool validate(const std::string& admin);

    /**
     * Hook to match the admin part of a bundle tuple.
     *
     * @return true if the demux matches the tuple.
     */
    virtual bool match(const std::string& demux, const std::string& admin);

    /**
     * Current version of the TCP / UDP framing protocol.
     */
    static const int CURRENT_VERSION = 0x01;
    
    /**
     * Values for ContactHeader flags.
     */
    typedef enum {
        BUNDLE_ACK_ENABLED = 0x1,	///< enable bundle acking
        BLOCK_ACK_ENABLED = 0x2,	///< enable block acking (tcp only)
        KEEPALIVE_ENABLED = 0x3,	///< enable keepalive handshake
        CONN_IS_RECEIVER = 0x4,		///< connection initiator is rcvr
    } contact_header_flags_t;
   
    /**
     * Contact parameter header. Sent once in each direction for TCP,
     * and at the start of each bundle for UDP.
     */
    struct ContactHeader {
        u_int8_t  version;		///< framing protocol version
        u_int8_t  flags;		///< connection flags (see above)
        u_int8_t  ackblock_sz;		///< size of ack blocks (power of two)
        u_int8_t  keepalive_sec;	///< seconds between keepalive packets
	u_int32_t magic;		///< magic word
    };

    /**
     * Valid type codes for the framing headers.
     *
     * For TCP, the one byte code is always sent first, followed by
     * the per-type header. For UDP, the one byte type code is sent
     * with three bytes of padding to preserve the word alignment of
     * the subsequent frame header.
     */
    typedef enum {
        BUNDLE_START	= 0x1,		///< first block of bundle data
        BUNDLE_BLOCK	= 0x2,		///< subsequent block of bundle data
        BUNDLE_ACK	= 0x3,		///< bundle acknowledgement
        KEEPALIVE	= 0x4,		///< keepalive packet
    } frame_header_type_t;
    
    /**
     * Header for the start of a block of bundle data. In UDP mode,
     * this always precedes a full bundle (or a fragment at the 
     * bundling layer).
     */
    struct BundleStartHeader {
        u_int32_t bundle_id;		///< bundle identifier at sender
        u_int32_t bundle_length;	///< length of the bundle + headers
        u_int32_t block_length;		///< amount of bundle data that follows
                                        ///< in the current frame
        u_int16_t header_length;	///< length of the bundle header
        u_int8_t partial;		///< 1 if this is not the whole bundle
        u_int8_t pad;			///< unused
    } __attribute__((packed));

    /**
     * Header for an internal block of bundle data (TCP only).
     */
    struct BundleBlockHeader {
        u_int32_t block_length;		///< length of the following block
    } __attribute__((packed));
    
    /**
     * Header for a bundle acknowledgement.
     */
    struct BundleAckHeader {
        u_int32_t bundle_id;		///< identical to BundleStartHeader
        u_int32_t acked_length;		///< total length received
    } __attribute__((packed));
};

#endif /* _IP_CONVERGENCE_LAYER_H_ */
