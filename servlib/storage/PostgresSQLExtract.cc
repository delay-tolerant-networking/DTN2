#include "PostgresSQLExtract.h"

/**
 * Constructor.
 */

PostgresSQLExtract::PostgresSQLExtract(PgDatabase* db)
  : SQLExtract()
{
  data_base_pointer_ = db;
}


const char*
PostgresSQLExtract::next_slice()
{    
    if (error_)
        return NULL;
    const char* ret ;
    ret = data_base_pointer_->GetValue(0,field_);
    field_ ++ ; 
    return ret;
}
