#ifndef _MARSHAL_SERIALIZE_H_
#define _MARSHAL_SERIALIZE_H_

#include "Serialize.h"

/**
 * Common base class for Marshal and Unmarshal that manages the flat
 * buffer.
 */
class BufferedSerializeAction : public SerializeAction {
public:
    /**
     * The basic action function is the same for marshalling and
     * unmarshalling.
     */
    int action(SerializableObject* object);

    /** 
     * Since BufferedSerializeAction ignores the name field, calling
     * process() on a contained object is the same as just calling the
     * contained object's serialize() method.
     */
    virtual void process(const char* name, SerializableObject* object)
    {
        object->serialize(this);
    }
    
protected:
    /**
     * Constructor
     */
    BufferedSerializeAction(action_t action, u_char* buf, size_t length);

    /**  
     * Get the next R/W length of the buffer.
     *
     * @return R/W buffer of size length or NULL on error
     */
    u_char* next_slice(size_t length);
    
    /** @return buffer */
    u_char* buf() { return error_ ? 0 : buf_; }

    /** @return buffer length */
    size_t length() { return length_; }
    
    /** Rewind to the beginning of the buffer */
    void rewind() { offset_ = 0; }

 private:
    u_char* buf_;		///< Buffer that is un/marshalled
    size_t  length_;		///< Length of the buffer.
    size_t  offset_;
};

/**
 * Marshal is a SerializeAction that flattens an object into a byte
 * stream.
 */
class Marshal : public BufferedSerializeAction {
public:
    // Constructor
    Marshal(u_char* buf, size_t length);

    /**
     * Since the Marshal operation doesn't actually modify the
     * SerializableObject, define a variant of process() that allows a
     * const SerializableObject* as the object parameter.
     */
    int action(const SerializableObject* const_object)
    {
        SerializableObject* object = (SerializableObject*)const_object;
        return BufferedSerializeAction::action(object);
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
 * Unmarshal is a SerializeAction that constructs an object's
 * internals from a flat byte stream.
 */
class Unmarshal : public BufferedSerializeAction {
public:
    // Constructor
    Unmarshal(const u_char* buf, size_t length);

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
    MarshalSize() : SerializeAction(INFO), size_(0)
    {
    }

    /**
     * The virtual action function. Always succeeds.
     */
    int action(SerializableObject* object);
    
    /**
     * Again, we can tolerate a const object as well.
     */
    int action(const SerializableObject* const_object)
    {
        SerializableObject* object = (SerializableObject*)const_object;
        return action(object);
    }
    
    /** @return Measured size */
    size_t size() { return size_; }
    
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

#endif /* _MARSHAL_SERIALIZE_H_ */
