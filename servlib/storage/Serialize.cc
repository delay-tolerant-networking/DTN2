

#include "Serialize.h"

#include "iostream"
using namespace std;

SerializeAction::SerializeAction(action_t type)
    : type_(type), error_(false), log_(0)
{

}

SerializeAction::~SerializeAction()
{
}

/**
 * Call the virtual serialize() callback which will, in turn, call the
 * various process() callbacks on ourself. If any of the process
 * functions fails due to insufficient buffer space, it will set
 * error_ to true.
 */
int
SerializeAction::action(SerializableObject* object)
{
    error_ = false;

    object->serialize(this);
    
    if (error_ == true)
        return -1;
    
    return 0;
}

