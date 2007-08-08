
from dtnapi import *;

h = dtn_open();
if (h == -1):
    print "error in dtn_open";
    exit(1);

print "handle is", h;

src = dtn_build_local_eid(h, "src");
dst = dtn_build_local_eid(h, "dst");

print "src is", src, "dst is, dst";

regid = dtn_find_registration(h, dst);
if (regid != -1):
    print "found existing registration -- id ", regid, " calling bind...";
    dtn_bind(h, regid);
else:
    regid = dtn_register(h, dst, DTN_REG_DROP, 10, 0, "");
    print "created new registration -- id regid";


print "sending a bundle in memory...";
id = dtn_send(h, src, dst, "dtn:none", COS_NORMAL,
	      0, 30, DTN_PAYLOAD_MEM, "test payload");

print "bundle id: ", id;

print "  source:", id.source;
print "  creation_secs:",  id.creation_secs;
print "  creation_seqno:", id.creation_seqno;

del id;

print "receiving a bundle in memory...";
bundle = dtn_recv(h, DTN_PAYLOAD_MEM, 10000);

print "bundle:";
print "  source:", bundle.source;
print "  dest:", bundle.dest;
print "  replyto:", bundle.replyto;
print "  priority:", bundle.priority;
print "  dopts:", bundle.dopts;
print "  expiration:", bundle.expiration;
print "  creation_secs:", bundle.creation_secs;
print "  creation_seqno:", bundle.creation_seqno;
print "  payload:", bundle.payload;

del bundle;

print "dtn_recv timeout:";
bundle = dtn_recv(h, DTN_PAYLOAD_MEM, 0);
if (bundle or (dtn_errno(h) != DTN_ETIMEOUT)):
    print "  bundle is bundle, errno is", dtn_errno(h);
else:
    print "  ", dtn_strerror(dtn_errno(h));

dtn_close(h);




