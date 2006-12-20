/*
Copyright 2004-2006 BBN Technologies Corporation

Licensed under the Apache License, Version 2.0 (the "License"); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at http://www.apache.org/licenses/LICENSE-2.0
 
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied.
    
See the License for the specific language governing permissions and limitations
under the License.

$Id$
*/

#ifndef _ECLA_COMMAND_H_
#define _ECLA_COMMAND_H_

#include <config.h>
#ifdef XERCES_C_ENABLED

#include <oasys/tclcmd/TclCommand.h>

namespace dtn {

/**
 * Parameter setting command
 */
class ECLACommand : public oasys::TclCommand {
public:
    ECLACommand();
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);
};


} // namespace dtn

#endif // XERCES_C_ENABLED
#endif // _ECLA_COMMAND_H_ 
