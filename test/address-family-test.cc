
#include <oasys/debug/Debug.h>

#include "bundling/AddressFamily.h"
#include "bundling/BundleTuple.h"

#define VALID   true
#define INVALID false

#define MATCH   true
#define NOMATCH false

void
test_parse(bool expect_ok, const char* str )
{
    BundleTuple t(str);

    if (expect_ok) {
        if (t.valid()) {
            log_info("/test", "ok  -- '%s' valid", str);
        } else {
            log_err("/test",  " ERR -- '%s' INVALID expected valid", str);
        }
    } else {
        if (!t.valid()) {
            log_info("/test", "ok  -- '%s' invalid", str);
        } else {
            log_err("/test",  " ERR -- '%s' VALID expected invalid", str);
        }
    }
}

void
test_match(bool expect_match, const char* pattern, const char* tuple)
{
    BundleTuplePattern p(pattern);
    BundleTuple t(tuple);

    ASSERT(p.valid() && t.valid());

    bool match = p.match(t);
    
    if (expect_match) {
        if (match) {
            log_info("/test", "ok  -- '%s' match '%s'", pattern, tuple);
        } else {
            log_err("/test",  " ERR -- '%s' NOMATCH '%s' expected match",
                    pattern, tuple);
        }
    } else {
        if (!match) {
            log_info("/test", "ok  -- '%s' nomatch '%s'", pattern, tuple);
        } else {
            log_err("/test",  " ERR -- '%s' MATCH '%s' expected nomatch",
                    pattern, tuple);
        }
    }
}

int
main(int argc, const char** argv)
{
    Log::init(1, LOG_INFO);
    AddressFamilyTable::init();

    log_info("/test", "address family test starting...");

    // test a bunch of invalid tuples
    test_parse(INVALID, "");
    test_parse(INVALID, "bundles");
    test_parse(INVALID, "bundles:");
    test_parse(INVALID, "bundles:/");
    test_parse(INVALID, "bundles://");
    test_parse(INVALID, "bundles://region");
    test_parse(INVALID, "bundles://region/");
    test_parse(INVALID, "bundles:///admin");
    test_parse(INVALID, "bundles://region/admin");
    test_parse(INVALID, "bundles://region/admin:");
    test_parse(INVALID, "bundles://region/admin:/");
    test_parse(INVALID, "bundles://region/admin://");
    
    // test internet style parsing
    test_parse(VALID,   "bundles://internet/host://tier.cs.berkeley.edu/demux");
    test_parse(VALID,   "bundles://internet/host://host/demux");
    test_parse(VALID,   "bundles://internet/host://host:port/demux");

    // test fixed style
    test_parse(VALID,   "bundles://fixed/1");
    test_parse(VALID,   "bundles://fixed/12");
    test_parse(INVALID, "bundles://fixed/123");
    
    // test internet style matching
    test_match(MATCH,
               "bundles://internet/host://*",
               "bundles://internet/host://tier.cs.berkeley.edu/demux");

    test_match(MATCH,
               "bundles://internet/ip://128.32.0.0/16",
               "bundles://internet/ip://128.32.100.56:5000/demux");
    
    test_match(MATCH,
               "bundles://internet/ip://128.32.0.0/255.255.0.0",
               "bundles://internet/ip://128.32.100.56:5000/demux");

    // test fixed matching
    test_match(MATCH,
               "bundles://fixed/1",
               "bundles://fixed/1");

    test_match(MATCH,
               "bundles://fixed/12",
               "bundles://fixed/12");

    test_match(NOMATCH,
               "bundles://fixed/1",
               "bundles://fixed/11");

    test_match(MATCH,
               "bundles://fixed/\a",
               "bundles://fixed/\a");

    test_match(MATCH,
               "bundles://fixed/\n",
               "bundles://fixed/\n");

    // test wildcard matching
    test_match(MATCH,
               "bundles://*/*:*",
               "bundles://internet/host://tier.cs.berkeley.edu/demux");

    test_match(MATCH,
               "bundles://internet/*:*",
               "bundles://internet/host://tier.cs.berkeley.edu/demux");

    test_match(NOMATCH,
               "bundles://*/*",
               "bundles://internet/host://tier.cs.berkeley.edu/demux");

    test_match(MATCH,
               "bundles://*/*:*",
               "bundles://fixed/1");

    test_match(MATCH,
               "bundles://fixed/*:*",
               "bundles://fixed/1");

    char buf[128];
    sprintf(buf, "bundles://fixed/%c", 42);
    test_match(MATCH, "bundles://*/*", buf);

    //test_parse(INVALID,   "bundles://internet/host://:port/demux");

}
