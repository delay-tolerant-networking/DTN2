
#include "ParamCommand.h"
#include "bundling/BundlePayload.h"

ParamCommand ParamCommand::instance_;

ParamCommand::ParamCommand() :
    AutoCommandModule("param")
{
}

void
ParamCommand::at_reg()
{
    bind_s("payload_dir", &BundlePayload::dir_);
    bind_i("payload_mem_threshold", (int*)&BundlePayload::mem_threshold_);
}

const char*
ParamCommand::help_string()
{
    return("param set <var> <val");
}
