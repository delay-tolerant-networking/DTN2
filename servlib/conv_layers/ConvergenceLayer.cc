#include "ConvergenceLayer.h"
#include "TCPConvergenceLayer.h"
#include "UDPConvergenceLayer.h"
#include "FileConvergenceLayer.h"

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
    log_debug("adding interface %s", iface->tuple()->c_str());
    return true;
}

/**
 * Remove an interface
 */
bool
ConvergenceLayer::del_interface(Interface* iface)
{
    log_debug("removing interface %s", iface->tuple()->c_str());
    return true;
}
