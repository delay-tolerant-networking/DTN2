/*
 *    Copyright 2007 Baylor University
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

#include "Stats.h"

namespace prophet
{

Stats::~Stats()
{
    for (pstats::iterator i = pstats_.begin(); i != pstats_.end(); i++)
    {
        delete (*i).second;
    }
    pstats_.clear();
}

void
Stats::update_stats(const Bundle* b, double p)
{
    StatsEntry* se = find(b);

    // weed out the oddball
    if (se == NULL) return;

    // update p_max
    if (se->p_max_ < p) se->p_max_ = p;

    // update MOPR, Section 3.7, eq. 7
    se->mopr_ += (1 - se->mopr_) * p;

    // update LMOPR, Section 3.7, eq. 8
    se->lmopr_ += p;

    // this shouldn't be necessary, since the pointer is already stored
    // pstats_[b] = se;
}

double
Stats::get_p_max(const Bundle* b) const
{
    if (b == NULL) return 0.0;
    return const_cast<Stats*>(this)->find(b)->p_max_;
}

double
Stats::get_mopr(const Bundle* b) const
{
    if (b == NULL) return 0.0;
    return const_cast<Stats*>(this)->find(b)->mopr_;
}

double
Stats::get_lmopr(const Bundle* b) const
{
    if (b == NULL) return 0.0;
    return const_cast<Stats*>(this)->find(b)->lmopr_;
}

StatsEntry*
Stats::find(const Bundle* b)
{
    if (b == NULL) return NULL;

    // search for Stats for Bundle
    iterator i = pstats_.lower_bound(b->sequence_num());

    // return it if we find it
    if (i != pstats_.end() && !pstats_.key_comp()(b->sequence_num(),i->first))
        return (*i).second;

    // otherwise create new zero'd StatsEntry
    StatsEntry* se = new StatsEntry();

    // insert into container
    pstats_.insert(i,pstats::value_type(b->sequence_num(),se));

    // return new entry
    return se;
}

void
Stats::drop_bundle(const Bundle* b)
{
    if (b == NULL) return;

    iterator i = pstats_.find(b->sequence_num());
    if (i != pstats_.end())
    {
        delete (*i).second;
        pstats_.erase(i);
        dropped_++;
    }
}

}; // namespace prophet
