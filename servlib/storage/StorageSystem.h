
#include "BundleStore.h"
#include "RegistrationStore.h"
//#include "RouteTableStore.h"

/**
 * This is the root containment object for the different persistent
 * storage managers in the DTN system. It really just serves as a
 * static hook to scope the singleton instances of the different
 * storage layers.
 */
class StorageSystem {
public:
    static BundleStore* 	bundle_store_;
    static RegistrationStore*	registration_store_;
    //static RouteTableStore*	route_table_store_;
};
