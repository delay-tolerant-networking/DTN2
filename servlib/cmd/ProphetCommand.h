/*
 *    Copyright 2006 Baylor University
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

#ifndef _PROPHET_COMMAND_H_
#define _PROPHET_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

namespace dtn {

/**
 * The "prophet" command.
 */
class ProphetCommand : public oasys::TclCommand {
public:
    ProphetCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);

    std::string fwd_strategy_;
    std::string q_policy_;
};

} // namespace dtn

#endif /* _PROPHET_COMMAND_H_ */
