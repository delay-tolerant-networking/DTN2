
#include "debug/Debug.h"
#include "BundlePayload.h"

BundlePayload::BundlePayload()
{
}

BundlePayload::~BundlePayload()
{
}

void
BundlePayload::serialize(SerializeAction* a)
{
    a->process("data", &data_);
}
