
#include "ParamCommand.h"
#include "bundling/BundlePayload.h"
#include "conv_layers/TCPConvergenceLayer.h"
#include "routing/BundleRouter.h"

ParamCommand::ParamCommand() 
    : TclCommand("param")
{
    bind_s("payload_dir",
           &BundlePayload::dir_, "/tmp/bundles");
    bind_i("payload_mem_threshold",
           (int*)&BundlePayload::mem_threshold_, 16384);
    bind_b("payload_test_no_remove",
           &BundlePayload::test_no_remove_, false);

    bind_i("proactive_frag_threshold",
           (int*)&BundleRouter::proactive_frag_threshold_, -1);

    bind_i("tcpcl_ack_blocksz",
           &TCPConvergenceLayer::Defaults.ack_blocksz_, 1024);
    bind_i("tcpcl_keepalive_interval",
           &TCPConvergenceLayer::Defaults.keepalive_interval_, 2);
    bind_i("tcpcl_test_fragment_size",
           &TCPConvergenceLayer::Defaults.test_fragment_size_, -1);
}

const char*
ParamCommand::help_string()
{
    return("param set <var> <val");
}
