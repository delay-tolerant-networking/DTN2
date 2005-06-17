#include<vector>

#define CONTACT(s, d) { s, d }
#define absdiff(x,y) ((x<y)?((y)-(x)):((x)-(y)))
#define min(x,y) (((x)<(y))?x:y)
#define max(x,y) (((x)>(y))?x:y)

#define WARPING_WINDOW .02
#define PERIOD_TOLERANCE .02
#define MAX_DIST 1<<30

typedef struct {
    int start;
    int duration;
} Contact;

typedef std::vector<Contact> Log;

Log* find_schedule(Log* log);
