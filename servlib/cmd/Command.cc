#include "Command.h"
#include "debug/Debug.h"
#include "io/NetUtils.h"
#include "thread/Mutex.h"

/******************************************************************************
 *
 * CommandInterp
 *
 *****************************************************************************/
// static variables
CommandInterp* CommandInterp::instance_;
CommandList* CommandInterp::auto_reg_ = NULL;

CommandInterp::CommandInterp()
    : Logger("/command")
{
    interp_ = Tcl_CreateInterp();
    
    lock_ = new Mutex("/command/lock");

    // do auto registration of commands (if any)
    if (auto_reg_) {
        ASSERT(auto_reg_); 
        while (!auto_reg_->empty()) {
            CommandModule* m = auto_reg_->front();
            auto_reg_->pop_front();
            reg(m);
        }
    
        delete auto_reg_;
        auto_reg_ = NULL;
    }
}

CommandInterp::~CommandInterp()
{
    // the destructor isn't ever called
    NOTREACHED;
}

int
CommandInterp::exec_file(const char* file)
{
    int err;
    ScopeLock l(lock_);

    log_debug("executing command file %s", file);
    
    err = Tcl_EvalFile(interp_, (char*)file);
    
    if (err != TCL_OK) {
        logf(LOG_ERR, "error: line %d, %s",
             interp_->errorLine, Tcl_GetStringResult(interp_));
    }
    
    return err;    
}

int
CommandInterp::exec_command(const char* command)
{
    int err;
    ScopeLock l(lock_);

    // tcl modifies the command string while executing it, so we need
    // to make a copy
    char* buf = strdup(command);

    log_debug("executing command '%s'", buf);
    
    err = Tcl_Eval(interp_, buf);
    
    free(buf);
    
    if (err != TCL_OK) {
        logf(LOG_ERR, "tcl error: line %d, \"%s\"",
             interp_->errorLine, interp_->result);
    }
    
    return err;
}

void
CommandInterp::reg(CommandModule *module)
{
    ScopeLock l(lock_);
    
    module->logf(LOG_DEBUG, "command registering");
    
    Tcl_CreateCommand(interp_, 
                      (char*)module->name(),
                      CommandInterp::tcl_cmd,
                      (ClientData)module,
                      NULL);
    
    modules_.push_front(module);

    module->at_reg();
}

void
CommandInterp::auto_reg(CommandModule *module)
{
    // this should only be called from the static initializers, i.e.
    // we haven't been initialized yet
    ASSERT(instance_ == NULL);

    // we need to explicitly create the auto_reg list the first time
    // since there's no guarantee of ordering of static constructors
    if (!auto_reg_)
        auto_reg_ = new CommandList();
    
    auto_reg_->push_back(module);
}

void
CommandInterp::reg_atexit(void(*fn)(void*), void* data)
{
    ScopeLock l(lock_);
    Tcl_CreateExitHandler(fn, data);
}
    
int 
CommandInterp::tcl_cmd(ClientData client_data, Tcl_Interp* interp,
                       int argc, CONSTTCL84 char** argv)
{
    CommandModule* module = (CommandModule*)client_data;

    const char** const_argv = (const char**)argv;

    // first check for builtin commands
    if (module->do_builtins_ && argc > 2) {
        const char* cmd = argv[1];
        if (strcmp(cmd, "set") == 0) {
            return module->cmd_set(argc, const_argv, interp);
        }
    }

    return module->exec(argc, (const char**)argv, interp);
}

void
CommandInterp::set_result(const char* result)
{
    Tcl_SetResult(interp_, (char*)result, TCL_VOLATILE);
}

void
CommandInterp::append_result(const char* result)
{
    Tcl_AppendResult(interp_, (char*)result, NULL);
}

void
CommandInterp::vresultf(const char* fmt, va_list ap, bool append)
{
    size_t sz = 256;
    char buf[sz];
    
    size_t n = vsnprintf(buf, sz, fmt, ap);
    
    if (n >= sz) {
        NOTIMPLEMENTED; // XXX/demmer handle this
    }

    if (append) {
        Tcl_AppendResult(interp_, buf, NULL);
    } else {
        Tcl_SetResult(interp_, buf, TCL_VOLATILE);
    }
}

void
CommandInterp::resultf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vresultf(fmt, ap, false);
    va_end(ap);
}

void
CommandInterp::append_resultf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vresultf(fmt, ap, true);
    va_end(ap);
}

const char*
CommandInterp::get_result()
{
    return Tcl_GetStringResult(interp_);
}

/******************************************************************************
 *
 * CommandModule
 *
 *****************************************************************************/
CommandModule::CommandModule(const char* name)
    : name_(name), do_builtins_(true)
{
    logpathf("/command/%s", name);

    // Don't do registration here. This makes the code overly obscure
    // b/c it is not obvious who own the module that is created.
}

CommandModule::~CommandModule()
{
}

int
CommandModule::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    resultf("command %s not implemented", argv[0]);
    return TCL_ERROR;
}

void
CommandModule::resultf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    CommandInterp::instance()->vresultf(fmt, ap, false);
    va_end(ap);
}

void
CommandModule::append_resultf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    CommandInterp::instance()->vresultf(fmt, ap, true);
    va_end(ap);
}

void
CommandModule::wrong_num_args(int argc, const char** argv, int parsed,
                              int min, int max)
{
    set_result("wrong number of arguments to '");
    append_result(argv[0]);
    
    for (int i = 1; i < parsed; ++i) {
        append_result(" ");
        append_result(argv[i]);
    }
    append_result("'");
    
    if (max != -1) {
        append_resultf(" expected %d - %d, got %d", min, max, argc);
    } else {
        append_resultf(" expected %d, got %d", min, argc);
    }
}

int
CommandModule::cmd_set(int argc, const char** args, Tcl_Interp* interp)
{
    ASSERT(argc >= 2);
    ASSERT(strcmp(args[1], "set") == 0);
    
    // handle "set binding [value]" command
    if (argc < 3 || argc > 4) {
        resultf("wrong number of args: expected 3-4, got %d", argc);
        return TCL_ERROR;
    }

    const char* var = args[2];
    const char* val = (argc == 4) ? args[3] : 0;
  
    std::map<std::string, Binding*>::iterator itr;
    itr = bindings_.find(var);
    
    if (itr == bindings_.end()) {
        resultf("set: binding for %s does not exist", var);
        return TCL_ERROR;
    }
    
    // set value (if any)
    Binding* b = (*itr).second;

    if (argc == 4) 
    {
        switch(b->type_) 
        {
        case BINDING_INT:
            *(b->val_.intval_) = atoi(val);
            break;
            
        case BINDING_BOOL:
            if (strcasecmp(val, "t")    == 0 ||
                strcasecmp(val, "true") == 0 ||
                strcasecmp(val, "1")    == 0)
            {
                *(b->val_.boolval_) = true;
            }
            else if (strcasecmp(val, "f")     == 0 ||
                     strcasecmp(val, "false") == 0 ||
                     strcasecmp(val, "0")     == 0)
            {
                *(b->val_.boolval_) = false;
            }
            else
            {
                resultf("set: invalid value '%s' for boolean", val);
                return TCL_ERROR;
            }
            break;
            
        case BINDING_ADDR:
        {
            if (gethostbyname(val, b->val_.addrval_) != 0) {
                resultf("set: invalid value '%s' for addr", val);
                return TCL_ERROR;
            }
            break;

        }    
        case BINDING_STRING:
            b->val_.stringval_->assign(val);
            break;
            
        default:
            logf(LOG_CRIT, "unimplemented binding type %d", b->type_);
            ASSERT(0);
        }
    }

    switch(b->type_) 
    {
    case BINDING_INT:
        resultf("%d", *(b->val_.intval_));
        break;

    case BINDING_BOOL:
        if (*(b->val_.boolval_))
            set_result("true");
        else
            set_result("false");
        break;
        
    case BINDING_ADDR:
        resultf("%s", intoa(*(b->val_.addrval_)));
        break;
        
    case BINDING_STRING:
        set_result(b->val_.stringval_->c_str());
        break;
        
    default:
        logf(LOG_CRIT, "unimplemented binding type %d", b->type_);
        ASSERT(0);
    }
    
    return 0;
}

// boilerplate code
#define BIND_FUNCTION(_fn, _type, _typecode)                            \
void                                                                    \
_fn(const char* name, _type* val, _type initval)                        \
{                                                                       \
    *val = initval;                                                     \
    if (bindings_.find(name) != bindings_.end())                        \
    {                                                                   \
        log_warn("warning, binding for %s already exists", name);       \
    }                                                                   \
                                                                        \
    log_debug("creating %s binding for %s -> %p", #_type, name, val);   \
                                                                        \
    bindings_[name] = new Binding(_typecode, val);                      \
}

BIND_FUNCTION(CommandModule::bind_i, int, BINDING_INT);
BIND_FUNCTION(CommandModule::bind_b, bool, BINDING_BOOL);
BIND_FUNCTION(CommandModule::bind_addr, in_addr_t, BINDING_ADDR);

void
CommandModule::bind_s(const char* name, std::string* val,
                      const char* initval)
{
    if (initval)
        val->assign(initval);
    
    if (bindings_.find(name) != bindings_.end()) {
        log_warn("warning, binding for %s already exists", name);
    }

    log_debug("creating string binding for %s -> %p", name, val);

    bindings_[name] = new Binding(BINDING_STRING, val);
}
