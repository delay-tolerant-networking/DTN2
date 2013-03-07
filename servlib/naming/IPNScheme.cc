#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/debug/Log.h>
#include <oasys/util/Glob.h>

#include "IPNScheme.h"
#include "EndpointID.h"

template <>
dtn::IPNScheme* oasys::Singleton<dtn::IPNScheme>::instance_ = 0;

namespace dtn {

        bool
        IPNScheme::validate(const URI& uri, bool is_pattern)
        {
                (void) is_pattern;

                u_int64_t ipn_eid = 0, ipn_tag;
                const char *temp;

                if (!uri.valid())
                {
                        log_debug_p("/dtn/scheme/ipn", "IPNScheme::validate: invalid URI");
                        return false;
                }

                temp = uri.ssp().c_str();

                if (uri.ssp().length() < 3 || strncmp(temp, "//", 2) != 0)
                {
                        printf("Malformed IPN EID. Must be of the form ipn://posint[/posint]\n");
                        return false;
                }
                temp += 2;

		if(sscanf(temp, "%" PRIu64 "/*", &ipn_eid))
		{
			return true;
		}
                else if(sscanf(temp, "%" PRIu64 "/%" PRIu64, &ipn_eid, &ipn_tag) < 1)
                {
                        printf("THIS IS A BAD EID:\t%s\n", uri.ssp().c_str());
                        return false;
                }
                return true;
        }

        bool
        IPNScheme::match(const EndpointIDPattern& pattern,
        const EndpointID& eid)
        {
                ASSERT(pattern.scheme() == this);

                u_int64_t pattern_n=0, pattern_s=0;
		int flag_p;
                
		u_int64_t eid_n=0, eid_s=0;
		int flag_s;


                if(!strcmp(eid.c_str(), "dtn:none"))
                {
                        return false;
                }

                flag_s = sscanf(eid.c_str(), "ipn://%" PRIu64 "/%" PRIu64, &eid_n, &eid_s);
                flag_p = sscanf(pattern.c_str(), "ipn://%" PRIu64 "/%" PRIu64, &pattern_n, &pattern_s);

                if(flag_p < 1 || flag_s < 1)
                {
                        return false;
                }

                if(pattern_n == eid_n)
                {
                        if(flag_p == flag_s && pattern_s == eid_s)
                        {
                                return true;
                        }
                	if(strstr(pattern.c_str(), "/*") > 0)
				return true;
		        if(flag_p == 1 && flag_s == 2)
                        {
                                return false;
                        }
                }
                return false;
        }

        Scheme::singleton_info_t
        IPNScheme::is_singleton(const URI& uri)
        {
                (void)uri;
                return EndpointID::SINGLETON;
        }

        bool
        IPNScheme::append_service_tag(URI* uri, const char* tag)
        {
                if (uri == NULL) return false;

                u_int64_t temp = 0;

                //if(strcmp(tag, "ping") ==0)
                //{
                //        return true;
                //}

                if(sscanf(tag, "%" PRIu64, &temp) != 1)
                {
                        printf("Bad tag!\t%s\n", tag);
                        return false;
                }

                if (tag[0] != '/')
                {
                        uri->set_path(std::string("/")+tag);
                }
                else
                {
                        uri->set_path(tag);
                }
                return true;
        }

        bool
        IPNScheme::remove_service_tag(URI* uri)
        {
                if(uri == NULL) return false;
                uri->set_path("");
                return true;
        }



}

