// glr.cc
// code for glr.h

/* Implementation Notes
 *
 * A design point: [GLR] uses more 'global's than I do.  My criteria
 * here is that something should be global (stored in class GLR) if
 * it has meaning between processing of tokens.  If something is only
 * used during the processing of a single token, then I make it a
 * parameter where necessary.
 *
 * Update: I've decided to make 'currentToken' and 'parserWorklist'
 * global because they are needed deep inside of 'glrShiftNonterminal',
 * though they are not needed by the intervening levels, and their
 * presence in the argument lists would therefore only clutter them.
 *
 * It should be clear that many factors contribute to this
 * implementation being slow, and I'm going to refrain from any
 * optimization for a bit.
 *
 * Description of the various lists in play here:
 *
 *   activeParsers
 *   -------------
 *   The active parsers are at the frontier of the parse tree
 *   space.  It *never* contains more than one stack node with
 *   a given parse state; I call this the unique-state property
 *   (USP).  If we're about to add a stack node with the same
 *   state as an existing node, we merge them (if it's a shift,
 *   we add another leftAdjState; if it's a reduction, we add a
 *   rule node *and* another leftAdjState).
 *
 *   Before a token is processed, activeParsers contains those
 *   parsers that successfully shifted the previous token.  This
 *   list is then copied to the parserWorklist (described below).
 *
 *   As reductions are processed, new parsers are generated and
 *   added to activeParsers (modulo USP).
 *
 *   Before the accumulated shifts are processed, the activeParsers
 *   list is cleared.  As each shift is processed, the resulting
 *   parser is added to activeParsers (modulo USP).
 *
 *   [GLR] calls this "active-parsers"
 *
 *
 *   parserWorklist
 *   --------------
 *   The worklist contains all parsers we have not yet been considered
 *   for advancement (shifting or reducing).  Initially, it is the
 *   same as the activeParsers, but we take a parser from it on
 *   each iteration of the inner loop in 'glrParse'.
 *
 *   Whenever, during processing of reductions, we add a parser to
 *   activeParsers, we also add it to parserWorklist.  This ensures
 *   the just-reduced parser gets a chance to reduce or shift.
 *
 *   [GLR] calls this "for-actor"
 *
 *
 *   pendingShifts
 *   -------------
 *   The GLR parser alternates between reducing and shifting.
 *   During the processing of a given token, shifting happens last.
 *   This keeps the parsers synchronized, since every token is
 *   shifted by every parser at the same time.  This synchronization
 *   is important because it makes merging parsers possible.
 *
 *   [GLR] calls this "for-shifter"
 *
 * 
 * Discussion of path re-examination, called do-limited-reductions by
 * [GLR]:
 *
 * After thinking about this for some time, I have reached the conclusion
 * that the only way to handle the above problem is to separate the
 * collection of paths from the iteration over them.
 *
 * Here are several alternative schemes, and the reasons they don't
 * work:
 *
 *   1. [GLR]'s approach of limiting re-examination to those involving
 *      the new link
 *
 *      This fails because it does not prevent re-examined paths
 *      from appearing in the normal iteration also.
 *
 *   2. Modify [GLR] so the new link can't be used after the re-examination
 *      is complete
 *
 *      Then if *another* new link is added, paths involving both new
 *      links wouldn't be processed.
 *
 *   3. Further schemes involving controlling which re-examination stage can
 *      use which links
 *
 *      Difficult to reason about, unclear a correct scheme exists, short
 *      of the full-blown path-listing approach I'm going to take.
 *
 *   4. My first "fix" which assumes there is never more than one path to
 *      a given parser
 *
 *      This is WRONG.  There can be more than one path, even as all such
 *      paths are labelled the same (namely, with the RHS symbols).  Consider
 *      grammar "E -> x | E + E" parsing "x+x+x": both toplevel parses use
 *      the "E -> E + E" rule, and both arrive at the root parser
 *
 * So, the solution I will implement is to collect all paths into a list
 * before processing any of them.  During path re-examination, I also will
 * collect paths into a list, this time only those that involve the new
 * link.
 *
 * This scheme is clearly correct, since path collection cannot be disrupted
 * by the process of adding links, and when links are added, exactly the new
 * paths are collected and processed.  It's easy to see that every path is
 * considered exactly once.
 *
 *
 * Below, parse-tree building activity is marked "TREEBUILD".
 */


#include "glr.h"         // this module
#include "strtokp.h"     // StrtokParse
#include "syserr.h"      // xsyserror
#include "trace.h"       // tracing system
#include "strutil.h"     // replace
#include "lexer1.h"      // Lexer1
#include "lexer2.h"      // Lexer2
#include "grampar.h"     // readGrammarFile
#include "parssppt.h"    // treeMain
#include "bflatten.h"    // BFlatten

#include <stdio.h>       // FILE
#include <fstream.h>     // ofstream

#if 1
  #define D(stmt) stmt
#else
  #define D(stmt) ((void)0)
#endif


// ------------- front ends to user code ---------------
SemanticValue GLR::duplicateSemanticValue(Symbol const *sym, SemanticValue sval)
{
  if (!sval) return sval;

  if (sym->isTerminal()) {
    return userAct->duplicateTerminalValue(sym->asTerminalC().termIndex, sval);
  }
  else {
    return userAct->duplicateNontermValue(sym->asNonterminalC().ntIndex, sval);
  }
}

void deallocateSemanticValue(Symbol const *sym, UserActions *user,
                             SemanticValue sval)
{
  if (!sval) return;

  if (sym->isTerminal()) {
    return user->deallocateTerminalValue(sym->asTerminalC().termIndex, sval);
  }
  else {
    return user->deallocateNontermValue(sym->asNonterminalC().ntIndex, sval);
  }
}

void GLR::deallocateSemanticValue(Symbol const *sym, SemanticValue sval)
{
  ::deallocateSemanticValue(sym, userAct, sval);
}


// ----------------------- StackNode -----------------------
int StackNode::numStackNodesAllocd=0;
int StackNode::maxStackNodesAllocd=0;


StackNode::StackNode(int id, int col, ItemSet const *st, UserActions *user)
  : stackNodeId(id),
    tokenColumn(col),
    state(st),
    leftSiblings(),
    referenceCount(0),
    userAct(user)
{
  INC_HIGH_WATER(numStackNodesAllocd, maxStackNodesAllocd);
}

StackNode::~StackNode()
{
  numStackNodesAllocd--;
  if (!unwinding()) {
    xassert(numStackNodesAllocd >= 0);
    xassert(referenceCount == 0);
  }

  // explicitly deallocate siblings, so I can deallocate their
  // semantic values if necessary (this requires knowing the
  // associated symbol, which the SiblinkLinks don't know)
  while (leftSiblings.isNotEmpty()) {
    Owner<SiblingLink> sib(leftSiblings.removeAt(0));
    D(trace("sval") << "deleting sval " << sib->sval << endl);
    deallocateSemanticValue(getSymbolC(), userAct, sib->sval);
  }
}


Symbol const *StackNode::getSymbolC() const
{
  return state->getStateSymbolC();
}


// add a new sibling by creating a new link
SiblingLink *StackNode::
  addSiblingLink(StackNode *leftSib, SemanticValue sval)
{
  SiblingLink *link = new SiblingLink(leftSib, sval);
  leftSiblings.append(link);
  return link;
}


void StackNode::decRefCt()
{
  xassert(referenceCount > 0);
  if (--referenceCount == 0) {
    delete this;
  }
}


STATICDEF void StackNode::printAllocStats()
{
  cout << "stack nodes: " << numStackNodesAllocd
       << ", max stack nodes: " << maxStackNodesAllocd
       << endl;
}


// ------------------ SiblingLink ------------------
SiblingLink::~SiblingLink()
{}


// ------------------ PendingShift ------------------
PendingShift::~PendingShift()
{}


// ------------------ PathCollectionState -----------------
PathCollectionState::PathCollectionState(Production const *p, int start)
  : startStateId(start),
    paths(),      // initially empty
    production(p)
{
  int len = p->rhsLength();
                                        
  // possible optimization: allocate these once, using the largest length
  // of any production, when parsing starts; do the same for 'toPass'
  // in collectReductionPaths
  siblings = new SiblingLink *[len];
  symbols = new Symbol const *[len];
}

PathCollectionState::~PathCollectionState()
{
  delete[] siblings;
  delete[] symbols;
}

PathCollectionState::ReductionPath::~ReductionPath()
{
  if (sval) {
    // this should be very unusual, since 'sval' is consumed
    // (and nullified) soon after the ReductionPath is created;
    // it can happen if an exception is thrown from certain places
    cout << "interesting: using ~ReductionPath's deallocator on " << sval << endl;
    deallocateSemanticValue(finalState->getSymbolC(), finalState->userAct, sval);
  }
}


// ------------------------- GLR ---------------------------
void decParserList(SObjList<StackNode> &list)
{
  SMUTATE_EACH_OBJLIST(StackNode, list, iter) {
    iter.data()->decRefCt();
  }
}

void incParserList(SObjList<StackNode> &list)
{
  SMUTATE_EACH_OBJLIST(StackNode, list, iter) {
    iter.data()->incRefCt();
  }
}


GLR::GLR(UserActions *user)
  : userAct(user),
    activeParsers(),
    nextStackNodeId(initialStackNodeId),
    currentTokenColumn(0),
    currentTokenClass(NULL),
    currentTokenValue(NULL),
    parserWorklist()
  // some fields (re-)initialized by 'clearAllStackNodes'
{}

GLR::~GLR()
{
  decParserList(activeParsers);
  decParserList(parserWorklist);
}


void GLR::clearAllStackNodes()
{
  nextStackNodeId = initialStackNodeId;
  currentTokenColumn = 0;
  
  // the stack nodes themselves are now reference counted, so they
  // should already be cleared if we're between parses
}


// process the input file, and yield a parse graph
bool GLR::glrParseNamedFile(Lexer2 &lexer2, SemanticValue &treeTop,
                            char const *inputFname)
{
  // do first phase lexer
  traceProgress() << "lexical analysis...\n";
  traceProgress(2) << "lexical analysis stage 1...\n";
  Lexer1 lexer1;
  {
    FILE *input = fopen(inputFname, "r");
    if (!input) {
      xsyserror("fopen", inputFname);
    }

    lexer1_lex(lexer1, input);
    fclose(input);

    if (lexer1.errors > 0) {
      printf("L1: %d error(s)\n", lexer1.errors);
      return false;
    }
  }

  // do second phase lexer
  traceProgress(2) << "lexical analysis stage 2...\n";
  lexer2_lex(lexer2, lexer1, inputFname);

  // parsing itself
  return glrParse(lexer2, treeTop);
}


// used to extract the svals from the nodes just under the
// start symbol reduction
SemanticValue GLR::grabTopSval(StackNode *node)
{
  SiblingLink *sib = node->leftSiblings.first();
  SemanticValue ret = sib->sval;
  sib->sval = duplicateSemanticValue(node->getSymbolC(), sib->sval);

  trace("sval") << "dup'd " << ret << " for top sval, yielded "
                << sib->sval << endl;

  return ret;
}

bool GLR::glrParse(Lexer2 const &lexer2, SemanticValue &treeTop)
{
  // get ready..
  traceProgress() << "parsing...\n";
  clearAllStackNodes();

  // create an initial ParseTop with grammar-initial-state,
  // set active-parsers to contain just this
  StackNode *first = makeStackNode(itemSets.first());
  activeParsers.append(first);
  first->incRefCt();

  // for each input symbol
  int tokenNumber = 0;
  for (ObjListIter<Lexer2Token> tokIter(lexer2.tokens);
       !tokIter.isDone(); tokIter.adv(), tokenNumber++) {
    Lexer2Token const *currentToken = tokIter.data();

    // debugging
    trace("parse")
      << "------- "
      << "processing token " << currentToken->toString()
      << ", " << activeParsers.count() << " active parsers"
      << " -------"
      << endl;

    // token reclassification
    int classifiedType = userAct->reclassifyToken(currentToken->type,
                                                  currentToken->sval);

    // convert the token to a symbol
    xassert((unsigned)classifiedType < (unsigned)numTerms);
    currentTokenClass = indexedTerms[classifiedType];
    currentTokenColumn = tokenNumber;
    currentTokenValue = currentToken->sval;

    // ([GLR] called the code from here to the end of
    // the loop 'parseword')

    // we will queue up shifts and process them all
    // at the end
    ObjList<PendingShift> pendingShifts;                  // starts empty

    // put active parser tops into a worklist
    decParserList(parserWorklist);
    parserWorklist = activeParsers;
    incParserList(parserWorklist);

    // work through the worklist
    RCPtr<StackNode> lastToDie = NULL;
    while (parserWorklist.isNotEmpty()) {
      RCPtr<StackNode> parser = parserWorklist.removeAt(0);     // dequeue
      parser->decRefCt();     // no longer on worklist

      // to count actions, first record how many parsers we have
      // before processing this one
      int parsersBefore = parserWorklist.count() + pendingShifts.count();

      // process this parser
      glrParseAction(parser, pendingShifts);

      // now observe change -- normal case is we now have one more
      int actions = (parserWorklist.count() + pendingShifts.count()) -
                      parsersBefore;

      if (actions == 0) {
        trace("parse") << "parser in state " << parser->state->id
                       << " died\n";
        lastToDie = parser;          // for reporting the error later if necessary
      }
      else if (actions > 1) {
        trace("parse") << "parser in state " << parser->state->id
                       << " split into " << actions << " parsers\n";
      }
    }

    // process all pending shifts
    glrShiftTerminals(pendingShifts);

    // if all active parsers have died, there was an error
    if (activeParsers.isEmpty()) {
      cout << "Line " << currentToken->loc.line
           << ", col " << currentToken->loc.col
           << ": Parse error at " << currentToken->toString() << endl;

      if (lastToDie == NULL) {
        // I'm not entirely confident it has to be nonnull..
        cout << "what the?  lastToDie is NULL??\n";
      }
      else {
        // print out the context of that parser
        cout << "last parser (state " << lastToDie->state->id << ") to die had:\n"
                "  sample input: " << sampleInput(lastToDie->state) << "\n"
                "  left context: " << leftContextString(lastToDie->state) << "\n";
      }

      return false;
    }
  }

  traceProgress() << "done parsing\n";
  trace("parse") << "Parse succeeded!\n";


  // finish the parse by reducing to start symbol
  if (activeParsers.count() != 1) {
    cout << "parsing finished with more than one active parser!\n";
    return false;
  }
  StackNode *last = activeParsers.nth(0);

  // pull out the semantic values; this assumes the start symbol
  // always looks like "Start -> Something EOF"; it also assumes
  // the top of the tree is unambiguous
  SemanticValue arr[2];
  StackNode *nextToLast = last->leftSiblings.first()->sib;
  arr[0] = grabTopSval(nextToLast);   // Something's sval
  arr[1] = grabTopSval(last);         // eof's sval

  // reduce
  trace("sval") << "handing toplevel svals " << arr[0]
                << " and " << arr[1] << " top start's reducer\n";
  treeTop = userAct->doReductionAction(
              last->state->getFirstReduction()->prodIndex, arr);


  #if 0
  // print the parse as a graph for my graph viewer
  // (the slight advantage to printing the graph first is that
  // if there is circularity in the parse tree, the graph
  // printer will handle it ok, whereas thet tree printer will
  // get into an infinite loop (so at least the graph will be
  // available in a file after I kill the process))
  if (tracingSys("parse-graph")) {      // profiling says this is slow
    traceProgress() << "writing parse graph...\n";
    writeParseGraph("parse.g");
  }


  // print parse tree in ascii
  TreeNode const *tn = getParseTree();
  if (tracingSys("parse-tree")) {
    tn->printParseTree(trace("parse-tree") << endl, 2 /*indent*/, false /*asSexp*/);
  }


  // generate an ambiguity report
  if (tracingSys("ambiguities")) {
    tn->ambiguityReport(trace("ambiguities") << endl);
  }    
  #endif // 0
  
  return true;
}


// do the actions for this parser, on input 'token'; if a shift
// is required, we postpone it by adding the necessary state
// to 'pendingShifts'; returns number of actions performed
// ([GLR] called this 'actor')
int GLR::glrParseAction(StackNode *parser,
                        ObjList<PendingShift> &pendingShifts)
{
  int actions =
    postponeShift(parser, pendingShifts);

  actions +=
    doAllPossibleReductions(parser, NULL /*no link restrictions*/);

  return actions;
}


int GLR::postponeShift(StackNode *parser,
                       ObjList<PendingShift> &pendingShifts)
{
  // see where a shift would go
  ItemSet const *shiftDest = parser->state->transitionC(currentTokenClass);
  if (shiftDest != NULL) {
    // postpone until later; save necessary state (the
    // parser and the state to transition to)

    // add (parser, shiftDest) to pending-shifts
    pendingShifts.append(new PendingShift(parser, shiftDest));
    
    return 1;   // 1 action
  }                        
  else {
    return 0;
  }
}


// mustUseLink: if non-NULL, then we only want to consider
// reductions that use that link
int GLR::doAllPossibleReductions(StackNode *parser,
                                 SiblingLink *mustUseLink)
{
  int actions = 0;

  // get all possible reductions where 'currentToken' is in Follow(LHS)
  ProductionList reductions;
  parser->state->getPossibleReductions(reductions, currentTokenClass,
                                       true /*parsing*/);

  // for each possible reduction, do it
  SFOREACH_PRODUCTION(reductions, prod) {
    int rhsLen = prod.data()->rhsLength();
    xassert(rhsLen >= 0);    // paranoia before using this to control recursion

    // in ordinary LR parsing, at this point we would pop 'rhsLen'
    // symbols off the stack, and push the LHS of this production.
    // here, we search down the stack for the node 'sibling' that
    // would be at the top after popping, and then tack on a new
    // StackNode after 'sibling' as the new top of stack

    // however, there might be more than one 'sibling' node, so we
    // must process all candidates.  the strategy will be to do a
    // simple depth-first search

    // WRONG:
      // (note that each such candidate defines a unique path -- all
      // paths must be composed of the same symbol sequence (namely the
      // RHS symbols), and if more than one such path existed it would
      // indicate a failure to collapse and share somewhere)

    // CORRECTION:
      // there *can* be more than one path to a given candidate; see
      // the comments at the top

    // step 1: collect all paths of length 'rhsLen' that start at
    // 'parser', via DFS
    PathCollectionState pcs(prod.data(), parser->state->id);
    collectReductionPaths(pcs, rhsLen, parser, mustUseLink);

    // invariant: poppedSymbols' length is equal to the recursion
    // depth in 'popStackSearch'; thus, should be empty now
    // update: since it's now an array, this is implicitly true
    //xassert(pcs.poppedSymbols.isEmpty());

    // terminology:
    //   - a 'reduction' is a grammar rule plus the TreeNode children
    //     that constitute the RHS
    //   - a 'path' is a reduction plus the parser (StackNode) that is
    //     at the end of the sibling link path (which is essentially the
    //     parser we're left with after "popping" the reduced symbols)

    // step 2: process those paths
    // ("mutate" because need non-const access to rpath->finalState)
    MUTATE_EACH_OBJLIST(PathCollectionState::ReductionPath, pcs.paths, pathIter) {
      PathCollectionState::ReductionPath *rpath = pathIter.data();

      // I'm not sure what is the best thing to call an 'action' ...
      actions++;

      // this is like shifting the reduction's LHS onto 'finalParser'
      glrShiftNonterminal(rpath->finalState, prod.data(), rpath->sval);
      
      // nullify 'sval' to mark it as consumed
      rpath->sval = NULL;
    } // for each path
  } // for each possible reduction

  return actions;
}


/*
 * collectReductionPaths():
 *   this function searches for all paths (over the
 *   sibling links) of a particular length, starting
 *   at currentNode
 *
 * pcs:
 *   various search state elements; see glr.h
 *
 * popsRemaining:
 *   number of links left to traverse before we've popped
 *   the correct number
 *
 * currentNode:
 *   where we are in the search; this call will consider the
 *   siblings of currentNode
 *
 * mustUseLink:
 *   a particular sibling link that must appear on all collected
 *   paths; it is NULL if we have no such requirement
 *
 * ([GLR] called this 'do-reductions' and 'do-limited-reductions')
 */
void GLR::collectReductionPaths(PathCollectionState &pcs, int popsRemaining,
                                StackNode *currentNode, SiblingLink *mustUseLink)
{
  // inefficiency: all this mechanism is somewhat overkill, given that
  // most of the time the reductions are unambiguous and unrestricted

  if (popsRemaining == 0) {
    // we've found a path of the required length

    // if we have failed to use the required link, ignore this path
    if (mustUseLink != NULL) {
      return;
    }

    if (tracingSys("parse")) {
      // profiling reported this used significant time even when not tracing
      trace("parse")
        << "state " << pcs.startStateId
        << ", reducing by " << pcs.production->toString(false /*printType*/)
        << ", back to state " << currentNode->state->id
        << endl;
    }

    // before calling the user, duplicate any needed values
    int rhsLen = pcs.production->rhsLength();
    SemanticValue *toPass = new SemanticValue[rhsLen];
    for (int i=0; i < rhsLen; i++) {
      SiblingLink *sib = pcs.siblings[i];

      // we're about to yield sib's 'sval' to the reduction action
      toPass[i] = sib->sval;

      // we inform the user, and the user responds with a value
      // to be kept in this sibling link *instead* of the passed
      // value; if this link yields a value in the future, it will
      // be this replacement
      sib->sval = duplicateSemanticValue(pcs.symbols[i], sib->sval);

      D(trace("sval") << "dup'd " << toPass[i] << " to pass, yielding "
                      << sib->sval << " to store" << endl);
    }

    // we've popped the required number of symbols; call the
    // user's code to synthesize a semantic value by combining them
    // (TREEBUILD)
    SemanticValue sval = userAct->doReductionAction(
                           pcs.production->prodIndex, toPass);
    D(trace("sval") << "reduced " << pcs.production->toString()
                    << ", yielding " << sval << endl);
    delete[] toPass;

    // see if the user wants to keep this reduction
    if (!userAct->keepNontermValue(pcs.production->left->ntIndex, sval)) {
      D(trace("sval") << "but the user decided not to keep it" << endl);
      return;
    }

    // and just collect this reduction, and the final state, in our
    // running list
    pcs.paths.append(new PathCollectionState::
      ReductionPath(currentNode, sval));
  }

  else {
    // explore currentNode's siblings
    MUTATE_EACH_OBJLIST(SiblingLink, currentNode->leftSiblings, sibling) {
      // the symbol on the sibling link is being popped;
      // TREEBUILD: we are collecting 'treeNode' for the purpose
      // of handing it to the user's reduction routine
      pcs.siblings[popsRemaining-1] = sibling.data();
      pcs.symbols[popsRemaining-1] = currentNode->getSymbolC();

      // recurse one level deeper, having traversed this link
      if (sibling.data() == mustUseLink) {
        // consumed the mustUseLink requirement
        collectReductionPaths(pcs, popsRemaining-1, sibling.data()->sib,
                              NULL /*mustUseLink*/);
      }
      else {
        // either no requirement, or we didn't consume it; just
        // propagate current requirement state
        collectReductionPaths(pcs, popsRemaining-1, sibling.data()->sib,
                              mustUseLink);
      }
    }
  }
}


#if 0
// temporary
void *mergeAlternativeParses(int ntIndex, void *left, void *right)
{
  cout << "merging at ntIndex " << ntIndex << ": "
       << left << " and " << right << endl;

  // just pick one arbitrarily
  return left;
}
#endif // 0


// shift reduction onto 'leftSibling' parser, 'prod' says which
// production we're using
// ([GLR] calls this 'reducer')
void GLR::glrShiftNonterminal(StackNode *leftSibling, Production const *prod,
                              SemanticValue /*owner*/ sval)
{
  // this is like a shift -- we need to know where to go
  ItemSet const *rightSiblingState =
    leftSibling->state->transitionC(prod->left);

  // debugging
  if (tracingSys("parse")) {
    trace("parse")
      << "state " << leftSibling->state->id
      << ", shift prductn " << prod->toString(false /*printType*/)
      << ", to state " << rightSiblingState->id
      << endl;
  }

  // is there already an active parser with this state?
  StackNode *rightSibling = findActiveParser(rightSiblingState);
  if (rightSibling) {
    // does it already have a sibling link to 'leftSibling'?
    MUTATE_EACH_OBJLIST(SiblingLink, rightSibling->leftSiblings, sibIter) {
      SiblingLink *sibLink = sibIter.data();

      // check this link for pointing at the right place
      if (sibLink->sib == leftSibling) {
        // we already have a sibling link, so we don't need to add one

        // +--------------------------------------------------+
        // | it is here that we are bringing the tops of two  |
        // | alternative parses together (TREEBUILD)          |
        // +--------------------------------------------------+
        // call the user's code to merge, and replace what we have
        // now with the merged version
        D(SemanticValue old = sibLink->sval);
        sibLink->sval = userAct->mergeAlternativeParses(prod->left->ntIndex,
                                                        sibLink->sval, sval);
        D(trace("sval") << "merged " << old << " and " << sval
                        << ", yielding " << sibLink->sval << endl);

        // ok, done
        return;

        // and since we didn't add a link, there is no potential for new
        // paths
      }
    }

    // we get here if there is no suitable sibling link already
    // existing; so add the link (and keep the ptr for loop below)
    SiblingLink *sibLink =
      rightSibling->addSiblingLink(leftSibling, sval);

    // adding a new sibling link may have introduced additional
    // opportunties to do reductions from parsers we thought
    // we were finished with.
    //
    // what's more, it's not just the parser ('rightSibling') we
    // added the link to -- if rightSibling's itemSet contains 'A ->
    // alpha . B beta' and B ->* empty (so A's itemSet also has 'B
    // -> .'), then we reduced it (if lookahead ok), so
    // 'rightSibling' now has another left sibling with 'A -> alpha
    // B . beta'.  We need to let this sibling re-try its reductions
    // also.
    //
    // so, the strategy is to let all 'finished' parsers re-try
    // reductions, and process those that actually use the just-
    // added link

    // for each 'finished' parser (i.e. those not still on
    // the worklist)
    SMUTATE_EACH_OBJLIST(StackNode, activeParsers, parser) {
      if (parserWorklist.contains(parser.data())) continue;

      // do any reduce actions that are now enabled
      doAllPossibleReductions(parser.data(), sibLink);
    }
  }

  else {
    // no, there is not already an active parser with this
    // state.  we must create one; it will become the right
    // sibling of 'leftSibling'
    rightSibling = makeStackNode(rightSiblingState);

    // add the sibling link (and keep ptr for tree stuff)
    rightSibling->addSiblingLink(leftSibling, sval);

    // since this is a new parser top, it needs to become a
    // member of the frontier
    activeParsers.append(rightSibling);
    rightSibling->incRefCt();
    parserWorklist.append(rightSibling);
    rightSibling->incRefCt();

    // no need for the elaborate re-checking above, since we
    // just created rightSibling, so no new opportunities
    // for reduction could have arisen
  }
}


void GLR::glrShiftTerminals(ObjList<PendingShift> &pendingShifts)
{
  // clear the active-parsers list; we rebuild it in this fn
  decParserList(activeParsers);
  activeParsers.removeAll();

  // foreach (leftSibling, newState) in pendingShifts
  MUTATE_EACH_OBJLIST(PendingShift, pendingShifts, pshift) {
    StackNode *leftSibling = pshift.data()->parser;
    ItemSet const *newState = pshift.data()->shiftDest;

    // debugging
    trace("parse")
      << "state " << leftSibling->state->id
      << ", shift token " << currentTokenClass->name
      << ", to state " << newState->id
      << endl;

    // if there's already a parser with this state
    StackNode *rightSibling = findActiveParser(newState);
    if (rightSibling != NULL) {
      // no need to create the node
    }

    else {
      // must make a new stack node
      rightSibling = makeStackNode(newState);

      // and add it to the active parsers
      activeParsers.append(rightSibling);
      rightSibling->incRefCt();
    }

    // either way, add the sibling link now
    D(trace("sval") << "grabbed token sval " << currentTokenValue << endl);
    rightSibling->addSiblingLink(leftSibling, currentTokenValue);
  }
}


// if an active parser is at 'state', return it; otherwise
// return NULL
StackNode *GLR::findActiveParser(ItemSet const *state)
{
  SMUTATE_EACH_OBJLIST(StackNode, activeParsers, parser) {
    if (parser.data()->state == state) {
      return parser.data();
    }
  }
  return NULL;
}


StackNode *GLR::makeStackNode(ItemSet const *state)
{
  StackNode *sn = new StackNode(nextStackNodeId++, currentTokenColumn,
                                state, userAct);
  return sn;
}


#if 0
SemanticValue GLR::getParseResult()
{
  // the final activeParser is the one that shifted the end-of-stream
  // marker, so we want its left sibling, since that will be the
  // reduction(s) to the start symbol
  SemanticValue sv =
    activeParsers.first()->                    // parser that shifted end-of-stream
      leftSiblings.first()->sib->              // parser that shifted start symbol
      leftSiblings.first()->                   // sibling link with start symbol
      sval;                                    // start symbol tree node

  return sv;
}    
#endif // 0


// ------------------ stuff for outputting raw graphs ------------------
#if 0   // disabled for now
// name for graphs (can't have any spaces in the name)
string stackNodeName(StackNode const *sn)
{     
  Symbol const *s = sn->getSymbolC();
  char const *symName = (s? s->name.pcharc() : "(null)");
  return stringb(sn->stackNodeId
              << ":col="  << sn->tokenColumn
              << ",st=" << sn->state->id
              << ",sym=" << symName);
}

// name for rules; 'rn' is the 'ruleNo'-th rule in 'sn'
// (again, no spaces allowed)
string reductionName(StackNode const *sn, int ruleNo, Reduction const *red)
{
  return stringb(sn->stackNodeId << "/" << ruleNo << ":"
              << replace(red->production->toString(), " ", "_"));
}                                                            


// this prints the graph in my java graph applet format, where
// nodes lines look like
//   n <name> <optional-desc>
// and edges look like
//   e <from> <to>
// unfortunately, the graph applet needs a bit of work before it
// is worthwhile to use this routinely (though it's great for
// quickly verifying a single (small) parse)
//
// however, it's worth noting that the text output is not entirely
// unreadable...
void GLR::writeParseGraph(char const *fname) const
{
  FILE *out = fopen(stringb("graphs/" << fname), "w");
  if (!out) {
    xsyserror("fopen", stringb("opening file `graphs/" << fname << "'"));
  }

  // header info
  fprintf(out, "# parse graph file: %s\n", fname);
  fprintf(out, "# automatically generated\n"
               "\n");

  #if 0    // can't do anymore because allStackNodes is gone ...
  // for each stack node
  FOREACH_OBJLIST(StackNode, allStackNodes, stackNodeIter) {
    StackNode const *stackNode = stackNodeIter.data();
    string myName = stackNodeName(stackNode);

    // visual delimiter
    fputs(stringb("\n# ------ node: " << myName << " ------\n"), out);

    // write info for the node itself
    fputs(stringb("n " << myName << "\n\n"), out);

    // for all sibling links
    int ruleNo=0;
    FOREACH_OBJLIST(SiblingLink, stackNode->leftSiblings, sibIter) {
      SiblingLink const *link = sibIter.data();

      // write the sibling link
      fputs(stringb("e " << myName << " "
                         << stackNodeName(link->sib) << "\n"), out);

      // ideally, we'd attach the reduction nodes directly to the
      // sibling edge.  however, since I haven't developed the
      // graph applet far enough for that, I'll instead attach it
      // to the stack node directly..

      if (link->treeNode->isNonterm()) {
        // for each reduction node
        FOREACH_OBJLIST(Reduction, link->treeNode->asNonterm().reductions,
                        redIter) {
          Reduction const *red = redIter.data();
          ruleNo++;

          string ruleName = reductionName(stackNode, ruleNo, red);

          // write info for the rule node
          fputs(stringb("n " << ruleName << "\n"), out);

          // put the link from the stack node to the rule node
          fputs(stringb("e " << myName << " " << ruleName << "\n"), out);

          // write all child links
          // ACK!  until my graph format is better, this is almost impossible
          #if 0
          SFOREACH_OBJLIST(StackNode, rule->children, child) {
            fputs(stringb("e " << ruleName << " "
                               << stackNodeName(child.data()) << "\n"), out);
          }
          #endif // 0

          // blank line for visual separation
          fputs("\n", out);
        } // for each reduction
      } // if is nonterminal
    } // for each sibling
  } // for each stack node
  #endif // 0

  // done
  if (fclose(out) != 0) {
    xsyserror("fclose");
  }
}
#endif // 0


// --------------------- testing ------------------------
// read an entire file into a single string
// currenty is *not* pipe-friendly because it must seek
// (candidate for adding to 'str' module)
string readFileIntoString(char const *fname)
{
  // open file
  FILE *fp = fopen(fname, "r");
  if (!fp) {
    xsyserror("fopen", stringb("opening `" << fname << "' for reading"));
  }

  // determine file's length
  if (fseek(fp, 0, SEEK_END) < 0) {
    xsyserror("fseek");
  }
  int len = (int)ftell(fp);      // conceivably problematic cast..
  if (len < 0) {
    xsyserror("ftell");
  }
  if (fseek(fp, 0, SEEK_SET) < 0) {
    xsyserror("fseek");
  }

  // allocate a sufficiently large buffer
  string ret(len);
  
  // read the file into that buffer
  if (fread(ret.pchar(), 1, len, fp) < (size_t)len) {
    xsyserror("fread");
  }

  // close file
  if (fclose(fp) < 0) {
    xsyserror("fclose");
  }

  // return the new string
  return ret;
}


bool GLR::glrParseFrontEnd(Lexer2 &lexer2, SemanticValue &treeTop,
                           char const *grammarFname, char const *inputFname, 
                           char const *symOfInterestName)
{
  #if 0
    // [ASU] grammar 4.19, p.222: demonstrating LR sets-of-items construction
    parseLine("E' ->  E $                ");
    parseLine("E  ->  E + T  |  T        ");
    parseLine("T  ->  T * F  |  F        ");
    parseLine("F  ->  ( E )  |  id       ");

    char const *input[] = {
      " id                 $",
      " id + id            $",
      " id * id            $",
      " id + id * id       $",
      " id * id + id       $",
      " ( id + id ) * id   $",
      " id + id + id       $",
      " id + ( id + id )   $"
    };
  #endif // 0/1

 #if 0
    // a simple ambiguous expression grammar
    parseLine("S  ->  E  $                  ");
    parseLine("E  ->  x  |  E + E  | E * E  ");

    char const *input[] = {
      "x + x * x $",
    };
  #endif // 0/1

  #if 0
    // my cast-problem grammar
    parseLine("Start -> Expr $");
    parseLine("Expr -> CastExpr");
    parseLine("Expr -> Expr + CastExpr");
    parseLine("CastExpr -> AtomExpr");
    parseLine("CastExpr -> ( Type ) CastExpr");
    parseLine("AtomExpr -> 3");
    parseLine("AtomExpr -> x");
    parseLine("AtomExpr -> ( Expr )");
    parseLine("AtomExpr -> AtomExpr ( Expr )");
    parseLine("Type -> int");
    parseLine("Type -> x");

    char const *input[] = {
      "( x ) ( x ) $",
    };

    printProductions(cout);
    runAnalyses();

    INTLOOP(i, 0, (int)TABLESIZE(input)) {
      cout << "------ parsing: `" << input[i] << "' -------\n";
      glrParse(input[i]);
    }
  #endif // 0/1

  #if 1
    // read grammar
    //if (!readFile(grammarFname)) {
    //  // error with grammar; message already printed
    //  return;
    //}

    if (!strstr(grammarFname, ".bin")) {
      // assume it's an ascii grammar file and do the whole thing

      // new code to read a grammar (throws exception on failure)
      readGrammarFile(*this, grammarFname);

      // spit grammar out as something bison might be able to parse
      //{
      //  ofstream bisonOut("bisongr.y");
      //  printAsBison(bisonOut);
      //}

      // prepare for symbol of interest
      if (symOfInterestName != NULL) {
        symOfInterest = findSymbolC(symOfInterestName);
        if (!symOfInterest) {
          cout << "warning: " << symOfInterestName << " isn't in the grammar\n";
        }
      }

      if (tracingSys("grammar")) {
        printProductions(trace("grammar") << endl);
      }

      runAnalyses();
    }

    else {
      // before using 'xfer' we have to tell it about the string table
      flattenStrTable = &grammarStringTable;

      // assume it's a binary grammar file and try to
      // read it in directly
      traceProgress() << "reading binary grammar file " << grammarFname << endl;
      BFlatten flat(grammarFname, true /*reading*/);
      xfer(flat);
    }


    // parse input
    return glrParseNamedFile(lexer2, treeTop, inputFname);

  #endif // 0/1
}


#ifdef GLR_MAIN

int main(int argc, char **argv)
{
  traceAddSys("progress");
  //traceAddSys("parse-tree");

  ParseTreeAndTokens tree;
  treeMain(tree, argc, argv);

  return 0;
}
#endif // GLR_MAIN
