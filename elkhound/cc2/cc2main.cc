// cc2main.cc
// toplevel driver for cc2

#include <iostream.h>     // cout
#include <stdlib.h>       // exit

#include "trace.h"        // traceAddSys
#include "parssppt.h"     // ParseTreeAndTokens, treeMain
#include "cc_lang.h"      // CCLang
#include "ptreenode.h"    // PTreeNode


// no bison-parser present, so need to define this
Lexer2Token const *yylval = NULL;

// defined in cc2.gr
UserActions *makeUserActions();


void doit(int argc, char **argv)
{
  traceAddSys("progress");
  //traceAddSys("parse-tree");

  // parsing language options
  CCLang lang;
  lang.ANSI_Cplusplus();


  // --------------- parse --------------
  {
    SemanticValue treeTop;
    ParseTreeAndTokens tree(lang, treeTop);
    UserActions *user = makeUserActions();
    tree.userAct = user;
    if (!treeMain(tree, argc, argv,
          "  additional flags for cc2:\n"
          "    printTree          print tree after parsing (if avail.)\n"
          "")) {
      // parse error
      exit(2);
    }

    traceProgress(2) << "final parse result: " << treeTop << endl;

    if (treeTop && tracingSys("printTree")) {
      PTreeNode *node = (PTreeNode*)treeTop;
      cout << "local ambiguities: " << PTreeNode::alternativeCount << endl;
      cout << "number of parses: " << node->countTrees() << endl;
      node->printTree(cout);
    }

    delete user;
  }

  traceRemoveAll();
}

int main(int argc, char **argv)
{
  doit(argc, argv);

  return 0;
}
