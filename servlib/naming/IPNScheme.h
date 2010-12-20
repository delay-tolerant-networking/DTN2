#ifndef _IPN_SCHEME_H_
#define _IPN_SCHEME_H_

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "Scheme.h"
#include <oasys/util/Singleton.h>

namespace dtn
{

        class IPNScheme : public Scheme, public oasys::Singleton<IPNScheme>

        {
        public:

                virtual bool validate(const URI& uri, bool is_pattern = false);
                virtual bool match(const EndpointIDPattern& pattern,
                        const EndpointID& eid);
                virtual singleton_info_t is_singleton(const URI& uri);

                virtual bool append_service_tag(URI* uri, const char* tag);
                virtual bool remove_service_tag(URI* uri);
        private:
                friend class oasys::Singleton<IPNScheme>;
                IPNScheme() {}
        };
}

#endif
