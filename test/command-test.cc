
#include <stdio.h>
#include <errno.h>
#include "cmd/Command.h"

using namespace std;

class TestModule : public CommandModule {
public:
    TestModule() : CommandModule("test") {
        bind_i("val", &val_);
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
    
    int  val_;
    std::string str_;
};

int
main(int argc, char* argv[])
{
    Log::init(LOG_INFO);
    
    CommandInterp::init();
    
    CommandModule* mod = new CommandModule("mod1");
    TestModule*   test = new TestModule();

    CommandInterp::instance()->reg(mod);
    CommandInterp::instance()->reg(test);


    while (1) {
        char buf[256];
        fprintf(stdout, "> ");
        fflush(stdout);
        
        char* line = fgets(buf, sizeof(buf), stdin);
        if (!line) {
            logf("/test", LOG_INFO, "got eof on stdin, exiting: %s", strerror(errno));
            continue;
        }
        
        line[strlen(line) - 1] = '\0';
        logf("/test", LOG_DEBUG, "got line '%s'", line);
        
        if (CommandInterp::instance()->exec_command(buf) != TCL_OK) {
            fprintf(stdout, "error: ");
        }
        
        const char* res = CommandInterp::instance()->get_result();
        if (res && res[0] != '\0') {
            fprintf(stdout, "%s\r\n", res);
            fflush(stdout);
        }
    }
}

