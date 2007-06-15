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

#ifndef _PROPHET_QUEUE_POLICY_H_
#define _PROPHET_QUEUE_POLICY_H_

#include "Bundle.h"
#include "Table.h"
#include "Stats.h"

#include <string>

namespace prophet
{

// forward declaration
class QueueComp;

struct QueuePolicy
{
    /**
      * Queuing policies
      * p. 18, 3.7
      */
    typedef enum {
        INVALID_QP = 0,
        FIFO,
        MOFO,
        MOPR,
        LINEAR_MOPR,
        SHLI,
        LEPR
    } q_policy_t;

    /**
     * Utility function to convert type code to const char*
     */
    static const char*
    qp_to_str(q_policy_t qp)
    {
        switch(qp) {
#define CASE(_q_p) case _q_p: return # _q_p
        CASE(FIFO);
        CASE(MOFO);
        CASE(MOPR);
        CASE(LINEAR_MOPR);
        CASE(SHLI);
        CASE(LEPR);
#undef CASE
        default: return "Unknown queuing policy";
        }
    }

    static q_policy_t
    str_to_qp(const char* str)
    {
        std::string qp(str);
        if (qp == "FIFO")
            return FIFO;
        if (qp == "MOFO")
            return MOFO;
        if (qp == "MOPR")
            return MOPR;
        if (qp == "LINEAR_MOPR")
            return LINEAR_MOPR;
        if (qp == "SHLI")
            return SHLI;
        if (qp == "LEPR")
            return LEPR;
        return INVALID_QP;
    }

    /**
     * Factory method for creating QueuePolicy comparator
     * instance. It is the caller's responsibility to ensure
     * that pointers supplied as parameters to this factory
     * method are valid for the lifetime of the returned
     * QueueComp instance and any of its copies.
     */
    static inline QueueComp* policy(q_policy_t qp,
                                    const Stats* stats = NULL,
                                    const Table* nodes = NULL,
                                    u_int min_forward = 0);

}; // struct QueuePolicy

/**
 * Bundle queuing policy requires a sort order, which is provided
 * by QueueComp and its derivatives.
 */
class QueueComp
{
public:
    /**
     * Destructor
     */
    virtual ~QueueComp() {}

    /**
      * Comparator operator
      */
    virtual bool operator() (const Bundle* a, const Bundle* b) const
    {
        if (verbose_)
        printf("FIFO: %u:%u %s %u:%u\n",
               b->creation_ts(),
               b->sequence_num(),
               (*b < *a) ? "<" : "!<",
               a->creation_ts(),
               a->sequence_num());
        return *b < *a;
    }

    ///@{  Accessors
    QueuePolicy::q_policy_t qp() const { return qp_; }
    const Stats* stats() const { return stats_; }
    const Table* nodes() const { return nodes_; }
    ///@}

protected:
    friend class QueuePolicy;

    /**
     * Constructor, protected to enforce factory method
     */
    QueueComp(QueuePolicy::q_policy_t qp = QueuePolicy::INVALID_QP,
              const Stats* stats = NULL, const Table* nodes = NULL,
              u_int minfwd = 0)
        : qp_(qp),
          stats_(stats),
          nodes_(nodes),
          min_fwd_(minfwd),
          verbose_(false) {}

    QueuePolicy::q_policy_t qp_; ///< type code for this comparator's policy
    const Stats* stats_;         ///< For stats lookup per Bundle
    const Table* nodes_;         ///< For p_value lookup per Bundle

public:
    u_int  min_fwd_;             ///< Only evict bundles whose NF > min_fwd_

    bool verbose_; ///< debug setting
}; // struct QueuePolicy

/**
 * Queuing policy comparator for MOFO
 */
class QueueCompMOFO : public QueueComp
{
public:
    /**
     * Destructor
     */
    virtual ~QueueCompMOFO() {}

    /**
     * Virtual from std::greater
     */
    virtual bool operator() (const Bundle* a, const Bundle* b) const
    {
        // evict most forwarded first (reverse order)
        if (verbose_)
        printf("MOFO: %d (%d) %s %d (%d)\n",
               a->sequence_num(),
               a->num_forward(),
               (a->num_forward() < b->num_forward()) ? ">" : "<",
               b->sequence_num(),
               b->num_forward());
        return a->num_forward() < b->num_forward();
    }

protected:
    friend class QueuePolicy;

    /**
     * Constructor, protected to enforce factory method
     */
    QueueCompMOFO(QueuePolicy::q_policy_t qp)
        : QueueComp(qp) {}
}; // class QueueCompMOFO

/**
 * Queuing policy comparator for MOPR
 */
class QueueCompMOPR : public QueueComp
{
public:
    /**
     * Destructor
     */
    virtual ~QueueCompMOPR() {}

    /**
     * Virtual from std::greater
     */
    virtual bool operator() (const Bundle* a, const Bundle* b) const
    {
        // evict most favorably forwarded first
        if (verbose_)
        printf("MOPR: %d (%.2f) %s %d (%.2f)\n",
               a->sequence_num(),
               stats_->get_mopr(a),
               (stats_->get_mopr(a) < stats_->get_mopr(b)) ? "<" : "!<",
               b->sequence_num(),
               stats_->get_mopr(b));
        return stats_->get_mopr(a) < stats_->get_mopr(b);
    }

protected:
    friend class QueuePolicy;

    /**
     * Constructor, protected to enforce factory method
     */
    QueueCompMOPR(QueuePolicy::q_policy_t qp, const Stats* stats)
        : QueueComp(qp,stats) {}
}; // class QueueCompMOPR

/**
 * Queuing policy comparator for LINEAR_MOPR
 */
class QueueCompLMOPR : public QueueComp
{
public:
    /**
     * Destructor
     */
    virtual ~QueueCompLMOPR() {}

    /**
     * Virtual from std::greater
     */
    virtual bool operator() (const Bundle* a, const Bundle* b) const
    {
        // evict most favorably forwarded first, linear increase
        if (verbose_)
        printf("LMOPR: %d (%.2f) %s %d (%.2f)\n",
               a->sequence_num(),
               stats_->get_lmopr(a),
               (stats_->get_lmopr(a) < stats_->get_lmopr(b)) ? "<" : "!<",
               b->sequence_num(),
               stats_->get_lmopr(b));
        return stats_->get_lmopr(a) < stats_->get_lmopr(b);
    }

protected:
    friend class QueuePolicy;

    /**
     * Constructor, protected to enforce factory method
     */
    QueueCompLMOPR(QueuePolicy::q_policy_t qp, const Stats* stats)
        : QueueComp(qp,stats) {}
}; // class QueueCompLMOPR

/**
 * Queuing policy comparator for SHLI
 */
class QueueCompSHLI : public QueueComp
{
public:
    /**
     * Destructor
     */
    virtual ~QueueCompSHLI() {}

    /**
     * Virtual from std::greater
     */
    virtual bool operator() (const Bundle* a, const Bundle* b) const
    {
        // evict shortest lifetime first

        // expiration ts is an offset to be added to creation ts
        u_int32_t ae = a->creation_ts() + a->expiration_ts();
        u_int32_t be = b->creation_ts() + b->expiration_ts();

        if (verbose_)
        printf("SHLI: %d (%d) %s %d (%d)\n",
               a->sequence_num(),
               ae,
               (ae > be) ? ">" : "!>",
               b->sequence_num(),
               be);
        return ae > be;
    }

protected:
    friend class QueuePolicy;

    /**
     * Constructor, protected to enforce factory method
     */
    QueueCompSHLI(QueuePolicy::q_policy_t qp)
        : QueueComp(qp) {}
}; // class QueueCompSHLI

/**
 * Queuing policy comparator LEPR
 */
class QueueCompLEPR : public QueueComp
{
public:
    /**
     * Destructor
     */
    virtual ~QueueCompLEPR() {}

    /**
     * Virtual from std::greater
     */
    virtual bool operator() (const Bundle* a, const Bundle* b) const
    {
        // evict least probable first
        if (verbose_)
        printf("LEPR: %d (%.2f) %s %d (%.2f)\n",
               a->sequence_num(),
               nodes_->p_value(a),
               (nodes_->p_value(b) < nodes_->p_value(a)) ? ">" : "<",
               b->sequence_num(),
               nodes_->p_value(b));
        return nodes_->p_value(b) < nodes_->p_value(a);
    }

protected:
    friend class QueuePolicy;

    /**
     * Constructor, protected to enforce factory method
     */
    QueueCompLEPR(QueuePolicy::q_policy_t qp,
                  const Table* nodes,
                  u_int min_forward)
        : QueueComp(qp,NULL,nodes,min_forward) {}
}; // class QueueCompLEPR

QueueComp*
QueuePolicy::policy(QueuePolicy::q_policy_t qp,
                    const Stats* stats, const Table* nodes,
                    u_int min_forward)
{
    (void) stats;
    (void) nodes;
    switch (qp)
    {

        // first in first out: evict oldest first
        case QueuePolicy::FIFO:
            return new QueueComp(qp);

        // evict most forwarded first
        case QueuePolicy::MOFO:
            return new QueueCompMOFO(qp);

        // evict most favorably forwarded first
        case QueuePolicy::MOPR:
        {
            if (stats == NULL) return NULL;
            return new QueueCompMOPR(qp,stats);
        }

        // evict most favorably forwarded first, linear increase
        case QueuePolicy::LINEAR_MOPR:
        {
            if (stats == NULL) return NULL;
            return new QueueCompLMOPR(qp,stats);
        }

        // evict shortest lifetime first
        case QueuePolicy::SHLI:
            return new QueueCompSHLI(qp);

        // evict least probable first
        case QueuePolicy::LEPR:
        {
            if (min_forward == 0 || nodes == NULL) return NULL;
            return new QueueCompLEPR(qp,nodes,min_forward);
        }

        // oops, no QP specified
        case QueuePolicy::INVALID_QP:
        default:
            return NULL;
    }
}

}; // namespace prophet

#endif // _PROPHET_QUEUE_POLICY_H_
