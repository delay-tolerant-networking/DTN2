#include "ConvergenceLayer.h"
#include "TCPConvergenceLayer.h"
#include "UDPConvergenceLayer.h"
#include "FileConvergenceLayer.h"
#include "../sim/SimConvergenceLayer.h"

class SimConvergenceLayer;

ConvergenceLayer::~ConvergenceLayer()
{
}

ConvergenceLayer::AddressFamily* ConvergenceLayer::af_list_ = NULL;

void
ConvergenceLayer::add_clayer(const char* proto, ConvergenceLayer* cl)
{
    AddressFamily* af = new AddressFamily();
    af->proto_ = proto;
    af->cl_ = cl;

    af->next_ = af_list_;
    af_list_ = af;
}

void
ConvergenceLayer::init_clayers()
{
    add_clayer("tcp", new TCPConvergenceLayer());
    add_clayer("udp", new UDPConvergenceLayer());
    add_clayer("file", new FileConvergenceLayer());

}

ConvergenceLayer*
ConvergenceLayer::find_clayer(const std::string& admin)
{
    AddressFamily* af;
    size_t end = admin.find(':');
    
    if (end != std::string::npos) {
        for (af = af_list_; af != NULL; af = af->next_)
        {
            if (strncasecmp(admin.c_str(), af->proto_, end) == 0) {
                return af->cl_;
            }
        }
    }
    
    return NULL;
}

/**
 * Register a new interface.
 *
 * The default implementation doesn't do anything but log.
 */
bool
ConvergenceLayer::add_interface(Interface* iface,
                                int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->tuple().c_str());
    return true;
}

/**
 * Remove an interface
 */
bool
ConvergenceLayer::del_interface(Interface* iface)
{
    log_debug("removing interface %s", iface->tuple().c_str());
    return true;
}

/**
 * Open the connection to the given contact and prepare for
 * bundles to be transmitted.
 */
bool
ConvergenceLayer::open_contact(Contact* contact)
{
    log_debug("opening contact *%p", contact);
    return true;
}
    
/**
 * Close the connnection to the contact.
 */
bool
ConvergenceLayer::close_contact(Contact* contact)
{
    log_debug("closing contact *%p", contact);
    return true;
}

/**
 * Try to send the bundles queued up for the given contact. In
 * some cases (e.g. tcp) this is a no-op because open_contact spun
 * a thread which is blocked on the bundle list associated with
 * the contact. In others (e.g. file) there is no thread, so this
 * callback is used to send the bundle.
 */
void
ConvergenceLayer::send_bundles(Contact* contact)
{
    log_debug("sending bundles for contact *%p", contact);
}
