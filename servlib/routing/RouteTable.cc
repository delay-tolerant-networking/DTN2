
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

Contact*
RouteTable::next_hop(Bundle* b)
{
    NOTIMPLEMENTED;
}
