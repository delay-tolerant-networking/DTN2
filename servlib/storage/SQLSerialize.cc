
#include "SQLSerialize.h"
#include "debug/Log.h"


#include "iostream"
#include "util/StringUtils.h"

using namespace std;


/******************************************************************************
 *
 * SQLInsert
 *
 *****************************************************************************/


/**
 * Constructor.
 */

SQLInsert::SQLInsert()
    : SerializeAction(MARSHAL)
{
  firstmember_=0;
  query_ = "";
}


int
SQLInsert::action(SerializableObject* object)
{
  error_ = false;
  
  object->serialize(this);
  
  if (error_ == true)
    return -1;

  return 0;

}

void 
SQLInsert::append_query(const char* c) {

  if (firstmember_ == 0) {
    query_ = query_ + "" +  c; 
  }
  else {
    query_ = query_ + "," +  c; 
  }
  firstmember_ = 1;
}

// Virtual functions inherited from SerializeAction
void 
SQLInsert::process(const char* name, u_int32_t* i) {

  char buffer [50];  
  sprintf(buffer,"%u",*i);
  string val(buffer) ;
  append_query(val.c_str());

}

void 
SQLInsert::process(const char* name, string* s) 
{
  string val ;
  val =   "'" + *s + "'" ; 
  append_query(val.c_str());

}

void 
SQLInsert::process(const char* name, bool* b) {

  string val = ""  ;
  if (*b == 0)  val = "false" ; else val = "true";
  append_query(val.c_str());

}


void 
SQLInsert::process(const char* name, u_int16_t* i) {

  char buffer [50];  
  sprintf(buffer,"%u",*i);
  string val(buffer) ;
  append_query(val.c_str());

}

void
SQLInsert::process(const char* name, u_int8_t* i) {

  string val = ""  ;
  val.assign(1,*i);
  val =   "'" + val + "'" ; 
  append_query(val.c_str());
}
 
//todo, check if below is correct
void 
SQLInsert::process(const char* name, u_char* bp, size_t len) {

  char* p = new char[len+1];
  p[len+1]=0;

  string val (p);

  val =   "'" + val + "'" ; 
  append_query(val.c_str());
  
  if (log_) {
    std::string s;
    hex2str(&s, bp, len < 16 ? len : 16);
    logf(log_, LOG_DEBUG, "=>bufc(%d: '%.*s')", len, s.length(), s.data());
  }

}
    
void 
SQLInsert::process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy) {
    cout << " variable length buffer SQL serialiazation not implemented \n " ; 
    exit(0);
  
}
    

/******************************************************************************
 *
 * SQLTableFormat
 *
 *****************************************************************************/

/**
 * Constructor.
 */

SQLTableFormat::SQLTableFormat()
  : SerializeAction(INFO)
{
  firstmember_=0;
  query_ = "";
}


int
SQLTableFormat::action(SerializableObject* object)
{
  error_ = false;
  object->serialize(this);
  if (error_ == true)
    return -1;

  return 0;
}

void
SQLTableFormat::process(const char* name,  SerializableObject* object) 
{
  
  std::string column_old_prefix = column_prefix_ ; 
  if (name != "") 
    column_prefix_ = column_prefix_ +  name + "__"  ; 
  action(object);
  column_prefix_ = column_old_prefix;
}

// Helper function to conver a string to upper case. need to be moved.
void 
GetUpper(string& str)
{
   for(u_int i = 0; i < str.length(); i++)
   {
      str[i] = toupper(str[i]);
   }
   return;
}

void 
SQLTableFormat::append_query(const char* name, std::string data_type) {

  GetUpper(data_type);

  if (firstmember_ == 1) {
    query_ = query_ + ", "  ; 
  }
  firstmember_ = 1;
  
  query_ = query_ + "" +    column_prefix_ + name + "  " +  data_type; 
}

// Virtual functions inherited from SerializeAction
void 
SQLTableFormat::process(const char* name, u_int32_t* i) {

  std::string data_type = "bigint"  ;
  append_query(name, data_type);
}



void 
SQLTableFormat::process(const char* name, std::string* s) 
{
  std::string data_type = "text"  ;
  append_query(name, data_type);
}

void 
SQLTableFormat::process(const char* name, bool* b) {
  std::string data_type = "boolean"  ;
  append_query(name, data_type);
}


void 
SQLTableFormat::process(const char* name, u_int16_t* i) {

  std::string data_type = "int"  ;
  append_query(name, data_type);

}

void
SQLTableFormat::process(const char* name, u_int8_t* i) {

  std::string data_type = "char"  ;
  append_query(name, data_type);

}
 
void 
SQLTableFormat::process(const char* name, u_char* bp, size_t len) {

  std::string data_type = "text"  ;
  append_query(name, data_type);

}
    
void 
SQLTableFormat::process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy) {
  cout << " variable length buffer SQL serialiazation not implemented \n " ; 
  exit(0);
}
    


/******************************************************************************
 *
 * SQLExtract
 *
 *****************************************************************************/

/**
 * Constructor.
 */

SQLExtract::SQLExtract()
  : SerializeAction(UNMARSHAL)
{
  field_ = 0; 
}


int
SQLExtract::action(SerializableObject* object)
{  
  error_ = false;
  object->serialize(this);
  if (error_ == true)
   return -1;
 
 return 0;

}


void
SQLExtract::process(const char* name, u_int32_t* i)
{
    const char* buf = next_slice();
    if (buf == NULL) return;
    
    *i = atoi(buf) ; 

    if (log_) logf(log_, LOG_DEBUG, "<=int32(%d)", *i);
}

void 
SQLExtract::process(const char* name, u_int16_t* i)
{
    const char* buf = next_slice();
    if (buf == NULL) return;
    *i = atoi(buf) ; 
    
    if (log_) logf(log_, LOG_DEBUG, "<=int16(%d)", *i);
}


// todo, check character portability, should be ok if *i is a normal char
void 
SQLExtract::process(const char* name, u_int8_t* i)
{
    const char* buf = next_slice();
    if (buf == NULL) return;
    
    *i = buf[0];
    if (log_) logf(log_, LOG_DEBUG, "<=int8(%d)", *i);
}


bool
istrue (const char *buf) {

  if (buf == NULL) return 0;
 
  char first_char = buf[0] ; 
  if ((first_char == 'f') || ( first_char == 'F')) return 0;
  if ((first_char == 't') || ( first_char == 'T')) return 1;
  
  if (first_char == '0') return 0;
  if (first_char == '1') return 1;
  
  return 0;
}

void 
SQLExtract::process(const char* name, bool* b)
{
    const char* buf = next_slice();

    if (buf == NULL) return;
    
    if (istrue(buf)) *b = 1 ; else *b = 0;
    
    if (log_) logf(log_, LOG_DEBUG, "<=bool(%c)", *b ? 'T' : 'F');
}

void 
SQLExtract::process(const char* name, u_char* bp, size_t len)
{
    const char* buf = next_slice();
    if (buf == NULL) return;
    
    memcpy(bp, buf, len);

    if (log_) {
        std::string s;
        hex2str(&s, bp, len < 16 ? len : 16);
        logf(log_, LOG_DEBUG, "<=bufc(%d: '%.*s')", len, s.length(), s.data());
    }
}


void 
SQLExtract::process(const char* name, u_char** bp, size_t* lenp, bool alloc_copy)
{
 cout << " variable length buffer SQL serialiazation not implemented \n " ; 
  exit(-1);
}

void 
SQLExtract::process(const char* name, std::string* s)
{
    const char* buf = next_slice();
    if (buf == NULL) return;
    s->assign(buf);
    size_t len;
    len = s->length();
    
    if (log_) logf(log_, LOG_DEBUG, "<=string(%d: '%.*s')",
                   len, len < 32 ? len : 32, s->data());
}
