#ifndef _BUNDLE_PAYLOAD_H_
#define _BUNDLE_PAYLOAD_H_

#include <string>
#include "storage/Serialize.h"
#include "debug/Debug.h"
#include "io/FileIOClient.h"

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
     * Options for payload location state.
     */
    typedef enum {
        MEMORY = 1,
        DISK = 2
    } location_t;
    
    /**
     * Actual payload initialization function.
     * init_disk() or init_memory() is called depending upon
     * location
     */
     void init(int bundleid, location_t location);

     
     
    /**
     * Actual initialization function that creates a disk based 
     * bundle.
     */
     void init_disk(int bundleid);

    /**
     * Actual initialization function that creates a memory based 
     * bundle.
     */
    void init_memory(int bundleid);

  
    /**
     * Set the payload length in preparation for filling in with data.
     * Based on the configured threshold, this will also set the
     * location flag.
     */
    void set_length(size_t len);
    
    /**
     * The payload length.
     */
    size_t length() const { return length_; }

    /**
     * Set the payload data and length.
     */
    void set_data(const char* bp, size_t len);

    /**
     * Set the payload data.
     */
    void set_data(const std::string& data)
    {
        set_data(data.data(), data.length());
    }
    
    /**
     * Append a chunk of payload data. Assumes that the length was
     * previously set.
     */
    void append_data(const char* bp, size_t len);

    /**
     * Return a pointer to a chunk of payload data.
     */
    const char* read_data(off_t offset, size_t len);

    /**
     * Virtual from SerializableObject
     */
    virtual void serialize(SerializeAction* a);

    /*
     * Tunable parameters
     */
    static std::string dir_;	///< directory for payload files
    static size_t mem_threshold_; ///< maximum bundle size to keep in memory

protected:
    location_t location_;	///< location of the data (disk or memory)
    std::string data_;		///< the actual payload data if in memory
    size_t length_;     	///< the payload length
    std::string fname_;		///< payload file name
    FileIOClient* file_;	///< file handle if on disk
    off_t offset_;		///< cache of current fd position
};

#endif /* _BUNDLE_PAYLOAD_H_ */
