
#include "debug/Log.h"

int
main(int argc, const char** argv)
{
    Log::init();
    logf("/daemon", LOG_INFO, "bundle daemon initializing...");
}
