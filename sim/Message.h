#ifndef _MESSAGE_H_
#define _MESSAGE_H_

/**
 * Defines a message. A message is the basic entity that is routed
 * by the simulator. Every message has a unique id.
 * If the message is fragmented, the fragments have the same id
 * but different offsets.
 */

#include <vector>

class Message  {
    
public:
    static long total_ ;
    static long next() {
        return total_ ++ ;
    }

    Message();
    Message(int src, int dst, double size);
    
    int id () ; /// returns id of the message
    void set_size(double x) { size_ = x;}
    double size () { return size_ ; }
    void rm_bytes(double len) ;
    int src() { return src_ ; }
    int dst() { return dst_ ; }

    Message* clone() ;
    

    std::vector<int>    hop_idx_;
    std::vector<double> hop_time_;

    void add_hop(int nodeid,double time) 
    { 
        //hop_idx_->push_back(nodeid); hop_time_->push_back(time);
    }
protected:

    int id_; ///> id of the message
    int src_;
    int dst_;
    double size_;
    double origsize_;
    double offset_;


};


#endif /* _MESSAGE_H_ */


