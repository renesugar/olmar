// emitcode.cc
// code for emitcode.h

#include "emitcode.h"      // this module
#include "syserr.h"        // xsyserror
#include "fileloc.h"       // SourceLocation
#include "trace.h"         // tracingSys

EmitCode::EmitCode(char const *f)
  : stringBuilder(),
    os(f),
    fname(f),
    line(1)
{
  if (!os) {
    xsyserror("open", fname);
  }
}

EmitCode::~EmitCode()
{
  flush();
}


int EmitCode::getLine()
{
  flush();
  return line;
}


void EmitCode::flush()
{
  // count newlines
  char const *p = pcharc();
  while (*p) {
    if (*p == '\n') {
      line++;
    }
    p++;
  }

  os << *this;
  setlength(0);
}


char const *hashLine()
{                   
  if (tracingSys("nolines")) {
    // emit with comment to disable its effect
    return "// #line ";
  }
  else {
    return "#line "; 
  }
}


// note that #line must be preceeded by a newline
string lineDirective(SourceLocation const &loc)
{
  return stringc << hashLine() << loc.line
                 << " \"" << loc.file->filename << "\"\n";
}

stringBuilder &restoreLine(stringBuilder &sb)
{
  // little hack..
  EmitCode &os = (EmitCode&)sb;

  // +1 because we specify what line will be *next*
  int line = os.getLine()+1;
  return os << hashLine() << line
            << " \"" << os.getFname() << "\"\n";
}
