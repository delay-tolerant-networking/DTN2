
#include "config.h"
#include "DTNServer.h"

#include "bundling/AddressFamily.h"
#include "bundling/BundleForwarder.h"
#include "bundling/InterfaceTable.h"

#include "cmd/APICommand.h"
#include "cmd/BundleCommand.h"
#include "cmd/InterfaceCommand.h"
#include "cmd/ParamCommand.h"
#include "cmd/RegistrationCommand.h"
#include "cmd/RouteCommand.h"
#include "cmd/StorageCommand.h"

#include "conv_layers/ConvergenceLayer.h"

#include "reg/AdminRegistration.h"
#include "reg/RegistrationTable.h"

#include "routing/BundleRouter.h"

#include "storage/StorageConfig.h"

#if __DB_ENABLED__
#include "storage/BerkeleyDBStore.h"
#include "storage/BerkeleyDBBundleStore.h"
#include "storage/BerkeleyDBGlobalStore.h"
#include "storage/BerkeleyDBRegistrationStore.h"
#endif

#if __SQL_ENABLED__
#include "storage/SQLBundleStore.h"
#include "storage/SQLGlobalStore.h"
#include "storage/SQLRegistrationStore.h"
#endif

#if __MYSQL_ENABLED__
#include "storage/MysqlSQLImplementation.h"
#endif

#if __POSTGRES_ENABLED__
#include "storage/PostgresSQLImplementation.h"
#endif

void
DTNServer::init_commands()
{
    TclCommandInterp* interp = TclCommandInterp::instance();
    
    interp->reg(new APICommand());
    interp->reg(new BundleCommand());
    interp->reg(new InterfaceCommand());
    interp->reg(new ParamCommand());
    interp->reg(new RegistrationCommand());
    interp->reg(new RouteCommand());
    interp->reg(new StorageCommand());

    log_debug("/dtnserver", "registered dtn commands");
}

/**
 * Initialize all components before modifying any configuration.
 */
void
DTNServer::init_components()
{
    AddressFamilyTable::init();
    ConvergenceLayer::init_clayers();
    InterfaceTable::init();
    BundleForwarder::init(new BundleForwarder());
    
    log_debug("/dtnserver", "intialized dtn components");
}

/**
 * Post configuration, start up all components.
 */
void
DTNServer::start()
{
    BundleRouter* router;
    router = BundleRouter::create_router(BundleRouter::type_.c_str());

    BundleForwarder* forwarder = BundleForwarder::instance();
    forwarder->set_active_router(router);
    forwarder->start();

    // initialize the data store
    std::string& storage_type = StorageConfig::instance()->type_;
    GlobalStore* global_store;
    BundleStore* bundle_store;
    RegistrationStore* reg_store;
    
    if (storage_type.compare("berkeleydb") == 0) {
#if __DB_ENABLED__
        BerkeleyDBManager::instance()->init();
        global_store = new BerkeleyDBGlobalStore();
        bundle_store = new BerkeleyDBBundleStore();
        reg_store    = new BerkeleyDBRegistrationStore();
#else
        goto unimpl;
#endif

    } else if ((storage_type.compare("mysql") == 0) ||
               (storage_type.compare("postgres") == 0))
    {
#if __SQL_ENABLED__
        StorageConfig* cfg = StorageConfig::instance();
        SQLImplementation* impl = NULL;
        
        if (storage_type.compare("mysql") == 0)
        {
#if __MYSQL_ENABLED__
            impl = new MysqlSQLImplementation();
#else
            goto unimpl;
#endif /* __MYSQL_ENABLED__ */
        }            

        else if (storage_type.compare("postgres") == 0) {
#if __POSTGRES_ENABLED__
            impl = new PostgresSQLImplementation();
#else
            goto unimpl;
#endif /* __POSTGRES_ENABLED__ */
        }

        ASSERT(impl);
        
        if (impl->connect(cfg->sqldb_.c_str()) == -1) {
            log_err("/dtnserver", "error connecting to %s database %s",
                    storage_type.c_str(), cfg->sqldb_.c_str());
            exit(1);
        }
        
        global_store = new SQLGlobalStore(impl);
        bundle_store = new SQLBundleStore(impl);
        reg_store    = new SQLRegistrationStore(impl);

#else  /* __SQL_ENABLED__ */
        goto unimpl;
        
#endif /* __SQL_ENABLED__ */
    }        

    else {
 unimpl:
        log_err("/dtnserver", "storage type %s not implemented, exiting...",
                storage_type.c_str());
        exit(1);
    }
    
    GlobalStore::init(global_store);
    BundleStore::init(bundle_store);
    RegistrationStore::init(reg_store);
    
    // create (and auto-register) the default administrative registration
    RegistrationTable::init(RegistrationStore::instance());
    new AdminRegistration();
    
    // load in the various storage tables
    //GlobalStore::instance()->load();
    //RegistrationTable::instance()->load();
    //BundleStore::instance()->load();

    log_debug("/dtnserver", "started dtn server");
}
