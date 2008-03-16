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

#include "Node.h"
#include <time.h>       // for time()
#include <netinet/in.h> // for ntoh*,hton*
#include <math.h>       // for pow()

namespace prophet {

const double
NodeParams::DEFAULT_P_ENCOUNTER = 0.75;

const double
NodeParams::DEFAULT_BETA = 0.25;

const double
NodeParams::DEFAULT_GAMMA = 0.99;

const u_int
NodeParams::DEFAULT_KAPPA = 100;

Node::Node(const NodeParams* params)
    : p_value_(0.0), relay_(DEFAULT_RELAY), custody_(DEFAULT_CUSTODY),
      internet_gateway_(DEFAULT_INTERNET), dest_id_(""), heap_pos_(0)
{
    // store local copy of NodeParams
    if (params != NULL)
        params_ = new NodeParams(*params);
    else
        // take the default if nothing was specified
        params_ = new NodeParams();

    // read in current epoch
    age_ = time(0);
}

Node::Node(const Node& n)
    : p_value_(n.p_value_), relay_(n.relay_),
      custody_(n.custody_), internet_gateway_(n.internet_gateway_),
      dest_id_(n.dest_id_), age_(n.age_), heap_pos_(n.heap_pos_)
{
    // store local copy of NodeParams
    params_ = new NodeParams(*n.params_);
}

Node::Node(const std::string& dest_id,
           bool relay, bool custody, bool internet,
           const NodeParams* params)
    : params_(NULL), p_value_(0.0), relay_(relay),
      custody_(custody), internet_gateway_(internet), dest_id_(dest_id),
      heap_pos_(0)
{
    // store local copy of NodeParams
    if (params != NULL)
        params_ = new NodeParams(*params);
    else
        // take the default if nothing was specified
        params_ = new NodeParams();

    // read in current epoch
    age_ = time(0);
}

Node::~Node()
{
    delete params_;
}

void
Node::update_pvalue()
{
    // validate before proceeding
    if (!(p_value_ >= 0.0 && p_value_ <= 1.0)) return;
    if (params_ == NULL) return;

    // new p_value is P_(A,B), previous p_value_ is P_(A,B)_old
    // params_->encounter_ is P_encounter
    // A is the local node
    // B is the node just encountered, represented by this ProphetNode instance
    p_value_ = p_value_ + (1.0 - p_value_) * params_->encounter_;

    // update age to reflect data "freshness"
    age_ = time(0);
}

void
Node::update_transitive(double ab, double bc)
{
    // validate before proceeding
    if (!(p_value_ >= 0.0 && p_value_ <= 1.0)) return;
    if (!(ab >= 0.0 && ab <= 1.0)) return;
    if (!(bc >= 0.0 && bc <= 1.0)) return;
    if (p_value_ > bc) return;
    if (params_ == NULL) return;

    // new p_value_ is P_(A,C), previous p_value is P_(A,C)_old
    // params_->beta_ is beta
    // A is the local node
    // B is the node just encountered (originator of RIB)
    // C is the node represented by this ProphetNode instance
    p_value_ = (p_value_ * params_->beta_) + (1.0 - params_->beta_) * ab * bc * params_->encounter_;

    // update age to reflect data "freshness"
    age_ = time(0);
}

void
Node::update_age()
{
    // validate before proceeding
    if (!(p_value_ >= 0.0 && p_value_ <= 1.0)) return;
    if (params_ == NULL) return;

    // new p_value_ is P_(A,B), previous p_value is P_(A,B)_old
    // params_->gamma_ is gamma
    // timeunits is k
    u_int32_t now = time(0);

    double agefactor = 1.0;

    u_int timeunits = time_to_units(now - age_);
    if (timeunits > 0)
        agefactor = pow( params_->gamma_, timeunits );

    p_value_ *= agefactor;

    // update age to reflect data "freshness"
    age_ = time(0);
}

u_int
Node::time_to_units(u_int32_t timediff) const
{
    // prevent div by 0
    if (params_->kappa_ == 0.0) return 0;

    // kappa is in units of milliseconds per timeunit
    return (u_int) (timediff * 1000 / params_->kappa_);
}

}; // namespace prophet
