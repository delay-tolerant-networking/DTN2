#ifndef _FILE_CONVERGENCE_LAYER_H_
#define _FILE_CONVERGENCE_LAYER_H_

#include "ConvergenceLayer.h"
#include "thread/Thread.h"

class FileConvergenceLayer : public ConvergenceLayer {
public:
    /**
     * Constructor.
     */
    FileConvergenceLayer();
    
    /**
     * The meat of the initialization happens here.
     */
    virtual void init();

    /**
     * Hook for shutdown.
     */
    virtual void fini();

    /**
     * Hook to validate the admin part of a bundle tuple.
     *
     * @return true if the admin string is valid
     */
    virtual bool validate(const std::string& admin);

    /**
     * Hook to match the admin part of a bundle tuple.
     *
     * @return true if the demux matches the tuple.
     */
    virtual bool match(const std::string& demux, const std::string& admin);

    /**
     * Register a new interface.
     */
    virtual bool add_interface(Interface* iface, int argc, const char* argv[]);

    /**
     * Remove an interface
     */
    virtual bool del_interface(Interface* iface);
    
    /**
     * Open the connection to the given contact and prepare for
     * bundles to be transmitted.
     */
    virtual bool open_contact(Contact* contact);
    
    /**
     * Close the connnection to the contact.
     */
    virtual bool close_contact(Contact* contact);
    
    /**
     * Try to send the bundles queued up for the given contact. In
     * some cases (e.g. tcp) this is a no-op because open_contact spun
     * a thread which is blocked on the bundle list associated with
     * the contact. In others (e.g. file) there is no thread, so this
     * callback is used to send the bundle.
     */
    virtual void send_bundles(Contact* contact);

protected:
    /**
     * Pull a filesystem directory out of the admin portion of a
     * tuple.
     */
    bool extract_dir(const BundleTuple& tuple, std::string* dirp);
    
    /**
     * Validate that a given directory exists and that the permissions
     * are correct.
     */
    bool validate_dir(const std::string& dir);
        
    /**
     * Helper class (and thread) that periodically scans a directory
     * for new bundle files.
     */
    class Scanner : public InterfaceInfo, public Logger, public Thread {
    public:
        /**
         * Constructor.
         */
        Scanner(int secs_per_scan, const std::string& dir);

    protected:
        /**
         * Main thread function.
         */
        void run();
        
        int secs_per_scan_;	///< scan interval
        std::string dir_;	///< directory to scan for bundles.
    };
};

#endif /* _FILE_CONVERGENCE_LAYER_H_ */
