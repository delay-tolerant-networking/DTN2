#ifndef _LTP_CONVERGENCE_LAYER_H_
#define _LTP_CONVERGENCE_LAYER_H_

#ifdef LTP_ENABLED
#include <oasys/thread/Thread.h>

#include "IPConvergenceLayer.h"
#include "ltp_base.h"
#include "ltp.h"
#include "ltp_cues.h"
namespace dtn{

class LTPConvergenceLayer : public IPConvergenceLayer {

public:
	//default port for ltp
	static const u_int16_t LTPCL_DEFAULT_PORT = 1113;
	
	//Constructor
	LTPConvergenceLayer();

	//interface up
	bool interface_up(Interface* iface, int argc, const char* argv[]);
	
	//interface down
	bool interface_down(Interface* iface);
	
	//dump interface (Show us what you got link)
	void dump_interface(Interface* iface, oasys::StringBuffer* buf);
	
	//initialise link (create the bits and bobs needed for the LTP CL)
	bool init_link(const LinkRef& link, int argc, const char* argv[]);

	//delete link (delete the aforementioned bits and bobs)
	void delete_link(const LinkRef& link);
	
	//dump link (Show us what you got link)
	void dump_link(const LinkRef& link, oasys::StringBuffer* buf);
	
	//open contact
	bool open_contact(const ContactRef& contact);

	//close contact
	bool close_contact(const ContactRef& contact);

	//bundle queued/send (Fly my pretty!)
	void bundle_queued(const LinkRef& link, const BundleRef& bundle);

	//parameter parsing
	class Params : public CLInfo {
	public:

		virtual void serialize(oasys::SerializeAction *a);
		in_addr_t local_addr_;		//local address to bind to
		u_int16_t local_port_;		//local port to bind to
		in_addr_t remote_addr_;		//address to connect to
		u_int16_t remote_port_;		//port to connect to
	};
	//struct to hold our default parameters
	static Params defaults_;

protected:

	bool parse_params(Params* params, int argc, const char** argv,
                      const char** invalidp);
	
    	class Receiver : public CLInfo,
		public Logger, public oasys::Thread {
	public:
		
		//constructor
		Receiver(LTPConvergenceLayer::Params* params);
	
		//destructor
		virtual ~Receiver() {}
	
		void run();
	
		LTPConvergenceLayer::Params params_;
	
		void set_should_stop(void);

		bool should_stop(void);

		void set_sock(int sock);

		int get_sock();

		ltpaddr listener;

	protected:
		//lets process our incoming ltp segment
		void process_data(u_char* bp, size_t len);

		bool should_stop_;

		int logfd_;

		// the listener socket
		int s_sock;

	};
	
	class Sender : public CLInfo , public Logger	{
	public:
		//constructor (see private)
		//destructor
		
		virtual ~Sender() {}
		bool init(Params* params, in_addr_t addr, u_int16_t port);
		
	private:
		friend class LTPConvergenceLayer;

		Sender(const ContactRef& contact);
		
		int send_bundle(const BundleRef& bundle);
		
		Params* params_;
		
		//ltp socket
		int sock;
		
		//ltp source and destination
		ltpaddr source;
		ltpaddr dest;
		
		//our receive buffer size
		int rxbufsize;
		
		//we need to know what contact we represent
		ContactRef contact_;
		
		// private buffer copy
		u_char buf_[1000000];
	};
	
};
}//namespace dtn

#endif // LTP_ENABLED

#endif // _LTP_CONVERGENCE_LAYER_H_
