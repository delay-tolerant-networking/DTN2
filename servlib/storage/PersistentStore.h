
#include <vector>
#include <oasys/serialize/Serialize.h>

/**
 * The abstract base class implementing a persistent storage system.
 * Specific implementations (i.e. Berkeley DB or SQL) should derive
 * from this class.
 *
 * TODO:
 *   * should the key be an int or a std::string?
 */
class PersistentStore {
public:
    
    /**
     * Fill in the fields of the object referred to by *obj with the
     * value stored at the given key.
     */
    virtual int get(SerializableObject* obj, const int key) = 0;
    
    /**
     * Store the object with the given key.
     */
    virtual int put(SerializableObject* obj, const int key) = 0;

    /**
     * Delete the object at the given key.
     */
    virtual int del(const int key) = 0;

    /**
     * Return the number of elements in the table.
     */
    virtual int num_elements() = 0;
    
    /**
     * Fill in the given vector with the keys currently stored in the
     * table.
     */
    virtual void keys(std::vector<int> v) = 0;

    /**
     * Fill in the given vector with all the unserialized objects
     * stored in the table.
     */
    virtual void elements(std::vector<SerializableObject*> v) = 0;
};
