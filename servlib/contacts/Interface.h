#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include "BundleTuple.h"
#include "Contact.h"
#include "debug/Log.h"

class ConvergenceLayer;
class InterfaceInfo;

/**
 * Abstraction of a local dtn interface.
 *
 * Generally, interfaces are created by the console.
 */
class Interface {
public:
    /**
     * Add a new interface.
     */
    static bool add_interface(const char* tuplestr,
                              int argc, const char* argv[]);
    
    /**
     * Remove the specified interface.
     */
    static bool del_interface(const char* tuple);
    
    // Accessors
    const std::string& region() const { return tuple_.region(); }
    const std::string& admin()  const { return tuple_.admin(); }
    const BundleTuple* tuple()  const { return &tuple_; }
    ConvergenceLayer*  clayer() const { return clayer_; }
    InterfaceInfo*     info()   const { return info_; }

    /**
     * Store the ConvergenceLayer specific state.
     */
    void set_info(InterfaceInfo* info) { info_ = info; }

protected:
    Interface(const BundleTuple& tuple,
              ConvergenceLayer* clayer);
    ~Interface();

    static Interface* if_list_;		///< list of all interfaces
    Interface* next_;			///< linkage for all interfaces
    
    BundleTuple tuple_;			///< Local tuple of the interface
    ConvergenceLayer* clayer_;		///< Convergence layer to use
    InterfaceInfo* info_;		///< Convergence layer specific state
};

/**
 * Abstract base class for convergence layer specific portions of an
 * interface.
 */
class InterfaceInfo {
public:
    virtual ~InterfaceInfo() {}
};

#endif /* _INTERFACE_H_ */
