
#ifndef _MESSAGE_H_
#define _MESSAGE_H_

/**
 * Defines a message
 */

class Message  {

public:
    static long total_ ;
    static long next() {
        return total_ ++ ;
    }

    Message();
    Message(int src, int dst, double size);
    
    int id () ;
    void set_size(double x) { size_ = x;}
    double size () { return size_ ; }
    void rm_bytes(double len) ;
    int src() { return src_ ; }
    int dst() { return dst_ ; }

    Message* clone() ;

protected:

    int id_;
    int src_;
    int dst_;
    double size_;
    double origsize_;
    double offset_;


};


#endif /* _MESSAGE_H_ */


