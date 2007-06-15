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

#ifndef _PROPHET_ORACLE_H_
#define _PROPHET_ORACLE_H_

#include "Params.h"
#include "Stats.h"
#include "Table.h"
#include "Repository.h"
#include "AckList.h"
#include "BundleCore.h"

namespace prophet
{

/**
 * The all-seeing Oracle has knowledge of Prophet parameters,
 * Prophet Bundle stats, Prophet's list of routes, and access
 * to the Bundle core.
 */
class Oracle
{
public:
    virtual ~Oracle() {}
    // params are read-only for Encounter
    virtual const ProphetParams* params() const = 0;
    virtual Stats*         stats()      = 0;
    virtual Table*         nodes()      = 0;
    virtual AckList*       acks()       = 0;
    virtual BundleCore*    core()       = 0;
    virtual void ack(const prophet::Bundle* b)
    {
        if (b == NULL) return;
        acks()->insert(b,core());
        stats()->drop_bundle(b);
        core()->drop_bundle(b);
    }
}; // class Oracle

}; // namespace prophet

#endif // _PROPHET_ORACLE_H_
