
#include <stdio.h>
#include <errno.h>
#include "cmd/Command.h"

using namespace std;

class TestModule : public CommandModule {
public:
    TestModule() : CommandModule("test") {
        bind_b("bool", &bool_);
        bind_addr("addr", &addr_);
        bind_i("int", &int_);
        bind_s("str", &str_);
    }

    virtual int exec(int argc, const char** argv, Tcl_Interp* interp) {
        if (argc >= 2 && !strcmp(argv[1], "error")) {
            resultf("error in test::exec: %d", 100);
            return TCL_ERROR;
        }
            
        set_result("test::exec ");
        for (int i = 0; i < argc; ++i) {
            append_result(argv[i]);
            append_result(" ");
        }
        return TCL_OK;
    }

    bool bool_;
    in_addr_t addr_;
    int  int_;
    std::string str_;
};

int
main(int argc, char* argv[])
{
    Log::init(LOG_INFO);
    
    CommandInterp::init("test");
    
    CommandModule* mod = new CommandModule("mod1");
    TestModule*   test = new TestModule();

    CommandInterp::instance()->reg(mod);
    CommandInterp::instance()->reg(test);

    CommandInterp::instance()->loop("test");
}

