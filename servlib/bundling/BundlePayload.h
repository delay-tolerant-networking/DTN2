#ifndef _BUNDLE_PAYLOAD_H_
#define _BUNDLE_PAYLOAD_H_

#include <string>
#include "serialize/Serialize.h"
#include "debug/Debug.h"
#include "io/FileIOClient.h"

/**
 * The representation of a bundle payload.
 *
 * This is abstracted into a separate class to allow the daemon to
 * separately manage the serialization of header information from the
 * payload.
 *
 */
class BundlePayload : public SerializableObject {
public:
    BundlePayload();
    virtual ~BundlePayload();
    
    /**
     * Options for payload location state.
     */
    typedef enum {
        MEMORY = 1,		/// copy of the payload kept in memory
        DISK = 2,		/// payload only kept on disk
        UNDETERMINED = 3,	/// determine MEMORY or DISK based on threshold
        NODATA = 4,		/// no data storage at all (used for simulator)
    } location_t;
    
    /**
     * Actual payload initialization function.
     */
    void init(SpinLock* lock, int bundleid, location_t location);
  
    /**
     * Set the payload length in preparation for filling in with data.
     * Optionally also force-sets the location, or leaves it based on
     * configured parameters.
     */
    void set_length(size_t len, location_t location = UNDETERMINED);

    /**
     * Truncate the payload. Used for reactive fragmentation.
     */
    void truncate(size_t len);
    
    /**
     * The payload length.
     */
    size_t length() const { return length_; }

    /**
     * The payload location.
     */
    location_t location() const { return location_; }
    
    /**
     * Set the payload data and length, closing the payload file after
     * it's been written to.
     */
    void set_data(const char* bp, size_t len);

    /**
     * Set the payload data, closing the payload file after it's been
     * written to.
     */
    void set_data(const std::string& data)
    {
        set_data(data.data(), data.length());
    }

    /**
     * Append a chunk of payload data. Assumes that the length was
     * previously set. Keeps the payload file open.
     */
    void append_data(const char* bp, size_t len);

    /**
     * Write a chunk of payload data at the specified offset. Keeps
     * the payload file open.
     */
    void write_data(const char* bp, size_t offset, size_t len);

    /**
     * Writes len bytes of payload data from from another payload at
     * the given src_offset to the given dst_offset. Keeps the payload
     * file open.
     */
    void write_data(BundlePayload* src, size_t src_offset,
                    size_t len, size_t dst_offset);

    /**
     * Reopen the payload file.
     */
    void reopen_file();
    
    /**
     * Close the payload file.
     */
    void close_file();

    /**
     * Get a pointer to the in-memory data buffer.
     */
    char* memory_data()
    {
        ASSERT(location_ == MEMORY);
        return (char*)data_.c_str();
    }
    
    /**
     * Return a pointer to a chunk of payload data. For in-memory
     * bundles, this will just be a pointer to the data buffer.
     * Otherwise, it will call read() into the supplied buffer (which
     * must be >= len).
     */
    const char* read_data(size_t offset, size_t len, char* buf,
                          bool keep_file_open = false);

    /**
     * Virtual from SerializableObject
     */
    virtual void serialize(SerializeAction* a);

    /*
     * Tunable parameters
     */
    static std::string dir_;	///< directory for payload files
    static size_t mem_threshold_; ///< maximum bundle size to keep in memory
    static bool test_no_remove_;  ///< test: don't rm payload files

protected:
    void internal_write(const char* bp, size_t offset, size_t len);

    location_t location_;	///< location of the data (disk or memory)
    std::string data_;		///< the actual payload data if in memory
    size_t length_;     	///< the payload length
    size_t rcvd_length_;     	///< the payload length we actually have
    std::string fname_;		///< payload file name
    FileIOClient* file_;	///< file handle if on disk
    size_t cur_offset_;		///< cache of current fd position
    size_t base_offset_;	///< for fragments, offset into the file (todo)
    SpinLock* lock_;		///< the lock for the given bundle
};

#endif /* _BUNDLE_PAYLOAD_H_ */
