#ifndef __SERIALIZE_H__
#define __SERIALIZE_H__

#include <string>

/**
 * This file defines the core set of objects that define the
 * Serialization layer.
 */

class SerializableObject;
class SerializeAction;

/**
 * Operation codes, one for each type of SerializeAction
 */
typedef enum {
    MARSHAL = 1, MARSHAL_SIZE, UNMARSHAL
} serializeaction_t;

/**
 * Inherit from this class to add serialization capability to the
 * class.
 */
class SerializableObject {
protected:
    friend class SerializeAction;
    
    /**
     * This should call v->process() on each of the types that are to
     * be serialized in the object.
     */
    virtual void serialize(SerializeAction* a, const serializeaction_t ctx) = 0;
};


/**
 * The SerializeAction is responsible for implementing callback
 * functions for all the basic types. The action object is then passed
 * to the serialize() function which will re-dispatch to the basic
 * type functions for each of the SerializableObject's member fields.
 *
 * INVARIANT: A single SerializeAction must be able to be called on
 * several different visitee objects in succession. (Basically this
 * ability is used to be able to string several Marshallable objects
 * together, either for writing or reading).
 */
class SerializeAction {
public:
    /**
     * Perform the serializing process.
     *
     * @return 0 if success, -1 on error
     */
    int process(SerializableObject* user);
    
    /**
     * Processor functions, one for each type.
     */

    /**
     * Process function for a 4 byte integer.
     */
    virtual void process(const char* name, u_int32_t* i) = 0;

    /**
     * Process function for a 2 byte integer.
     */
    virtual void process(const char* name, u_int16_t* i) = 0;

    /**
     * Process function for a byte.
     */
    virtual void process(const char* name, u_int8_t* i) = 0;

    /**
     * Process function for a boolean.
     */
    virtual void process(const char* name, bool* b) = 0;

    /**
     * Process function for a constant length char buffer.
     */
    virtual void process(const char* name, u_char* bp, size_t len) = 0;

    /**
     * Process function for a variable length char buffer. The
     * alloc_copy flag dictates whether the unmarshal variants should
     * malloc() a new buffer in the target object.
     */
    virtual void process(const char* name, u_char** bp,
                         size_t* len, bool alloc_copy) = 0;

    /**
     * Process function for a c++ string.
     */
    virtual void process(const char* name, std::string* s) = 0;

    /**
     * Adaptor functions for signed/unsigned compatibility
     */
    void process(const char* name, int32_t* i)
    {
        process(name, (u_int32_t*)i);
    }
    
    void process(const char* name, int16_t* i)
    {
        process(name, (u_int16_t*)i);
    }

    void process(const char* name, int8_t* i)
    {
        process(name, (u_int8_t*)i);
    }

    void process(const char* name, char* bp, size_t len)
    {
        process(name, (char*)bp, len);
    }
    
    /** @return buffer */
    u_char* buf() { return error_ ? 0 : buf_; }

    /** @return buffer length */
    virtual size_t length() { return length_; }
    
    /** @return Whether there was an error in marshalling */
    bool error() { return error_; }
    
    /** Rewind to the beginning of the buffer */
    void rewind() { offset_ = 0; }
    
    /** Set the log target */
    void logpath(const char* log) { log_ = log; }
    
    /**
     * Destructor.
     */
    virtual ~SerializeAction();

protected:
    /**
     * Create a SerializeAction with the specified buffer.
     * \param action Action code for the marshal context
     * \param buf Pointer to the buffer to marshal/unmarshal from.
     * \param length Length of the buffer.
     */
    SerializeAction(serializeaction_t action, u_char* buf, size_t length);

    /**  
     * Get the next R/W length of the buffer.
     * @return R/W buffer of size length or NULL on error
     */
    u_char* next_slice(size_t length);
    
    const char* log_;	///< Optional log for verbose marshalling
    
private:
    SerializeAction();
    
    serializeaction_t action_;	///< Serialization action code
    u_char* buf_;		///< Buffer that is un/marshalled
    size_t  length_;		///< Length of the buffer.

    size_t offset_;
    bool  error_;
};

/**
 * Marshaller is a SerializeAction that flattens an object into a byte
 * sequence.
 */
class Marshal : public SerializeAction {
public:
    // Constructor
    Marshal(u_char* buf, size_t length)
        : SerializeAction(MARSHAL, buf, length)
    {
    }

    /**
     * Since the Marshal operation doesn't actually modify the
     * SerializableObject, define a variant of process() that allows a
     * const SerializableObject* as the object parameter.
     */
    int process(const SerializableObject* object)
    {
        return SerializeAction::process((SerializableObject*)object);
    }

    // Virtual functions inherited from SerializeAction
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, size_t len);
    void process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy);
    void process(const char* name, std::string* s);
};

/**
 * MarshalSize is a SerializeAction that determines the buffer size
 * needed to run a Marshal action over the object.
 */
class MarshalSize : public SerializeAction {
public:
    // Constructor
    MarshalSize() : SerializeAction(MARSHAL_SIZE, 0, 0), size_(0)
    {
    }

    /**
     * Since the MarshalSize operation doesn't actually modify the
     * SerializableObject, define a variant of process() that allows a
     * const SerializableObject* as the object parameter.
     */
    int process(const SerializableObject* object)
    {
        return SerializeAction::process((SerializableObject*)object);
    }
    
    //! Return Measured size. Virtual from SerializeAction.
    size_t length() { return size_; }
    
    // Virtual functions inherited from SerializeAction
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, size_t len);
    void process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy);
    void process(const char* name, std::string* s);

private:
    size_t size_;
};

/**
 * Unmarshal is a SerializeAction that constructs an object's
 * internals from a flattened byte stream.
 */
class Unmarshal : public SerializeAction {
public:
    // Constructor
    Unmarshal(const u_char* buf, size_t length)
        : SerializeAction(UNMARSHAL, (u_char*)(buf), length)
    {
    }

    // Virtual functions inherited from SerializeAction
    void process(const char* name, u_int32_t* i);
    void process(const char* name, u_int16_t* i);
    void process(const char* name, u_int8_t* i);
    void process(const char* name, bool* b);
    void process(const char* name, u_char* bp, size_t len);
    void process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy);
    void process(const char* name, std::string* s); 
};

#endif //__SERIALIZE_H__
