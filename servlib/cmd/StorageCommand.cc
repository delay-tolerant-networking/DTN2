/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */


#include <oasys/storage/StorageConfig.h>

#include "StorageCommand.h"
#include "bundling/BundlePayload.h"

namespace dtn {

StorageCommand::StorageCommand(oasys::StorageConfig* cfg)
    : TclCommand(cfg->cmd_.c_str())
{
    inited_ = false;
    
    bind_s("type",	&cfg->type_, "What storage system to use.");
    bind_b("tidy",	&cfg->tidy_, "Same as the --tidy argument to dtnd.");
    bind_b("init_db",	&cfg->init_, "Same as the --init-db argument to dtnd.");
    bind_i("tidy_wait",	&cfg->tidy_wait_,
           "How long to wait before really doing the tidy operation.");
    bind_s("dbname",	&cfg->dbname_, "The database name.");
    bind_s("dbdir",	&cfg->dbdir_,  "The database directory.");
    bind_s("payloaddir",&BundlePayload::payloaddir_,
        "directory for payloads while in transit");

    bind_i("fs_fd_cache_size", &cfg->fs_fd_cache_size_, 
           "number of open fds to cache");

    bind_b("db_mpool", &cfg->db_mpool_, "use mpool in Berkeley DB");
    bind_b("db_log", &cfg->db_log_, "use logging in Berkeley DB");
    bind_b("db_txn", &cfg->db_txn_, "use transactions in Berkeley DB");
    bind_b("db_sharefile", &cfg->db_sharefile_, "use shared database file");
    bind_i("db_max_tx",   &cfg->db_max_tx_,
           "max # of active transactions in Berkeley DB");
    bind_i("db_max_locks", &cfg->db_max_locks_,
           "max # of active locks in Berkeley DB");
    bind_i("db_max_lockers", &cfg->db_max_lockers_,
           "max # of active locking threads in Berkeley DB");
    bind_i("db_max_lockedobjs", &cfg->db_max_lockedobjs_,
           "max # of active locked objects in Berkeley DB");
    bind_i("db_lockdetect",   &cfg->db_lockdetect_,
           "frequency to check for Berkeley DB deadlocks (zero disables locking)");
}

} // namespace dtn
