
#include "ParamCommand.h"
#include "bundling/BundlePayload.h"
#include "conv_layers/TCPConvergenceLayer.h"

ParamCommand::ParamCommand() :
    AutoCommandModule("param")
{
}

void
ParamCommand::at_reg()
{
    bind_s("payload_dir",
           &BundlePayload::dir_, "/tmp/bundles");
    bind_i("payload_mem_threshold",
           (int*)&BundlePayload::mem_threshold_, 16384);

    bind_i("tcpcl_ack_blocksz",
           &TCPConvergenceLayer::Defaults.ack_blocksz_, 1024);
    bind_i("tcpcl_keepalive_timer",
           &TCPConvergenceLayer::Defaults.keepalive_timer_, 10);
}

const char*
ParamCommand::help_string()
{
    return("param set <var> <val");
}
