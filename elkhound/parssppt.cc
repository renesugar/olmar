// parssppt.cc
// code for parssppt.h

#include "parssppt.h"     // this module
#include "glr.h"          // toplevelParse
#include "trace.h"        // traceProcessArg


// ---------------------- ParseTree --------------------
ParseTreeAndTokens::ParseTreeAndTokens()
  : lexer2()
{}

ParseTreeAndTokens::~ParseTreeAndTokens()
{}


// ---------------------- other support funcs ------------------
void toplevelParse(ParseTreeAndTokens &ptree, char const *grammarFname,
                   char const *inputFname, char const *symOfInterestName)
{
  // parse
  GLR glr(ptree);
  glr.glrParseFrontEnd(ptree.lexer2, grammarFname,
                       inputFname, symOfInterestName);
}


// useful for simple treewalkers
void treeMain(ParseTreeAndTokens &ptree, int argc, char **argv)
{
  // remember program name
  char const *progName = argv[0];

  // parameters
  char const *symOfInterestName = NULL;

  // process args
  while (argc >= 2) {
    if (traceProcessArg(argc, argv)) {
      continue;
    }
    else if (0==strcmp(argv[1], "-sym") && argc >= 3) {
      symOfInterestName = argv[2];
      argc -= 2;
      argv += 2;
    }
    else {
      break;     // didn't find any more options
    }
  }

  if (argc != 3) {
    cout << "usage: " << progName << " [options] grammar-file input-file\n"
            "  options:\n"
            "    -tr <sys>:  turn on tracing for the named subsystem\n"
            "    -sym <sym>: name the \"symbol of interest\"\n"
            ;
    exit(0);
  }

  toplevelParse(ptree, argv[1], argv[2], symOfInterestName);
}
