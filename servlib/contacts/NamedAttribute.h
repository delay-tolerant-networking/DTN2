/* Copyright 2004-2007 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * $Id$
 */

#ifndef _NAMED_ATTRIBUTE_H
#define _NAMED_ATTRIBUTE_H

#include <string>
#include <vector>

#include "oasys/debug/DebugUtils.h"
#include "oasys/debug/Log.h"
#include "oasys/serialize/Serialize.h"

namespace dtn {

class AttributeName {
public:
    AttributeName(const std::string& name)
        : name_(name) {}
    AttributeName(const oasys::Builder&)
        : name_("") {}

    const std::string& name() const { return name_; }

private:

    std::string name_;
};

/**
 * Class for a generic list of attributes/parameters, used for various
 * configuration functions.
 */
class NamedAttribute: public oasys::Logger {
public:
    /**
     * Attribute Types
     */
    typedef enum {
        ATTRIBUTE_TYPE_INVALID = 0,
        ATTRIBUTE_TYPE_INTEGER,
        ATTRIBUTE_TYPE_UNSIGNED_INTEGER,
        ATTRIBUTE_TYPE_BOOLEAN,
        ATTRIBUTE_TYPE_STRING
    } attribute_type_t;

    /**
     * Attribute type to string conversion.
     */
    static const char* type_to_str(int type) {
        switch(type) {
        case ATTRIBUTE_TYPE_INVALID:          return "invalid";
        case ATTRIBUTE_TYPE_INTEGER:          return "integer";
        case ATTRIBUTE_TYPE_UNSIGNED_INTEGER: return "unsigned integer";
        case ATTRIBUTE_TYPE_BOOLEAN:          return "boolean";
        case ATTRIBUTE_TYPE_STRING:           return "string";
        }
        NOTREACHED;
    }
    
    NamedAttribute(const std::string& name, int v)
        : Logger("NamedAttribute", "/dtn/attribute/%s", name.c_str()),
          name_(name),
          type_(ATTRIBUTE_TYPE_INTEGER),
          ival_(v), uval_(0), bval_(false), sval_("") {}
    NamedAttribute(const std::string& name, u_int v)
        : Logger("NamedAttribute", "/dtn/attribute/%s", name.c_str()),
          name_(name),
          type_(ATTRIBUTE_TYPE_UNSIGNED_INTEGER),
          ival_(0), uval_(v), bval_(false), sval_("") {}
    NamedAttribute(const std::string& name, bool v)
        : Logger("NamedAttribute", "/dtn/attribute/%s", name.c_str()),
          name_(name),
          type_(ATTRIBUTE_TYPE_BOOLEAN),
          ival_(0), uval_(0), bval_(v), sval_("") {}
    NamedAttribute(const std::string& name, const std::string& v)
        : Logger("NamedAttribute", "/dtn/attribute/%s", name.c_str()),
          name_(name),
          type_(ATTRIBUTE_TYPE_STRING),
          ival_(0), uval_(0), bval_(false), sval_(v) {}

    NamedAttribute(const oasys::Builder&)
        : Logger("NamedAttribute", "/dtn/attribute/UNKNOWN"),
          name_(""),
          type_(ATTRIBUTE_TYPE_INVALID),
          ival_(0), uval_(0), bval_(false), sval_("") {}

    const std::string& name()       const { return name_.name(); }
    attribute_type_t   type()       const { return type_; }
    
    int                int_val()    const {
                           if (type_ != ATTRIBUTE_TYPE_INTEGER) {
                               log_debug("NamedAttribute::int_val: "
                                         "invalid type %s",
                                         type_to_str(type_));
                           }
                           return ival_;
                       }
    u_int              u_int_val()  const {
                           if (type_ != ATTRIBUTE_TYPE_UNSIGNED_INTEGER) {
                               log_debug("NamedAttribute::u_int_val: "
                                         "invalid type %s",
                                         type_to_str(type_));
                           }
                           return uval_;
                       }
    bool               bool_val()   const {
                           if (type_ != ATTRIBUTE_TYPE_BOOLEAN) {
                               log_debug("NamedAttribute::bool_val: "
                                         "invalid type %s",
                                         type_to_str(type_));
                           }
                           return bval_;
                       }
    const std::string& string_val() const {
                           if (type_ != ATTRIBUTE_TYPE_STRING) {
                               log_debug("NamedAttribute::string_val: "
                                         "invalid type %s",
                                         type_to_str(type_));
                           }
                           return sval_;
                       }

private:
    AttributeName    name_;

    attribute_type_t type_;

    int              ival_;
    u_int            uval_;
    bool             bval_;
    std::string      sval_;
};

typedef std::vector<AttributeName>  AttributeNameVector;
typedef std::vector<NamedAttribute> AttributeVector;

} // namespace dtn 

#endif /* _NAMED_ATTRIBUTE_H */
