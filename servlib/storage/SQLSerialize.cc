
#include "SQLSerialize.h"
#include "SQLImplementation.h"
#include "debug/Log.h"
#include "util/StringUtils.h"


/******************************************************************************
 *
 * SQLQuery
 *
 *****************************************************************************/

/**
 * Constructor.
 */
SQLQuery::SQLQuery(action_t type, const char* initial_query)
    : SerializeAction(type), query_(256, initial_query)
{
}


const char*
SQLQuery::query()
{
      return query_.c_str();
}

/**
 * Clean the query  in the end.
 * The process() functions of all subclasses append ',' after each
 * value, so trim a trailing ',' before returning the string.
 * Also, add a closing bracket.
 */
 

void 
SQLQuery::end_action() 
{
    if (query_.data()[query_.length() - 1] == ',') {
	query_.data()[query_.length() - 1] = ')';
    }
}


/******************************************************************************
 *
 * SQLInsert
 *
 *****************************************************************************/

/**
 * Constructor.
 */
SQLInsert::SQLInsert(const char* table_name)
    :SQLQuery(MARSHAL)
{
    table_name_ = table_name;
}


// Virtual functions inherited from SerializeAction

void 
SQLInsert::begin_action() 
{
 query_.appendf("INSERT INTO %s  VALUES(",table_name_);
}


void 
SQLInsert::process(const char* name, u_int32_t* i)
{
    query_.appendf("%u,", *i);
}

void 
SQLInsert::process(const char* name, u_int16_t* i)
{
    query_.appendf("%u,", *i);
}

void 
SQLInsert::process(const char* name, u_int8_t* i)
{
    query_.appendf("%u,", *i);
}

void 
SQLInsert::process(const char* name, int32_t* i)
{
    query_.appendf("%d,", *i);
}

void 
SQLInsert::process(const char* name, int16_t* i)
{
    query_.appendf("%d,", *i);
}

void 
SQLInsert::process(const char* name, int8_t* i)
{
    query_.appendf("%d,", *i);
}

void 
SQLInsert::process(const char* name, bool* b) {

    if (*b) {
        query_.append("'TRUE',");
    } else {
        query_.append("'FALSE',");
    }
}

// XXX Sushant: Need to escape special chars such as ' from s

void 
SQLInsert::process(const char* name, std::string* s) 
{
    query_.append("'");
    query_.append(*s);
    query_.append("'");

    query_.append(',');
}

void 
SQLInsert::process(const char* name, u_char* bp, size_t len)
{
    NOTIMPLEMENTED;
}

void 
SQLInsert::process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy)
{
    NOTIMPLEMENTED;
}

/******************************************************************************
 *
 * SQLTableFormat
 *
 *****************************************************************************/

/**
 * Constructor.
 */

SQLTableFormat::SQLTableFormat(const char* table_name)
    :SQLQuery(INFO)
{
    table_name_ = table_name;
}


// Virtual functions inherited from SerializeAction

void 
SQLTableFormat::begin_action() 
{
    query_.appendf(" CREATE TABLE  %s  (",table_name_);
}



void
SQLTableFormat::process(const char* name,  SerializableObject* object) 
{
    int old_len = column_prefix_.length();

    column_prefix_.appendf("%s__", name);
    object->serialize(this);
    column_prefix_.trim(column_prefix_.length() - old_len);
}

void 
SQLTableFormat::append(const char* name, const char* type)
{
    query_.appendf("%.*s%s %s,",
                   column_prefix_.length(), column_prefix_.data(), name, type);
}

// Virtual functions inherited from SerializeAction
void 
SQLTableFormat::process(const char* name, u_int32_t* i)
{
    append(name, "BIGINT");
}

void 
SQLTableFormat::process(const char* name, u_int16_t* i)
{
    append(name, "INTEGER");
}

void
SQLTableFormat::process(const char* name, u_int8_t* i) {

    append(name, "SMALLINT");
}
 

// Mysql and Postgres do not have common implementation of bool 
void 
SQLTableFormat::process(const char* name, bool* b)
{
//    append(name, "BOOLEAN");
      append(name, "TEXT");
}

void 
SQLTableFormat::process(const char* name, std::string* s) 
{
    append(name, "TEXT");
}

void 
SQLTableFormat::process(const char* name, u_char* bp, size_t len)
{
    NOTIMPLEMENTED;
}
    
void 
SQLTableFormat::process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy)
{
    NOTIMPLEMENTED;
}

/******************************************************************************
 *
 * SQLExtract
 *
 *****************************************************************************/

/**
 * Constructor.
 */

SQLExtract::SQLExtract(SQLImplementation* db)
    : SerializeAction(UNMARSHAL)
{
    field_ = 0;
    db_ = db;
}

const char* 
SQLExtract::next_field()
{
    return db_->get_value(0,field_++);
}

void
SQLExtract::process(const char* name, u_int32_t* i)
{
    const char* buf = next_field();
    if (buf == NULL) return;
    
    *i = atoi(buf) ; 

    if (log_) logf(log_, LOG_DEBUG, "<=int32(%d)", *i);
}

void 
SQLExtract::process(const char* name, u_int16_t* i)
{
    const char* buf = next_field();
    if (buf == NULL) return;

    *i = atoi(buf) ; 
    
    if (log_) logf(log_, LOG_DEBUG, "<=int16(%d)", *i);
}


// todo, check character portability, should be ok if *i is a normal char
void 
SQLExtract::process(const char* name, u_int8_t* i)
{
    const char* buf = next_field();
    if (buf == NULL) return;
    
    *i = buf[0];
    
    if (log_) logf(log_, LOG_DEBUG, "<=int8(%d)", *i);
}

void 
SQLExtract::process(const char* name, bool* b)
{
    const char* buf = next_field();

    if (buf == NULL) return;

    switch(buf[0]) {
    case 'T':
    case 't':
    case '1':
    case '\1':
        *b = true;
    case 'F':
    case 'f':
    case '0':
    case '\0':
        *b = false;
        break;
    default:
        logf("/sql", LOG_ERR, "unexpected value '%s' for boolean column", buf);
        error_ = true;
        return;
    }
    
    if (log_) logf(log_, LOG_DEBUG, "<=bool(%c)", *b ? 'T' : 'F');
}

void 
SQLExtract::process(const char* name, std::string* s)
{
    const char* buf = next_field();
    if (buf == NULL) return;
    s->assign(buf);
    size_t len;
    len = s->length();
    
    if (log_) logf(log_, LOG_DEBUG, "<=string(%d: '%.*s')",
                   len, len < 32 ? len : 32, s->data());
}

void 
SQLExtract::process(const char* name, u_char* bp, size_t len)
{
    NOTIMPLEMENTED;
}


void 
SQLExtract::process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy)
{
    NOTIMPLEMENTED;
}

