#ifndef __SQL_DISABLED__

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



/******************************************************************************
 *
 * SQLInsert
 *
 *****************************************************************************/

/**
 * Constructor.
 */
SQLInsert::SQLInsert(const char* table_name, SQLImplementation* sql_impl)
    : SQLQuery(MARSHAL)
{
    table_name_ = table_name;
    sql_impl_ = sql_impl ; 
}


// Virtual functions inherited from SerializeAction

/**
 * Initialize the query buffer.
 */
void 
SQLInsert::begin_action() 
{
    query_.appendf("INSERT INTO %s  VALUES(",table_name_);
}

/**
 * Clean the query in the end, trimming the trailing ',' and adding a
 * closing parenthesis.
 */
void 
SQLInsert::end_action() 
{
    if (query_.data()[query_.length() - 1] == ',') {
	query_.data()[query_.length() - 1] = ')';
    }
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


void 
SQLInsert::process(const char* name, std::string* s) 
{
    query_.appendf("'%s',", sql_impl_->escape_string(s->c_str()));
}

void 
SQLInsert::process(const char* name, u_char* bp, size_t len)
{
    query_.appendf("'%s',", sql_impl_->escape_binary(bp, len));
}

void 
SQLInsert::process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy)
{
    NOTIMPLEMENTED;
}

/******************************************************************************
 *
 * SQLUpdate
 *
 *****************************************************************************/

/**
 * Constructor.
 */
SQLUpdate::SQLUpdate(const char* table_name, SQLImplementation* sql_impl)
    : SQLQuery(MARSHAL)
{
    table_name_ = table_name;
    sql_impl_ = sql_impl ; 
}


// Virtual functions inherited from SerializeAction
void 
SQLUpdate::begin_action() 
{
    query_.appendf("UPDATE %s SET ",table_name_);
}

void 
SQLUpdate::end_action() 
{
    if (query_.data()[query_.length() - 2] == ',') {
	query_.data()[query_.length() - 2] =  ' ';
    }
}

void 
SQLUpdate::process(const char* name, u_int32_t* i)
{
    query_.appendf("%s = %u, ", name, *i);
}

void 
SQLUpdate::process(const char* name, u_int16_t* i)
{
    query_.appendf("%s = %u, ", name, *i);
}

void 
SQLUpdate::process(const char* name, u_int8_t* i)
{
    query_.appendf("%s = %u, ", name, *i);
}

void 
SQLUpdate::process(const char* name, int32_t* i)
{
    query_.appendf("%s = %d, ", name, *i);
}

void 
SQLUpdate::process(const char* name, int16_t* i)
{
    query_.appendf("%s = %d, ", name, *i);
}

void 
SQLUpdate::process(const char* name, int8_t* i)
{
    query_.appendf("%s = %d, ", name, *i);
}

void 
SQLUpdate::process(const char* name, bool* b) {

    if (*b) {
        query_.appendf("%s = 'TRUE', ", name);
    } else {
        query_.appendf("%s = 'FALSE', ", name);
    }
}


void 
SQLUpdate::process(const char* name, std::string* s) 
{
    query_.appendf("%s = '%s', ", name, sql_impl_->escape_string(s->c_str()));
}

void 
SQLUpdate::process(const char* name, u_char* bp, size_t len)
{
    query_.appendf("%s = '%s', ", name, sql_impl_->escape_binary(bp, len));
}

void 
SQLUpdate::process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy)
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

SQLTableFormat::SQLTableFormat(const char* table_name,
                               SQLImplementation* sql_impl)
    : SQLQuery(INFO)
{
    table_name_ = table_name;
    sql_impl_ = sql_impl ; 
}


// Virtual functions inherited from SerializeAction

void 
SQLTableFormat::begin_action() 
{
    query_.appendf("CREATE TABLE  %s  (",table_name_);
}

void 
SQLTableFormat::end_action() 
{
    if (query_.data()[query_.length() - 1] == ',') {
	query_.data()[query_.length() - 1] =  ')';
    }
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
                   (int)column_prefix_.length(), column_prefix_.data(),
                   name, type);
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
    append(name,sql_impl_->binary_datatype());
}
    
void 
SQLTableFormat::process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy)
{
    append(name,sql_impl_->binary_datatype());
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
    sql_impl_ = db;
}

const char* 
SQLExtract::next_field()
{
    return sql_impl_->get_value(0,field_++);
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
        break;
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
    
    size_t len = s->length();
    
    if (log_) logf(log_, LOG_DEBUG, "<=string(%d: '%.*s')",
                   len, (int)(len < 32 ? len : 32), s->data());
}

void 
SQLExtract::process(const char* name, u_char* bp, size_t len)
{
    const char* buf = next_field();
    if (buf == NULL) return;
 
    const u_char* v = sql_impl_->unescape_binary((const u_char*)buf);

    memcpy(bp, v, len);
    if (log_) {
        std::string s;
        hex2str(&s, bp, len < 16 ? len : 16);
        logf(log_, LOG_DEBUG, "<=bufc(%d: '%.*s')",
             len, (int)s.length(), s.data());
    }

}


void 
SQLExtract::process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy)
{
    NOTIMPLEMENTED;
}

#endif /* __SQL_DISABLED__ */
