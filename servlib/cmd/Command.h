#ifndef _COMMAND_H_
#define _COMMAND_H_

#include <list> 
#include <map>
#include <string>

#include <stdarg.h>
#include <tcl.h>

#include <netinet/in.h>

/*
 * In tcl8.4, the argv to commands is const, but not so in tcl8.3, so
 * we need this stupid define, and force cast everything to const in
 * the implementation of tcl_cmd.
 */
#ifdef CONST84
#define CONSTTCL84 CONST84
#else
#define CONSTTCL84
#endif

#include <string>
#include "debug/Debug.h"

// forward decls
class CommandInterp;
class CommandModule;
class CommandError;
class Mutex;

/**
 * A list of CommandModules.
 */
typedef std::list<CommandModule*> CommandList;

/**
 *
 * Command interpreter class
 * 
 * Command files are Tcl scripts. When a module registers itself, the
 * interpreter binds a new command with the module name.
 *
 */
class CommandInterp : public Logger {    
public:
    /**
     * Return the singleton instance of config. This should only ever
     * be called after Command::init is called to initialize the
     * instance, hence the assertion that instance_ isn't null.
     */
    static CommandInterp* instance() {
        ASSERT(instance_ != NULL);
        return instance_;
    }

    /**
     * Initialize the interpreter instance and register any auto
     * registered command modules.
     */
    static void init() {
        ASSERT(instance_ == NULL);
        instance_ = new CommandInterp();
    }

    /**
     * Read in a configuration file and execute its contents
     * \return 0 if no error, -1 otherwise.
     */ 
    int exec_file(const char* file);

    /**
     * Parse a single command string.
     *
     * \return 0 if no error, -1 otherwise
     */
    int exec_command(const char* command);

    /**
     * Static callback function from Tcl to execute the commands.
     *
     * \param client_data Pointer to config module for which this
     *     command was registered.
     * \param interp Tcl interpreter
     * \param argc Argument count.
     * \param argv Argument values.
     */
    static int tcl_cmd(ClientData client_data, Tcl_Interp* interp,
                       int argc, CONSTTCL84 char **argv);

    /** 
     * Register the command module.
     */
    void reg(CommandModule* module);

    /**
     * Schedule the auto-registration of a command module. This _must_
     * be called from a static initializer, before the Command
     * instance is actually created.
     */
    static void auto_reg(CommandModule* module);

    /**
     * Register a function to be called at exit.
     */
    void reg_atexit(void(*fn)(void*), void* data);

    /**
     * Set the TclResult string.
     */
    void set_result(const char* result);

    /**
     * Append the string to the TclResult
     */
    void append_result(const char* result);

    /**
     * Format and set the TclResult string.
     */
    void resultf(const char* fmt, ...) PRINTFLIKE(2, 3);

    /**
     * Format and append the TclResult string.
     */
    void append_resultf(const char* fmt, ...) PRINTFLIKE(2, 3);
    
    /**
     * Format and set the TclResult string.
     */
    void vresultf(const char* fmt, va_list ap, bool append);
    
    /**
     * Get the TclResult string.
     */
    const char* get_result();

    /**
     * Return the list of registered modules
     */
    const CommandList* modules() { return &modules_; }
    
protected:
    /**
     * Do all the actual initialization.
     */
    CommandInterp();

    /**
     * Destructor is never called (and issues an assertion).
     */
    ~CommandInterp();


    Mutex* lock_;			///< Lock for command execution
    Tcl_Interp* interp_;		///< Tcl interpreter

    CommandList modules_;		///< List of registered modules
    static CommandList* auto_reg_;	///< List of odules to auto-register

    static CommandInterp* instance_;	///< Singleton instance
};


/** 
 * Extend this class to provide the command hooks for a specific
 * module. Register modules with Command::instance()->reg() or use the
 * AutoCommandModule class.
 */
class CommandModule : public Logger {
public:
    /** 
     * Constructor
     *
     *  @param name Name of the module
     */
    CommandModule(const char* name);
    virtual ~CommandModule();
    
    /** 
     * Override this to parse the list of arguments.
     *
     * @param argc Argument count 
     * @param argv Argument values
     * @param interp Tcl interpreter
     *
     * @return 0 on success, -1 on error
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);

    virtual char *help_string() {return("No help, sorry.");};

    /**
     * Internal handling of the "set" command.
     *
     * @param argc Argument count 
     * @param argv Argument values
     * @param interp Tcl interpreter
     *
     * @return 0 on success, -1 on error
     */
    virtual int cmd_set(int argc, const char** argv, Tcl_Interp* interp);

    /** 
     * Get the name of the module.
     */
    const char* name() const { return name_; }

protected:
    friend class CommandInterp;
    
    const char* name_;          ///< Name of the module.
    bool do_builtins_;		///< Set to false if a module doesn't want
                                ///< builtin commands like "set"

    /**
     * Binding type constants
     */
    enum type_t {
        BINDING_INVALID = -1,
        BINDING_INT = 1,
        BINDING_BOOL, 
        BINDING_STRING, 
        BINDING_ADDR
    };
    
    /** 
     * storage types for bindings 
     */
    struct Binding {
        Binding(type_t type, void* val)
            : type_(type)
        {
            val_.voidval_ = val;
        }
        
        /**
         * Add a destructor declaration with no implementation since
         * this is never called.
         */
        ~Binding();

        type_t type_;             	///< Type of the binding
        union {
            void*	voidval_;
            int*	intval_;
            bool*	boolval_;
            in_addr_t*	addrval_;
            std::string* stringval_;
        } val_; 			///< Union for value pointer
    };
    
    /** 
     * Binding list.
     *
     * The "set" command is automatically defined for a module. Set is
     * used to change the value of variable which are bound using the
     * bind_* functions defined below.
     * 
     */
    std::map<std::string, Binding*> bindings_;
   
    /**
     * Bind an integer to the set command
     */
    void bind_i(const char* name, int* val, int initval = 0);
    
    /**
     * Bind a boolean to the set command
     */
    void bind_b(const char* name, bool* val, bool initval = false);
    
    /**
     * Bind a string to the set command
     */
    void bind_s(const char* name, std::string* str,
                const char* initval = 0);

    /**
     * Bind an ip addr for the set command, allowing the user to pass
     * a hostname and/or a dotted quad style address
     */
    void bind_addr(const char* name, in_addr_t* addrp,
                   in_addr_t initval = INADDR_ANY);

    /**
     * Set the TclResult string.
     */
    void set_result(const char* result)
    {
        CommandInterp::instance()->set_result(result);
    }

    /**
     * Append the TclResult string.
     */
    void append_result(const char* result)
    {
        CommandInterp::instance()->append_result(result);
    }

    /**
     * Format and set the TclResult string.
     */
    void resultf(const char* fmt, ...) PRINTFLIKE(2, 3);

    /**
     * Format and set the TclResult string.
     */
    void append_resultf(const char* fmt, ...) PRINTFLIKE(2, 3);


    /**
     * Useful function for generating error strings indicating that
     * the wrong number of arguments were passed to the command.
     *
     * @param argc	original argument count to the command
     * @param argv	original argument vector to the command
     * @param parsed	number of args to include in error string
     * @param min	minimum number of expected args
     * @param max	maximum number of expected args (or INT_MAX)
     */
    void wrong_num_args(int argc, const char** argv, int parsed,
                        int min, int max);
    
    /**
     * Callback that's issued just after the command is registered.
     * This allows commands (particularly AutoCommandModule instances)
     * to do any post-registration activities like binding their vars.
     */
    virtual void at_reg() {} 
};

/**
 * CommandModule that auto-registers itself.
 */
class AutoCommandModule : public CommandModule {
public:
    AutoCommandModule(const char* name)
        : CommandModule(name)
    {
        CommandInterp::auto_reg(this);
    }
};   

#endif /* COMMAND_H */
