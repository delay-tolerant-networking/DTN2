/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DTNServer.h"

#include "bundling/AddressFamily.h"
#include "bundling/BundleForwarder.h"
#include "bundling/InterfaceTable.h"
#include "bundling/ContactManager.h"

#include "cmd/BundleCommand.h"
#include "cmd/InterfaceCommand.h"
#include "cmd/LinkCommand.h"
#include "cmd/ParamCommand.h"
#include "cmd/RegistrationCommand.h"
#include "cmd/RouteCommand.h"
#include "cmd/StorageCommand.h"

#include "conv_layers/ConvergenceLayer.h"

#include "reg/AdminRegistration.h"
#include "reg/RegistrationTable.h"

#include "routing/BundleRouter.h"

#include "storage/StorageConfig.h"
#include "storage/BundleStore.h"
#include "storage/GlobalStore.h"
#include "storage/RegistrationStore.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#if __DB_ENABLED__
#include "storage/BerkeleyDBStore.h"
#endif

#if __SQL_ENABLED__
#include "storage/SQLStore.h"
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
    
    interp->reg(new BundleCommand());
    interp->reg(new InterfaceCommand());
    interp->reg(new LinkCommand());
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
    ContactManager::init();
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

    // Initialize Storage
    StorageConfig* cfg = StorageConfig::instance();

    if (cfg->tidy_) 
    {
        // init is implicit with tidy
        cfg->init_ = true; 

        // remove data directories
        DTNServer::tidy_dir(cfg->payloaddir_.c_str());
#ifdef __DB_ENABLED__
        DTNServer::tidy_dir(cfg->dbdir_.c_str());
#endif
    }

    if (cfg->init_)
    {
        // initialize data directories
        DTNServer::init_dir(cfg->payloaddir_.c_str());
#ifdef __DB_ENABLED__
        DTNServer::init_dir(cfg->dbdir_.c_str());
#endif
    }

    // validation
    DTNServer::validate_dir(cfg->payloaddir_.c_str());
#ifdef __DB_ENABLED__
    DTNServer::validate_dir(cfg->dbdir_.c_str());
#endif


    // initialize the data storage objects
    std::string& storage_type = StorageConfig::instance()->type_;
    GlobalStore* global_store;
    BundleStore* bundle_store;
    RegistrationStore* reg_store;
    
    if (storage_type.compare("berkeleydb") == 0) {
#if __DB_ENABLED__
        BerkeleyDBManager::instance()->init();
        global_store = new GlobalStore(new BerkeleyDBStore("globals"));
        bundle_store = new BundleStore(new BerkeleyDBStore("bundles"));
        reg_store    = new RegistrationStore(new BerkeleyDBStore("registration"));
#else
        goto unimpl;
#endif

    } else if ((storage_type.compare("mysql") == 0) ||
               (storage_type.compare("postgres") == 0))
    {
#if __SQL_ENABLED__
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
        
        global_store = new GlobalStore(new SQLStore("globals", impl));
        bundle_store = new BundleStore(new SQLStore("bundles", impl));
        reg_store    = new RegistrationStore(new SQLStore("registration", impl));

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
    GlobalStore::instance()->load();
    RegistrationTable::instance()->load();
    BundleStore::instance()->load();

    log_debug("/dtnserver", "started dtn server");
}

void DTNServer::init_dir(const char * dirname)
{
    struct stat st;
    int statret;
    
    statret = stat(dirname, &st);
    if (statret == -1 && errno == ENOENT)
    {
        if (mkdir(dirname, 0700) != 0) {
            log_crit("/dtnserver", "can't create directory %s: %s",
                     dirname, strerror(errno));
            exit(1);
        }
    }
    else if (statret == -1)
    {
        log_crit("/dtnserver", "invalid path %s: %s", dirname, strerror(errno));;
        exit(1);
    }
}

void DTNServer::tidy_dir(const char * dirname)
{
    char cmd[256];
    struct stat st;
    
    if (stat(dirname, &st) == 0)
    {
        sprintf(cmd, "/bin/rm -rf %s", dirname);
        log_warn("/dtnserver",
                 "tidy option removing directory '%s'", cmd);
        
        if (system(cmd))
        {
            log_crit("/dtnserver", "error removing directory %s", dirname);
            exit(1);
        }
        
    }
    else if (errno == ENOENT)
    {
        log_debug("/dtnserver", "directory already removed %s", dirname);
    }
    else
    {
        log_crit("/dtnserver", "invalid directory name %s: %s", dirname, strerror(errno));
        exit(1);
    }
}

void DTNServer::validate_dir(const char * dirname)
{
    struct stat st;
    
    if (stat(dirname, &st) == 0 && S_ISDIR(st.st_mode))
    {
        log_debug("/dtnserver", "directory validated: %s", dirname);
    }
    else
    {
        log_crit("/dtnserver",
                 "invalid directory name %s: %s",
                 dirname, strerror(errno));
        exit(1);
    }
}
