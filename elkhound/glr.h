// glr.h
// GLR parsing algorithm

/*
 * Author: Scott McPeak, April 2000
 *
 * The fundamental concept in Generalized LR (GLR) parsing
 * is to permit (at least local) ambiguity by "forking" the
 * parse stack.  If the input is actually unambiguous, then
 * all but one of the forked parsers will, at some point,
 * fail to shift a symbol, and die.  If the input is truly
 * ambiguous, forked parsers rejoin at some point, and the
 * parse tree becomes a parse DAG, representing all possible
 * parses.  (In fact, since cyclic grammars are supported,
 * which can have an infinite number of parse trees for
 * some inputs, we may end up with a cyclic parse *graph*.)
 *
 * In the larger scheme of things, this level of support for
 * ambiguity is useful because it lets us use simpler and
 * more intuitive grammars, more sophisticated disambiguation
 * techniques, and parsing in the presence of incomplete
 * or incorrect information (e.g. in an editor).
 *
 * The downside is that parsing is slower, and whatever tool
 * processes the parse graph needs to have ways of dealing
 * with the multiple parse interpretations.
 *
 * references:
 *
 *   [GLR]  J. Rekers.  Parser Generation for Interactive
 *          Environments.  PhD thesis, University of
 *          Amsterdam, 1992.  Available by ftp from
 *          ftp://ftp.cwi.nl/pub/gipe/reports/Rek92.ps.Z .
 *          [Contains a good description of the Generalized
 *          LR (GLR) algorithm.]
 */

#ifndef __GLR_H
#define __GLR_H

#include "gramanl.h"     // basic grammar analyses, Grammar class, etc.
#include "owner.h"       // Owner
#include "rcptr.h"       // RCPtr
#include "useract.h"     // UserActions, SemanticValue
#include "objpool.h"     // ObjectPool


// fwds from other files
class Lexer2Token;       // lexer2.h
class Lexer2;            // lexer2.h

// forward decls for things declared below
class StackNode;         // unit of parse state
class SiblingLink;       // connections between stack nodes
class PendingShift;      // for postponing shifts.. may remove
class GLR;               // main class for GLR parsing


// a pointer from a stacknode to one 'below' it (in the LR
// parse stack sense); also has a link to the parse graph
// we're constructing
class SiblingLink {
public:
  // the stack node being pointed-at; it was created eariler
  // than the one doing the pointing
  RCPtr<StackNode> sib;

  // this is the semantic value associated with this link
  // (parse tree nodes are *not* associated with stack nodes --
  // that's now it was originally, but I figured out the hard
  // way that's wrong (more info in compiler.notes.txt));
  // this is an *owner* pointer
  SemanticValue sval;

  // the source location of the left edge of the subtree rooted
  // at this stack node; this is in essence part of the semantic
  // value, but automatically propagated by the parser
  SOURCELOC( SourceLocation loc; )

public:
  SiblingLink(StackNode *s, SemanticValue sv
              SOURCELOCARG( SourceLocation const &L) );
  ~SiblingLink();
};


// the GLR parse state is primarily made up of a graph of these
// nodes, which play a role analogous to the stack nodes of a
// normal LR parser; GLR nodes form a graph instead of a linear
// stack because choice points (real or potential ambiguities)
// are represented as multiple left-siblings
class StackNode {
public:
  // the LR state the parser is in when this node is at the
  // top ("at the top" means that nothing, besides perhaps itself,
  // is pointing to it)
  //ItemSet const * const state;                 // (serf)
  StateId state;       // now it is an id

  // each leftSibling points to a stack node in one possible LR stack.
  // if there is more than one, it means two or more LR stacks have
  // been joined at this point.  this is the parse-time representation
  // of ambiguity (actually, unambiguous grammars or inputs do
  // sometimes lead to multiple siblings)
  ObjList<SiblingLink> leftSiblings;           // this is a set

  // the *first* sibling is simply embedded directly into the
  // stack node, to avoid list overhead in the common case of
  // only one sibling; when firstSib.sib==NULL, there are no
  // siblings
  SiblingLink firstSib;

  // number of sibling links pointing at 'this', plus the number
  // of worklists on which 'this' appears (some liberty is taken
  // in the mini-LR parse, but it is carefully documented there)
  int referenceCount;

  // how many stack nodes can I pop before hitting a nondeterminism?
  // if this node itself has >1 sibling link, determinDepth==0; if
  // this node has 1 sibling, but that sibling has >1 sibling, then
  // determinDepth==1, and so on; if this node has 0 siblings, then
  // determinDepth==1
  int determinDepth;

  union {
    // somewhat nonideal: I need access to the 'userActions' to
    // deallocate semantic values when refCt hits zero, and I need
    // to map states to state-symbols for the same reason.
    // update: now I'm also using this to support pool-based
    // deallocation in decRefCt()
    GLR *glr;

    // this is used by the ObjectPool which handles allocation of
    // StackNodes
    StackNode *nextInFreeList;
  };

  // count and high-water for stack nodes
  static int numStackNodesAllocd;
  static int maxStackNodesAllocd;
                           
  
private:    // funcs
  SiblingLink *
    addAdditionalSiblingLink(StackNode *leftSib, SemanticValue sval
                             SOURCELOCARG( SourceLocation const &loc ) );

public:     // funcs
  StackNode();
  ~StackNode();

  // ctor/dtor from point of view of the object pool user
  void init(StateId state, GLR *glr);
  void deinit();

  // internal workings of 'deinit', exposed for performance reasons
  inline void decrementAllocCounter();
  void deallocSemanticValues();

  // add a new link with the given tree node; return the link
  SiblingLink *addSiblingLink(StackNode *leftSib, SemanticValue sval
                              SOURCELOCARG( SourceLocation const &loc ) );
                                
  // specialized version for performance-critical sections
  inline void
    addFirstSiblingLink_noRefCt(StackNode *leftSib, SemanticValue sval
                                SOURCELOCARG( SourceLocation const &loc ) );

  // return the symbol represented by this stack node;  it's
  // the symbol shifted or reduced-to to get to this state
  // (this used to be a data member, but there are at least
  // two ways to compute it, so there's no need to store it)
  SymbolId getSymbolC() const;

  // reference count stuff
  void incRefCt() { referenceCount++; }
  void decRefCt();

  // sibling count queries (each one answerable in constant time)
  bool hasZeroSiblings() const { return firstSib.sib==NULL; }
  bool hasOneSibling() const { return firstSib.sib!=NULL && leftSiblings.isEmpty(); }
  bool hasMultipleSiblings() const { return leftSiblings.isNotEmpty(); }

  // when you expect there's only one sibling link, get it this way
  SiblingLink const *getUniqueLinkC() const;
  SiblingLink *getUniqueLink() { return const_cast<SiblingLink*>(getUniqueLinkC()); }

  // retrieve pointer to the sibling link to a given node, or NULL if none
  SiblingLink *getLinkTo(StackNode *another);

  // recompute my determinDepth based on siblings, 
  // but don't actually change the state
  int computeDeterminDepth() const;

  // debugging
  static void printAllocStats();
  void checkLocalInvariants() const;
};


// during GLR parsing of a single token, we do all reductions before any
// shifts; thus, when we decide a shift is necessary, we need to save the
// relevant info somewhere so we can come back to it at the end
class PendingShift {
public:
  // which parser is this that's ready to shift?
  RCPtr<StackNode> parser;

  // which state is it ready to shift into?
  //ItemSet const * const shiftDest;            // (serf)
  StateId shiftDest;

public:
  PendingShift()
    : parser(NULL), shiftDest(STATE_INVALID) {}
  ~PendingShift();

  PendingShift& operator=(PendingShift const &obj);

  // start using this
  void init(StackNode *p, StateId s)
  {
    parser = p;
    shiftDest = s;
  }

  // stop using this
  void deinit();
};


// when a reduction is performed, we do a DFS to find all the ways
// the reduction can happen; this structure keeps state persistent
// across the recursion (comments in code have some details, too)
class PathCollectionState {
public:    // types
  // each particular reduction possibility is one of these
  class ReductionPath {
  public:
    // the state we end up in after reducing those symbols
    RCPtr<StackNode> finalState;

    // the semantic value yielded by the reduction action (owner)
    SemanticValue sval;

    // location of left edge of this subtree
    SOURCELOC( SourceLocation loc; )

  public:
    ReductionPath()
      : finalState(NULL), sval(NULL)  SOURCELOCARG( loc() ) {}
    ~ReductionPath();

    ReductionPath& operator=(ReductionPath const &obj);

    // begin using this
    void init(StackNode *f, SemanticValue s 
              SOURCELOCARG( SourceLocation const &L  ) )
    {
      finalState = f;        // increment reference count
      sval = s;
      SOURCELOC( loc = L; )
    }

    // stop using this
    void deinit();
  };

public:	   // data
  // ---- stuff that changes ----
  // id of the state we started in (where the reduction was applied)
  StateId startStateId;

  // we collect paths into this array, which is maintained as we
  // enter/leave recursion; the 0th item is the leftmost, i.e.
  // the last one we collect when starting from the reduction state
  // and popping symbols as we move left;
  GrowArray<SiblingLink*> siblings;     // (array of serfs)
  GrowArray<SymbolId> symbols;

  //SiblingLink **siblings;        // (owner ptr to array of serfs)
  //SymbolId *symbols;             // (owner ptr to array)

  // as reduction possibilities are encountered, we record them here
  ArrayStack<ReductionPath> paths;

  // ---- stuff constant across a series of DFSs ----
  // this is the (index of the) production we're trying to reduce by
  int prodIndex;

public:	   // funcs
  PathCollectionState();
  ~PathCollectionState();

  // begin using this object
  void init(int prodIndex, int rhsLen, StateId start);
                
  // done using it
  void deinit();
                  
  // add something to the 'paths' array
  void addReductionPath(StackNode *f, SemanticValue s
                        SOURCELOCARG( SourceLocation const &L ) );
};


// the GLR analyses are performed within this class; GLR
// differs from GrammarAnalysis in that the latter should have
// stuff useful across a wide range of possible analyses
class GLR : public GrammarAnalysis {
public:
  // user-specified actions
  UserActions *userAct;

  // ---- parser state between tokens ----
  // Every node in this set is (the top of) a parser that might
  // ultimately succeed to parse the input, or might reach a
  // point where it cannot proceed, and therefore dies.  (See
  // comments at top of glr.cc for more details.)
  ArrayStack<StackNode*> activeParsers;     // (refct list)

  // index: StateId -> index in 'activeParsers' of unique parser
  // with that state, or INDEX_NO_PARSER if none has that state
  typedef unsigned char ParserIndexEntry;
  enum { INDEX_NO_PARSER = 255 };
  ParserIndexEntry *parserIndex;            // (owner)

  // this is for assigning unique ids to stack nodes
  int nextStackNodeId;
  enum { initialStackNodeId = 1 };

  // ---- parser state during each token ----
  // the token we're trying to shift; any parser that fails to
  // shift this token (or reduce to one that can, recursively)
  // will "die"
  Terminal const *currentTokenClass;

  // the semantic value associated with that token
  SemanticValue currentTokenValue;

  // location of that current token
  SOURCELOC( SourceLocation currentTokenLoc; )

  // parsers that haven't yet had a chance to try to make progress
  // on this token
  ArrayStack<StackNode*> parserWorklist;    // (refct list)

  // ---- scratch space re-used at token-level (or finer) granularity ----
  // to be regarded as a local variable of GLR::doReduction; since
  // doReduction can call itself recursively (to handle new reductions
  // enabled by adding a sibling link), this is a stack
  ObjArrayStack<PathCollectionState> pcsStack;
  
  // pushing and popping using the ObjArrayStack interface would
  // defeat the purpose of pulling this out; pcsStackHeight gives the
  // next entry to use, and we only push a new entry onto pcsStack if
  // pcsStackHeight > pcsStack.length
  int pcsStackHeight;

  // to be regarded as a local variable of GLR::collectReductionPaths
  GrowArray<SemanticValue> toPass;

  // ---- allocation pools ----
  ObjectPool<StackNode> stackNodePool;

  // ---- debugging trace ----
  // these are computed during GLR::GLR since the profiler reports
  // there is significant expense to computing the debug strings
  // (that are then usually not printed)
  bool trParse;                             // tracingSys("parse")
  ostream &trsParse;                        // trace("parse")
  bool trSval;                              // tracingSys("sval")
  ostream &trsSval;                         // trace("sval")

private:    // funcs
  // comments in glr.cc
  SemanticValue duplicateSemanticValue(SymbolId sym, SemanticValue sval);
  void deallocateSemanticValue(SymbolId sym, SemanticValue sval);
  SemanticValue grabTopSval(StackNode *node);

  int glrParseAction(StackNode *parser, ActionEntry action,
                     ArrayStack<PendingShift> &pendingShifts);
  void doAllPossibleReductions(StackNode *parser, ActionEntry action,
                               SiblingLink *sibLink);
  void doReduction(StackNode *parser,
                   SiblingLink *mustUseLink,
                   int prodIndex);
  void collectReductionPaths(PathCollectionState &pcs, int popsRemaining,
                             StackNode *currentNode, SiblingLink *mustUseLink);
  void collectPathLink(PathCollectionState &pcs, int popsRemaining,
                       StackNode *currentNode, SiblingLink *mustUseLink,
                       SiblingLink *linkToAdd);
  void glrShiftNonterminal(StackNode *leftSibling, int lhsIndex,
                           SemanticValue sval
                           SOURCELOCARG( SourceLocation const &loc ) );
  void glrShiftTerminals(ArrayStack<PendingShift> &pendingShifts);
  StackNode *findActiveParser(StateId state);
  StackNode *makeStackNode(StateId state);
  void writeParseGraph(char const *input) const;
  void clearAllStackNodes();
  void addActiveParser(StackNode *parser);
  void pullFromActiveParsers(StackNode *parser);

public:     // funcs
  GLR(UserActions *userAct);
  ~GLR();

  // ------- primary interface -------
  // read the named grammar file (.bin extension, typically)
  void readBinaryGrammar(char const *grammarFname);

  // parse, using the token stream in 'lexer2', and store the final
  // semantic value in 'treeTop'
  bool glrParse(Lexer2 const &lexer2, SemanticValue &treeTop);

  // ------ oddball interfaces, mostly for testing -------
  // 'main' for testing this class; returns false on error;
  // yielded 'treeTop' is an owner (if a pointer)
  bool glrParseFrontEnd(Lexer2 &lexer2, SemanticValue &treeTop,
                        char const *grammarFname, char const *inputFname);

  // parse a given input file, using the passed lexer to lex it,
  // and store the result in 'treeTop'
  bool glrParseNamedFile(Lexer2 &lexer2, SemanticValue &treeTop,
                         char const *input);

  // after parsing, retrieve the toplevel semantic value
  // (obsolete now; the parsing function returns it)
  //SemanticValue getParseResult();
};


#endif // __GLR_H
