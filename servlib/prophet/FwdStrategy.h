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

#ifndef _PROPHET_FWD_STRATEGY_H_
#define _PROPHET_FWD_STRATEGY_H_

#include <algorithm>
#include "Bundle.h"
#include "Table.h"

namespace prophet
{

// forward declaration
class FwdStrategyComp;

struct FwdStrategy
{
    /**
     * Forwarding strategies
     * p. 17, 3.6
     */
    typedef enum {
        INVALID_FS = 0,
        GRTR,
        GTMX,
        GRTR_PLUS,
        GTMX_PLUS,
        GRTR_SORT,
        GRTR_MAX
    } fwd_strategy_t;

    /**
     * Utility function to convert type code to const char*
     */
    static const char*
    fs_to_str(fwd_strategy_t fs)
    {
        switch(fs) {
#define CASE(_f_s) case _f_s: return # _f_s
        CASE(GRTR);
        CASE(GTMX);
        CASE(GRTR_PLUS);
        CASE(GTMX_PLUS);
        CASE(GRTR_SORT);
        CASE(GRTR_MAX);
#undef CASE
        default: return "Unknown forwarding strategy";
        }
    }

    /**
     * Factory method to create instance of appropriate type 
     * of comparator
     */
    inline static FwdStrategyComp* strategy(
                                      FwdStrategy::fwd_strategy_t fs,
                                      const Table* local_nodes = NULL,
                                      const Table* remote_nodes = NULL);

}; // struct FwdStrategy

/**
 * Prophet forwarding strategy is laid out in Prophet I-D March 2006
 * Section 3.6, and involves two pieces: a decider function and a sort
 * order.  FwdStrategyComp and its derivatives fill the comparator 
 * function for sort order.  FwdStrategyComp defaults to FIFO ordering.
 */
class FwdStrategyComp :
    public std::binary_function<const Bundle*,const Bundle*,bool>
{
public:
    /**
     * Destructor
     */
    virtual ~FwdStrategyComp() {}

    /**
     * Comparator function for FIFO ordering in a heap
     */
    virtual bool operator() (const Bundle* a, const Bundle* b) const
    {
        return *b < *a;
    }

    ///@{ Accessors
    FwdStrategy::fwd_strategy_t fwd_strategy() const
    {
        return strategy_;
    }
    const char* fwd_strategy_str() const
    {
        return FwdStrategy::fs_to_str(strategy_);
    }
    ///@}

protected:
    friend class FwdStrategy; ///< for factory method

    /**
     * Constructor is protected to force use of factory method
     */
    FwdStrategyComp(FwdStrategy::fwd_strategy_t fs = FwdStrategy::INVALID_FS)
        : strategy_(fs) {}

    FwdStrategy::fwd_strategy_t strategy_; ///< which strategy is in use 
}; // class FwdStrategyComp

/**
 * Comparator for sorting Bundles according to GRTRSort,
 * Section 3.6, Prophet March 2006.  Sorted according to
 * metric P_(B,D) - P_(A,D), where A is local node, B is
 * peering Prophet node, and D represents the route to the
 * Bundle's destination.  
 */
class FwdStrategyCompGRTRSORT : public FwdStrategyComp
{
public:
    /**
     * Destructor
     */
    virtual ~FwdStrategyCompGRTRSORT() {}

    virtual bool operator() (const Bundle* a, const Bundle* b) const
    {
        if (local_ == NULL || remote_ == NULL) return false;
        double pa = remote_->p_value(a) - local_->p_value(a);
        double pb = remote_->p_value(b) - local_->p_value(b);
        return pa < pb;
    }

    ///@{ Accessors
    const Table* local_nodes() const { return local_; }
    const Table* remote_nodes() const { return remote_; }
    ///@}

protected:
    friend class FwdStrategy; ///< for factory method

    /**
     * Constructor is protected to restrict access to factory method
     */
    FwdStrategyCompGRTRSORT(FwdStrategy::fwd_strategy_t fs,
                            const Table* local, const Table* remote)
        : FwdStrategyComp(fs), local_(local), remote_(remote) {}

    const Table* local_;  ///< list of routes as known by local node
    const Table* remote_; ///< list of routes known by peer node

}; // class FwdStrategyCompGRTRSORT

class FwdStrategyCompGRTRMAX : public FwdStrategyComp
{
public:
    /**
     * Destructor
     */
    virtual ~FwdStrategyCompGRTRMAX() {}

    virtual bool operator() (const Bundle* a, const Bundle* b) const
    {
        if (remote_ == NULL) return false;
        return (remote_->p_value(a) < remote_->p_value(b));
    }

    ///@{ Accessors
    const Table* remote_nodes() const { return remote_; }
    ///@}

protected:
    friend class FwdStrategy; ///< for factory method

    /**
     * Constructor is protected to restrict access to factory method
     */
    FwdStrategyCompGRTRMAX(FwdStrategy::fwd_strategy_t fs,const Table* remote)
        : FwdStrategyComp(fs), 
          remote_(remote) {}

    const Table* remote_; ///< list of routes known by peer node
}; // class FwdStrategyCompGRTRMAX

/**
 * Due to extensive use of copy constructors in the STL, any inheritance
 * hierarchy of comparators will always get "clipped" back to the base
 * type. See Scott Meyer's excellent text on "Effective STL." To get 
 * around this limitation, use a wrapper that invokes the proper method
 * based on a pointer dereference. It's a long explanation, and the book
 * is worth every penny, so go spend US$40 and read up on it.
 */
struct BundleOfferComp :
    public std::binary_function<const Bundle*,const Bundle*,bool>
{
    BundleOfferComp(const FwdStrategyComp* comp)
        : comp_(comp) {}

    bool operator() (const Bundle* a, const Bundle* b) const
    {
        return comp_->operator()(a,b);
    }

    const FwdStrategyComp* comp_; ///< pointer to actual comparator instance
};

FwdStrategyComp*
FwdStrategy::strategy(FwdStrategy::fwd_strategy_t fs,
                      const Table* local,
                      const Table* remote)
{
    FwdStrategyComp* f = NULL;
    switch (fs)
    {
        case FwdStrategy::GRTR:
        case FwdStrategy::GTMX:
        case FwdStrategy::GRTR_PLUS:
        case FwdStrategy::GTMX_PLUS:
            // effectively uses BundleLess (FIFO) ordering
            f = new FwdStrategyComp(fs);
            break;
        case FwdStrategy::GRTR_SORT:
        {
            if (local == NULL || remote == NULL) return NULL;
            f = new FwdStrategyCompGRTRSORT(fs,local,remote);
            break;
        }
        case FwdStrategy::GRTR_MAX:
        {
            if (remote == NULL) return NULL;
            f = new FwdStrategyCompGRTRMAX(fs,remote);
            break;
        }
        case FwdStrategy::INVALID_FS:
        default:
            break;
    }
    return f;
}

}; // namespace prophet

#endif // _PROPHET_FWD_STRATEGY_H_
