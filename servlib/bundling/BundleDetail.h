/*
 *    Copyright 2004-2006 Intel Corporation
 *    Copyright 2011 Trinity College Dublin
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

#ifndef _BUNDLE_DETAIL_H_
#define _BUNDLE_DETAIL_H_

#include <oasys/serialize/Serialize.h>
#include <oasys/storage/StoreDetail.h>

#include "Bundle.h"

// This functionality is only useful with ODBC/SQL  data storage
#ifdef LIBODBC_ENABLED

namespace dtn {

/**
 * The class used to extract a number of fields from a Bundle that can be stored
 * separately in a database table to facilitate various operations, including
 * searching of a list of bundles t speed up forwarding with a large cache
 * and external access to bundles.  The exact fields of the bundle to be stored
 * are controlled by the setup list in the constructor.  The class is derived from
 * Serializable Object in order to allow it to be used in conjunction with the
 * Oasys DurableStore mechanisms, but in practice the data is *not* serialized
 * but is stored separately in columns of the bundle_details table (specified here).
 */



class BundleDetail : public oasys::StoreDetail

{

    public:
	    BundleDetail(Bundle 				   *bndl,
					 oasys::detail_get_put_t	op = oasys::DGP_PUT );
		/**
		 * Dummy constructor used only when doing get_table.
		 */
		BundleDetail(const oasys::Builder&);
	    ~BundleDetail() {}
        //void serialize(oasys::SerializeAction *a)   { (void)a; } ///<Do not use
	    /**
	     * Hook for the generic durable table implementation to know what
	     * the key is for the database.
	     */
	    u_int32_t durable_key() { return bundleid_; }


    private:
	    Bundle *		bundle_;			///< Bundle for which these details apply
	    u_int32_t		bundleid_;			///< ID of the above bundle

};


}  /* namespace dtn */

#endif /* defined LIBODBC_ENABLED */

#endif  /* _BUNDle_DETAIL_H_ */
