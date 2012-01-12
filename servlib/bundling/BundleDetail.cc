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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Logger.h>
#include <oasys/serialize/Serialize.h>
#include <oasys/storage/StoreDetail.h>

#include "Bundle.h"
#include "BundleDetail.h"

// This functionality is only useful with ODBC/SQL  data storage
#ifdef LIBODBC_ENABLED

namespace dtn {

//----------------------------------------------------------------------
BundleDetail::BundleDetail(Bundle				   *bndl,
						   oasys::detail_get_put_t	op ):
		StoreDetail("BundleDetail", "/dtn/bundle/bundle_detail", op),
		bundle_(bndl)
{
	bundleid_ = bndl->bundleid();
	add_detail("bundle_id",			oasys::DK_LONG,
			   (void *)&bundleid_,	sizeof(int32_t));
	add_detail("source_eid",		oasys::DK_VARCHAR,
			   (void *)const_cast<char *>(bndl->source().c_str()),
			   bndl->source().length());
	add_detail("dest_eid",			oasys::DK_VARCHAR,
			   (void *)const_cast<char *>(bndl->dest().c_str()),
			   bndl->dest().length());
	if (bndl->payload().location() == BundlePayload::DISK) {
		add_detail("payload_file",	oasys::DK_VARCHAR,
				   (void *)const_cast<char *>(bndl->payload().filename().c_str()),
				   bndl->payload().filename().length());
	} else {
		add_detail("payload_file",	oasys::DK_VARCHAR,		(void *)"in_memory", 9);
	}
}
//----------------------------------------------------------------------
BundleDetail::BundleDetail(const oasys::Builder&) :    
		StoreDetail("BundleDetail", "/dtn/bundle/bundle_detail", oasys::DGP_PUT),
		bundle_(NULL)
{
	bundleid_ = 0xFFFFFFFF;
}
//----------------------------------------------------------------------
}  /* namespace dtn */

#endif /* defined LIBODBC_ENABLED */
