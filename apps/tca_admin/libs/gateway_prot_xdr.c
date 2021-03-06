/*
 *    Copyright 2006 Intel Corporation
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

/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "gateway_prot.h"

bool_t
xdr_bamboo_stat (XDR *xdrs, bamboo_stat *objp)
{
// LIANG
//	register int32_t *buf;

	 if (!xdr_enum (xdrs, (enum_t *) objp))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_bamboo_key (XDR *xdrs, bamboo_key objp)
{
// LIANG
//	register int32_t *buf;

	 if (!xdr_opaque (xdrs, objp, 20))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_bamboo_value (XDR *xdrs, bamboo_value *objp)
{
// LIANG
//	register int32_t *buf;

	 if (!xdr_bytes (xdrs, (char **)&objp->bamboo_value_val, (u_int *) &objp->bamboo_value_len, 1024))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_bamboo_placemark (XDR *xdrs, bamboo_placemark *objp)
{
// LIANG
//	register int32_t *buf;

	 if (!xdr_bytes (xdrs, (char **)&objp->bamboo_placemark_val, (u_int *) &objp->bamboo_placemark_len, 100))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_bamboo_put_args (XDR *xdrs, bamboo_put_args *objp)
{
// LIANG
//	register int32_t *buf;

	 if (!xdr_string (xdrs, &objp->application, 255))
		 return FALSE;
	 if (!xdr_string (xdrs, &objp->client_library, 255))
		 return FALSE;
	 if (!xdr_bamboo_key (xdrs, objp->key))
		 return FALSE;
	 if (!xdr_bamboo_value (xdrs, &objp->value))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->ttl_sec))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_bamboo_get_args (XDR *xdrs, bamboo_get_args *objp)
{
// LIANG
//	register int32_t *buf;

	 if (!xdr_string (xdrs, &objp->application, 255))
		 return FALSE;
	 if (!xdr_string (xdrs, &objp->client_library, 255))
		 return FALSE;
	 if (!xdr_bamboo_key (xdrs, objp->key))
		 return FALSE;
	 if (!xdr_int (xdrs, &objp->maxvals))
		 return FALSE;
	 if (!xdr_bamboo_placemark (xdrs, &objp->placemark))
		 return FALSE;
	return TRUE;
}

bool_t
xdr_bamboo_get_res (XDR *xdrs, bamboo_get_res *objp)
{
// LIANG
//	register int32_t *buf;
	u_int values_len_pass = objp->values.values_len;
        char* values_val_pass = (char*)objp->values.values_val;
	 if (!xdr_array (xdrs, &values_val_pass, &values_len_pass, ~0,
		sizeof (bamboo_value), (xdrproc_t) xdr_bamboo_value))
		 return FALSE;
	 if (!xdr_bamboo_placemark (xdrs, &objp->placemark))
		 return FALSE;
	return TRUE;
}
