
#include "GlobalStore.h"


GlobalStore* GlobalStore::instance_;

GlobalStore::GlobalStore()
    : Logger("/storage/globals")
{
}

GlobalStore::~GlobalStore()
{
}

void
GlobalStore::serialize(SerializeAction* a)
{
    a->process("next_bundleid", &next_bundleid_);
    a->process("next_regid",    &next_regid_);
}


