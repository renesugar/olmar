// trace.h
// module for diagnostic tracing

#ifndef __TRACE_H
#define __TRACE_H

#include <iostream.h>     // ostream


// add a subsystem to the list of those being traced
void traceAddSys(char const *sysName);

// remove a subsystem; must have been there
void traceRemoveSys(char const *sysName);

// see if a subsystem is among those to trace
bool tracingSys(char const *sysName);

// trace; if the named system is active, this yields cout (after
// sending a little output to identify the system); if not, it
// yields an ostream attached to /dev/null; when using this
// method, it is up to you to put the newline
ostream &trace(char const *sysName);

// give an entire string to trace; do *not* put a newline in it
// (the tracer will do that)
void trstr(char const *sysName, char const *traceString);


#endif // __TRACE_H
