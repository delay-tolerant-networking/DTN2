#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <list> 
#include <map>
#include <string>

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
#include "lib/Debug.h"

// forward decls
class Command;
class CommandModule;
class CommandError;
class Mutex;

/*!
 * Exception thrown when an option is incorrect.
 */
class CommandError : public std::exception {
public:
    /*! Returns a string describing the error. Virtual from
     *  exception. */
    virtual const char* what() const throw() { return msg_.c_str(); }

    /*!
     * Formatted command error
     */
    CommandError(const char* fmt, ...) PRINTFLIKE(2, 3);

    /*!
     * Pre-formatted command error
     */
    CommandError(const std::string& msg) : msg_(msg) {}

    /*!
     * Destructor signature to match std::exception
     */
    ~CommandError() throw() {}

protected:
    std::string msg_;           //!< Error message
};

/*!
 *
 * Command manager class
 * 
 * Command files are Tcl scripts. When a module registers
 * itself, this creates a new command with the module name.
 *
 */
class Command : public Logger {    
public:
    /*!
     * Return the singleton instance of config. This should only ever
     * be called after Command::init is called to initialize the
     * instance, hence the assertion that instance_ isn't null.
     */
    static Command* instance() {
        ASSERT(instance_ != NULL);
        return instance_;
    }

    /*!
     * Initialize the interpreter instance and register any auto
     * registered command modules.
     */
    static void init() {
        ASSERT(instance_ == NULL);
        instance_ = new Command();
    }

    /*!
     * Read in a configuration file and execute its contents
     * \return 0 if no error, -1 otherwise.
     */ 
    int exec_file(const char* file);

    /*!
     * Parse a single command string.
     *
     * \return 0 if no error, -1 otherwise
     */
    int exec_command(const char* command);

    /*!
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

    /*! 
     * Register the command module.
     */
    void reg(CommandModule* module);

    /*!
     * Schedule the auto-registration of a command module. This _must_
     * be called from a static initializer, before the Command
     * instance is actually created.
     */
    static void auto_reg(CommandModule* module);

    /*!
     * Register a function to be called at exit.
     */
    void reg_atexit(void(*fn)(void*), void* data = 0);

    /// Set the TclResult string. (Copies result into a Tcl
    /// dynamically allocated buffer, so don't have to worry about
    /// memory management)
    void set_result(const char* result, int len);
    
    /// Get the TclResult string.
    const char* get_result();

    /// The list of registered modules
    typedef std::list<CommandModule*> CommandList;

    /// Return the list of registered modules
    const CommandList* modules() { return &modules_; }
    
protected:
    /*!
     * Do all the actual initialization.
     */
    Command();

    /*!
     * Destructor is never called (and issues an assertion).
     */
    ~Command();


    Mutex* lock_;
    Tcl_Interp* interp_;        //!< Tcl interpreter

    CommandList modules_; 	   //!< List of registered modules
    static CommandList* auto_reg_; //!< List of odules to auto-register

    static Command* instance_;   //!< singleton instance
};


/*! 
 * Extend this class to obtain the configuration behavior for a
 * specific module. Register modules with Command::instance()->reg()
 */
class CommandModule : public Logger {
public:
    /*! 
     * Constructor
     *
     *  \param name Name of the module
     */
    CommandModule(const char* name);
    virtual ~CommandModule();
    
    /*! 
     * Override this to parse the list of arguments.
     *
     * \param argc Argument count 
     * \param argv Argument values
     * \param interp Tcl interpreter
     *
     * \throws CommandError when the configuration file option is
     * invalid.
     */
    virtual void exec(int argc, const char** argv, Tcl_Interp* interp)
        throw(CommandError);

    /*!
     * Handler for builtin commands (such as 'set'). Can be overridden
     * if modules don't want this default behavior.
     * \param argc Argument count 
     * \param argv Argument values
     * \param interp Tcl interpreter
     *
     * \throws CommandError when the configuration file option is
     * invalid.
     *
     * \returns 0 If the command was handled, -1 if error (normally
     * just dispatched to the module's exec method).
     */
    virtual int builtin(int argc, const char** argv, Tcl_Interp* interp)
        throw(CommandError);

    /*! 
     * Get the name of the module.
     */
    const char* name() const { return name_; }

protected:
    friend class Command;
    
    const char* name_;          //!< Name of the module.

    /*!
     * Binding types constants
     */
    enum type_t {
        BINDING_INVALID = -1,
        BINDING_INT = 1,
        BINDING_BOOL, 
        BINDING_STRING, 
        BINDING_ADDR
    };
    
    /*! 
     * storage types for bindings 
     */
    struct Binding {
        Binding(type_t type, void* buf, int len) :
            type_(type), buf_(buf), len_(len) {}
        
        /**
         * Add a destructor declaration with no implementation since
         * this is never called.
         */
        ~Binding();

        type_t type_;             //!< Type of the binding
        void  *buf_;              //!< Buffer/pointer that is bound
        int    len_;              //!< length of the buffer (if needed)
    };
    

    /*! 
     * Binding
     *
     * The "set" command is automatically defined for a module. Set is
     * used to change the value of variable which are bound using the
     * bind_* functions defined below.
     * 
     */
    std::map<std::string, Binding*> bindings_;
   
    //! Bind an integer to the set command
    void bind_i(const char* name, int* val, int initval = 0);
    
    //! Bind a boolean to the set command
    void bind_b(const char* name, bool* val, bool initval = 0);
    
    //! Bind a string to the set command
    void bind_s(const char* name, char* str_buf, int len,
                const char* initval = 0);

    //! Bind an ip addr for the set command, allowing the user to pass
    //a hostname and/or a dotted quad style address
    void bind_addr(const char* name, in_addr_t* addrp,
                   in_addr_t initval = INADDR_ANY);

    /// Set the TclResult string. (Copies result into a Tcl
    /// dynamically allocated buffer, so don't have to worry about
    /// memory management)
    void set_result(const char* result, int len)
    {
        Command::instance()->set_result(result, len);
    }

    /// Callback that's issued just after the command is registered.
    /// This allows commands (particularly AutoCommandModule
    /// instances) to do any post-registration activities like binding
    /// vars.
    virtual void at_reg() {} 
};

/*!
 * CommandModule that auto-registers itself.
 */
class AutoCommandModule : public CommandModule {
public:
    AutoCommandModule(const char* name)
        : CommandModule(name)
    {
        Command::auto_reg(this);
    }
};   

#endif
