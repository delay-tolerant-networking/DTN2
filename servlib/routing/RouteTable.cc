
#include "RouteTable.h"

RouteTable* RouteTable::instance_ = NULL;

void
RouteTable::init()
{
    instance_ = new RouteTable();
}

void
RouteTable::serialize(SerializeAction* a)
{
}

RouteTable::RouteTable()
    : Logger("/route")
{
}

Contact*
RouteTable::next_hop(Bundle* b)
{
    logf(LOG_WARN, "next hop not implemented");
    return NULL;
}
