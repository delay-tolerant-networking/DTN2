#ifndef _DTNSERVER_H_
#define _DTNSERVER_H_

/**
  * Encapsulation class for the "guts" of the server library. All
  * functions and member variables are static.
 */
class DTNServer {
public:
    /**
     * Initialize and register all the server related dtn commands.
     */
    static void init_commands();
    
    /**
     * Initialize all components before modifying any configuration.
     */
    static void init_components();

    /**
     * Post configuration, start up all components.
     */
    static void start();

 protected:
    static void init_dir(const char * dirname);
    static void tidy_dir(const char * dirname);
    static void validate_dir(const char * dirname);
};

#endif /* _DTNSERVER_H_ */
