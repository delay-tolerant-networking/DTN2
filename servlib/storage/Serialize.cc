
#include "Serialize.h"

SerializeAction::SerializeAction(action_t type)
    : type_(type), error_(false)
{

}

SerializeAction::~SerializeAction()
{
}
