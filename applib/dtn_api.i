/* -*-c-*- */

%module dtn
%include cstring.i
%include std_string.i

%{
/* Include files needed to build the wrapper code */
using namespace std;

#include "dtn_types.h"
#include "dtn_api.h"
#include "dtn_api_wrap.cc"
%}

/* Pull in the functions and types exported by swig */

%include "dtn_types.h"
%include "dtn_errno.h"
%include "dtn_api_wrap.cc"
