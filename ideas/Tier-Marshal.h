#ifndef __MARSHAL_H__
#define __MARSHAL_H__

#include <string>

/*!
 * \file
 *
 * Future ideas: If we find that we are sending big blobs of data
 * around which we don't want to copy, we can change the packet format
 * from a char* to a PacketBuf object and use an iovec with
 * scatter/gather sending to avoid the copying.
 *
 */

// Predecl
class MarshalUser;
class MarshalVisitor;

/*! Operation codes, one for each type of MarshalVisitor */
typedef enum {
    MARSHAL = 1, UNMARSHAL, MARSHAL_SIZE, MARSHAL_PRINT
} marshalop_t;

/*! Type codes. */
typedef enum {
    MARSHAL_NETWORK = 1, MARSHAL_DISK
} marshaltype_t;

/*! A context is an op and a type */
struct MarshalContext {
    MarshalContext(marshalop_t o, marshaltype_t t) : op(o), type(t) {}
    
    marshalop_t     op;
    marshaltype_t type;
};

/*!
 * Inherit from this class to add marshalling capability to the class.
 */
class MarshalUser {
protected:
    friend class MarshalVisitor;
    
    /*! put objects to serial here */
    virtual void serialize(MarshalVisitor* v, const MarshalContext& ctx) = 0;
};


/*!
 * Visitor functions for each type that can be marshaled. Add more
 * virtual functions to extend for _basic_ types. Only add visitors
 * for basic types, because adding too many types here will make this
 * scheme not very scalable.
 *
 * INVARIANT: A single MarshalVisitor must be able to be called on
 * several different visitee objects in succession. (Basically this
 * ability is used to be able to string several Marshallable objects
 * together, either for writing or reading).
 */
class MarshalVisitor {
public:
    /*!
     * Virtual destructor.
     */
    virtual ~MarshalVisitor();

    /*!
     * Exception thrown when the buffer is too small to contain the
     * data to be marshalled or when there is no more additional data
     * to be read and a read is requested.
     */
    class MarshalError {
    public:
        MarshalError() {}
    };

    /*!
     * Perform the marshalling process.
     *
     * \return 0 if success.
     */
    int process(MarshalUser* user, marshaltype_t type);
    
    /**
     * Visitor functions, one for each type.
     */
    virtual void visit(u_int32_t* i) = 0;		// 4 bytes
    virtual void visit(u_int16_t* i) = 0;		// 2 bytes
    virtual void visit(u_int8_t* i) = 0;		// 1 byte
    virtual void visit(bool* b) = 0;			// boolean
    virtual void visit(u_char* bp, size_t len) = 0; 	// constant length buffer
    virtual void visit(u_char** bp, size_t* len, bool alloc_copy) = 0;
    							// variable length buffer
    virtual void visit(std::string* s) = 0;		// c++ string

    /** Adaptor functions for signed/unsigned compatibility */
    virtual void visit(int32_t* i)		{ visit((u_int32_t*)i); }
    virtual void visit(int16_t* i)		{ visit((u_int16_t*)i); }
    virtual void visit(char* bp, size_t len) 	{ visit((char*)bp, len); }
    
    /*! \return buffer */
    u_char* buf() { return error_ ? 0 : buf_; }

    /*! \return buffer length */
    virtual size_t length() { return length_; }
    
    /*! \return Whether there was an error in marshalling */
    bool error() { return error_; }

    /*! Rewind to the beginning of the buffer */
    void rewind() { offset_ = 0; }

    /*! Set the verbose log type */
    void marshal_log(const char* log) { log_ = log; }

protected:
    /*!
     * Create a MarshalVisitor with the specified buffer.
     * \param op Operation code for the marshal context
     * \param buf Pointer to the buffer to marshal/unmarshal from.
     * \param length Length of the buffer.
     * \param owner If owner is true, the buffer is deleted along with
     * the MarshalVisitor.
     */
    MarshalVisitor(marshalop_t op, u_char* buf, size_t length, bool owner);

    /*!
     * No default constructor please.
     */
    MarshalVisitor();
    
    /*!  
     * Get the next R/W length of the buffer.
     * \throws MarshalError when the requested length is too large.
     * \return R/W buffer of size length.
     */
    u_char* next_slice(size_t length) throw(MarshalError);
    
    const char* log_;	///< Optional log for verbose marshalling
    
private:
    marshalop_t op_;	///< Marshal operation code
    u_char* buf_;	///< Buffer that is un/marshalled
    bool    owner_;	///< True if buf_ is to be deleted with class.
    size_t  length_;	///< Length of the buffer.

    size_t offset_;
    bool  error_;
};

/*!
 * Marshalling visitor for the IA32 Intel platform.
 */
class Marshal : public MarshalVisitor {
public:
    // Constructor
    Marshal(u_char* buf, size_t length, bool owner = false) 
        : MarshalVisitor(MARSHAL, buf, length, owner) {}

    // We can tolerate const users
    int process(const MarshalUser* user, marshaltype_t type)
    {
        return MarshalVisitor::process(const_cast<MarshalUser*>(user), type);
    }

    // Virtual fcns inherited from MarshalVisitor
    void visit(u_int32_t* i)		throw(MarshalError);
    void visit(u_int16_t* i)		throw(MarshalError);
    void visit(u_int8_t* i)		throw(MarshalError);
    void visit(bool* b)			throw(MarshalError);
    void visit(u_char* bp, size_t len)	throw(MarshalError);
    void visit(u_char** bp, size_t* lenp, bool alloc_copy) throw(MarshalError);
    void visit(std::string* s)		throw(MarshalError);
};

/*!
 * Unmarshalling visitor for the IA32 Intel platform.
 */
class Unmarshal : public MarshalVisitor {
public:
    // Constructor
    Unmarshal(const u_char* buf, size_t length, bool owner = false) 
        : MarshalVisitor(UNMARSHAL, const_cast<u_char*>(buf), length, owner) {}

    // Virtual fcns inherited from MarshalVisitor
    void visit(u_int32_t* i)		throw(MarshalError);
    void visit(u_int16_t* i)		throw(MarshalError);
    void visit(u_int8_t* i)		throw(MarshalError);
    void visit(bool* b)			throw(MarshalError);
    void visit(u_char* bp, size_t len)	throw(MarshalError);
    void visit(u_char** bp, size_t *lenp, bool alloc_copy) throw(MarshalError);
    void visit(std::string* s)		throw(MarshalError);
};

class MarshalSize : public MarshalVisitor {
public:
    // Constructor
    MarshalSize() : MarshalVisitor(MARSHAL_SIZE, 0, 0, false), size_(0) { }

    // We can tolerate const users
    int process(const MarshalUser* user, marshaltype_t type)
    {
        return MarshalVisitor::process(const_cast<MarshalUser*>(user), type);
    }

    //! Return Measured size. Virtual from MarshalVisitor.
    size_t length() { return size_; }
    
    // virtual from MarshalVisitor
    void visit(u_int32_t* i)		throw(MarshalError);
    void visit(u_int16_t* i)		throw(MarshalError);
    void visit(u_int8_t* i)		throw(MarshalError);
    void visit(bool* b)			throw(MarshalError);
    void visit(u_char* bp, size_t len)	throw(MarshalError);
    void visit(u_char** bp, size_t *lenp, bool alloc_copy) throw(MarshalError);
    void visit(std::string* s)		throw(MarshalError);

private:
    size_t size_;
};

/*!
 * Pretty Printer class. Used for debugging purposes.
 */
class MarshalPrinter : public MarshalVisitor {
public:
    // Constructor
    MarshalPrinter(u_char* buf, int length, bool owner = false)
        : MarshalVisitor(MARSHAL_PRINT, buf, length, owner) {}

    // We can tolerate const users
    int process(const MarshalUser* user, marshaltype_t type)
    {
        return MarshalVisitor::process(const_cast<MarshalUser*>(user), type);
    }

    // Virtual fcns inherited from MarshalVisitor
    void visit(u_int32_t* i)		throw(MarshalError);
    void visit(u_int16_t* i)		throw(MarshalError);
    void visit(u_int8_t* i)		throw(MarshalError);
    void visit(bool* b)			throw(MarshalError);
    void visit(u_char* bp, size_t len)	throw(MarshalError);
    void visit(u_char** bp, size_t *lenp, bool alloc_copy) throw(MarshalError);
    void visit(std::string* s)		throw(MarshalError);
};

#endif //__MARSHAL_H__
