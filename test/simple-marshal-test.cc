
#include <stdio.h>
#include "debug/Log.h"
#include "bundling/Bundle.h"
#include "storage/MarshalSerialize.h"

int
main(int argc, const char** argv)
{
    Log::init();
    
    Bundle b, b2;
    b.bundleid_ = 100;
    ASSERT(!b.source_.valid());
    ASSERT(!b2.source_.valid());
    
    b.source_.set_tuple("bundles://internet/tcp://foo");
    ASSERT(b.source_.valid());
    ASSERT(b.source_.region().compare("internet") == 0);
    ASSERT(b.source_.admin().compare("tcp://foo") == 0);

    MarshalSize s;
    if (s.action(&b) != 0) {
        logf("/test", LOG_ERR, "error in marshal sizing");
        exit(1);
    }

    size_t sz = s.size();
    logf("/test", LOG_INFO, "marshalled size is %d", sz);

    u_char* buf = (u_char*)malloc(sz);

    Marshal m(buf, sz);
    if (m.action(&b) != 0) {
        logf("/test", LOG_ERR, "error in marshalling");
        exit(1);
    }

    Unmarshal u(buf, sz);
    if (u.action(&b2) != 0) {
        logf("/test", LOG_ERR, "error in unmarshalling");
        exit(1);
    }
    
    if (b.bundleid_ != b2.bundleid_) {
        logf("/test", LOG_ERR, "error: bundle id mismatch");
        exit(1);
    }

    if (!b2.source_.valid()) {
        logf("/test", LOG_ERR, "error: b2 source not valid");
        exit(1);
    }


#define COMPARE(x)                                                      \
    if (b.source_.x().compare(b2.source_.x()) != 0) {                   \
         logf("/test", LOG_ERR, "error: mismatch of %s: %s != %s",      \
              #x, b.source_.x().c_str(), b2.source_.x().c_str());       \
    }

    COMPARE(tuple);
    COMPARE(region);
    COMPARE(admin);

    logf("/test", LOG_INFO, "simple marshal test passed");
}
