#ifndef dtn_bundle_h
#define dtn_bundle_h

#include "storage/Serialize.h"

class Bundle : public SerializableObject {
public:

    int id() { return id_; }

    // virtual from SerializableObject
    void serialize(SerializeAction* a, const serializeaction_t ctx);

protected:
    int id_;
};

#endif /* dtn_bundle_h */
