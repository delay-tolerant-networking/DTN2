
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

    MarshalSize s;
    if (s.process_object(&b) != 0) {
        logf("/test", LOG_ERR, "error in marshal sizing");
        exit(1);
    }

    size_t sz = s.size();
    logf("/test", LOG_INFO, "marshalled size is %d", sz);

    u_char* buf = (u_char*)malloc(sz);

    Marshal m(buf, sz);
    if (m.process_object(&b) != 0) {
        logf("/test", LOG_ERR, "error in marshalling");
        exit(1);
    }

    Unmarshal u(buf, sz);
    if (u.process_object(&b2) != 0) {
        logf("/test", LOG_ERR, "error in unmarshalling");
        exit(1);
    }
    
    if (b.bundleid_ != b2.bundleid_) {
        logf("/test", LOG_ERR, "error: bundle id mismatch");
        exit(1);
    }

    logf("/test", LOG_INFO, "simple marshal test passed");
}
