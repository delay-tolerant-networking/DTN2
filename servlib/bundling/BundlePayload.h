#ifndef _BUNDLE_PAYLOAD_H_
#define _BUNDLE_PAYLOAD_H_

#include <string>
#include "storage/Serialize.h"
#include "debug/Debug.h"

/**
 * The representation of a bundle payload.
 *
 * This is abstracted into a separate class to allow the daemon to
 * separately manage the serialization of header information from the
 * payload.
 */
class BundlePayload : public SerializableObject {
public:
    BundlePayload();
    virtual ~BundlePayload();

    /**
     * Set the payload data.
     */
    void set_data(const std::string& data)
    {
        data_.assign(data);
    }
    
    /**
     * Set the payload data and length.
     */
    void set_data(const char* bp, size_t len)
    {
        data_.assign(bp, len);
        length_ = len;
    }

    /**
     * Append a chunk of payload data. Assumes that the length was
     * previously set.
     */
    void append_data(const char* bp, size_t len)
    {
        ASSERT((data_.length() + len) <= length_);
        data_.append(bp, len);
    }

    /**
     * Set the payload length in preparation for filling in with data.
     */
    void set_length(size_t len)
    {
        length_ = len;
    }
    
    /**
     * The payload length.
     */
    size_t length() const { return length_; }

    /**
     * The actual payload data.
     */
    const char* data() const { return data_.data(); }

    /**
     * Accessor to the underlying data string itself.
     */
    std::string& mutable_data() { return data_; }
     
    /**
     * Virtual from SerializableObject
     */
    virtual void serialize(SerializeAction* a);

protected:
    std::string data_;	///< the actual payload data
    size_t length_;     ///< the payload length, potentially not all filled in
};

#endif /* _BUNDLE_PAYLOAD_H_ */
