// grammar.h
// representation and algorithms for context-free grammars

#ifndef __GRAMMAR_H
#define __GRAMMAR_H

#include "str.h"       // string
#include "objlist.h"   // ObjList
#include "sobjlist.h"  // SObjList

// forward decls
class Bit2d;           // bit2d.h
class Terminal;        // below
class Nonterminal;     // below


// ---------------- Symbol --------------------
// either a nonterminal or terminal symbol
class Symbol {
public:
  // key attributes
  string const name;        // symbol's name in grammar
  bool const isTerm;        // true: terminal (only on right-hand sides of productions)
                            // false: nonterminal (can appear on left-hand sides)

public:
  Symbol(char const *n, bool t)
    : name(n), isTerm(t) {}
  virtual ~Symbol();

  // uniform selectors
  bool isTerminal() const { return isTerm; }
  bool isNonterminal() const { return !isTerm; }

  // casting
  Terminal const &asTerminalC() const;       // checks 'isTerminal' for cast safety
  Terminal &asTerminal()
    { return const_cast<Terminal&>(asTerminalC()); }

  Nonterminal const &asNonterminalC() const;
  Nonterminal &asNonterminal()
    { return const_cast<Nonterminal&>(asNonterminalC()); }

  // debugging
  virtual void print() const;    // print as '$name: isTerminal=$isTerminal' (no newline)
};

// I have several needs for serf lists of symbols, so let's use this for now
typedef SObjList<Symbol> SymbolList;
typedef SObjListIter<Symbol> SymbolListIter;
typedef SObjListMutator<Symbol> SymbolListMutator;

#define FOREACH_SYMBOL(list, iter) FOREACH_OBJLIST(Symbol, list, iter)
#define MUTATE_EACH_SYMBOL(list, iter) MUTATE_EACH_OBJLIST(Symbol, list, iter)
#define SFOREACH_SYMBOL(list, iter) SFOREACH_OBJLIST(Symbol, list, iter)
#define SMUTATE_EACH_SYMBOL(list, iter) SMUTATE_EACH_OBJLIST(Symbol, list, iter)


// ---------------- Terminal --------------------
// something that only appears on the right-hand side of
// productions, and is an element of the source language
class Terminal : public Symbol {
public:     // data
  // annotation done by algorithms
  int termIndex;	 // terminal index - this terminal's id

public:     // funcs
  Terminal(char const *name)
    : Symbol(name, true /*terminal*/),
      termIndex(-1) {}

  virtual void print() const;
};

typedef SObjList<Terminal> TerminalList;
typedef SObjListIter<Terminal> TerminalListIter;

#define FOREACH_TERMINAL(list, iter) FOREACH_OBJLIST(Terminal, list, iter)
#define MUTATE_EACH_TERMINAL(list, iter) MUTATE_EACH_OBJLIST(Terminal, list, iter)
#define SFOREACH_TERMINAL(list, iter) SFOREACH_OBJLIST(Terminal, list, iter)
#define SMUTATE_EACH_TERMINAL(list, iter) SMUTATE_EACH_OBJLIST(Terminal, list, iter)

// casting aggregates
ObjList<Symbol> const &toObjList(ObjList<Terminal> const &list)
  { return reinterpret_cast< ObjList<Symbol>const& >(list); }


// ---------------- Nonterminal --------------------
// something that can appear on the left-hand side of a production
// (or, emptyString, since we classify that as a nonterminal also)
class Nonterminal : public Symbol {
public:     // funcs
  // annotations used and computed by auxilliary algorithms
  int ntIndex;           // nonterminal index; see Grammar::computeWhatCanDeriveWhat
  bool cyclic;           // true if this can derive itself in 1 or more steps
  TerminalList first;    // set of terminals that can be start of a string derived from 'this'
  TerminalList follow;   // set of terminals that can follow a string derived from 'this'

public:     // funcs
  Nonterminal(char const *name);
  virtual ~Nonterminal();

  virtual void print() const;
};

typedef SObjList<Nonterminal> NonterminalList;
typedef SObjListIter<Nonterminal> NonterminalListIter;

#define FOREACH_NONTERMINAL(list, iter) FOREACH_OBJLIST(Nonterminal, list, iter)
#define MUTATE_EACH_NONTERMINAL(list, iter) MUTATE_EACH_OBJLIST(Nonterminal, list, iter)
#define SFOREACH_NONTERMINAL(list, iter) SFOREACH_OBJLIST(Nonterminal, list, iter)
#define SMUTATE_EACH_NONTERMINAL(list, iter) SMUTATE_EACH_OBJLIST(Nonterminal, list, iter)

// casting aggregates
ObjList<Symbol> const &toObjList(ObjList<Nonterminal> const &list)
  { return reinterpret_cast< ObjList<Symbol>const& >(list); }


// ---------------- Production --------------------
// a rewrite rule
class Production {
public:
  Nonterminal *left;            // (serf) left hand side; must be nonterminal
  SymbolList right;             // (serf) right hand side; terminals & nonterminals

public:
  Production(Nonterminal *L)
    : left(L) {}    // have to call append manually
  ~Production();

  // append a RHS symbol
  void append(Symbol *sym);

  // print to cout as 'A -> B c D' (no newline)
  void print() const;
};

typedef SObjList<Production> ProductionList;
typedef SObjListIter<Production> ProductionListIter;

#define FOREACH_PRODUCTION(list, iter) FOREACH_OBJLIST(Production, list, iter)
#define MUTATE_EACH_PRODUCTION(list, iter) MUTATE_EACH_OBJLIST(Production, list, iter)
#define SFOREACH_PRODUCTION(list, iter) SFOREACH_OBJLIST(Production, list, iter)
#define SMUTATE_EACH_PRODUCTION(list, iter) SMUTATE_EACH_OBJLIST(Production, list, iter)


// ---------------- DottedProduction --------------------
// a production, with an indicator that says how much of this
// production has been matched by some part of the input string
// (exactly which part of the input depends on where this appears
// in the algorithm's data structures)
class DottedProduction {
public:
  Production *prod;        // (serf) the base production
  int dot;                 // 0 means it's before all RHS symbols, 1 means after first, etc.

public:
  DottedProduction(Production *p, int d)
    : prod(p), dot(d) {}

  // print to cout as 'A -> B . c D' (no newline)
  void print() const;
};

// (serf) lists of dotted productions
typedef SObjList<DottedProduction> DProductionList;
typedef SObjListIter<DottedProduction> DProductionListIter;

#define FOREACH_DOTTEDPRODUCTION(list, iter) FOREACH_OBJLIST(DottedProduction, list, iter)
#define MUTATE_EACH_DOTTEDPRODUCTION(list, iter) MUTATE_EACH_OBJLIST(DottedProduction, list, iter)
#define SFOREACH_DOTTEDPRODUCTION(list, iter) SFOREACH_OBJLIST(DottedProduction, list, iter)
#define SMUTATE_EACH_DOTTEDPRODUCTION(list, iter) SMUTATE_EACH_OBJLIST(DottedProduction, list, iter)


// ---------------- Grammar --------------------
// represent a grammar: nonterminals, terminals, productions, and start-symbol
class Grammar {
  // ------ computed from key attributes --------
  // if entry i,j is true, then nonterminal i can derive nonterminal j
  // (this is a graph, represented (for now) as an adjacency matrix)
  enum { emptyStringIndex = 0 };
  Bit2d *derivable;                     // (owner)

  // indexing structures
  Nonterminal **indexedNonterms;        // (owner) ntTndex -> Nonterminal
  Terminal **indexedTerms;              // (owner) termIndex -> Terminal

  // only true after initializeAuxData has been called
  bool initialized;

public:
  // ---- key attributes ----
  ObjList<Nonterminal> nonterminals;    // (owner list)
  ObjList<Terminal> terminals;          // (owner list)
  ObjList<Production> productions;      // (owner list)
  Nonterminal *startSymbol;             // (serf) a particular nonterminal

  // ---- static ----
  // the special terminal for the empty string; does not appear in
  // the list of nonterminals or terminals for a grammar, but can
  // be referenced by productions, etc.
  Nonterminal emptyString;

  // ---- computed from the above ----
  // every production, with a dot in every possible place
  ObjList<DottedProduction> dottedProductions;

  // true if any nonterminal can derive itself in 1 or more steps
  bool cyclic;

private:
  // ------- symbol access ------------
  #define SYMBOL_ACCESS(Thing) 	       	       	       	    \
    /* retrieve, return NULL if not there */		    \
    Thing const *find##Thing##C(char const *name) const;    \
    Thing *find##Thing(char const *name)		    \
      { return const_cast<Thing*>(find##Thing##C(name)); }  \
							    \
    /* retrieve, or create it if not already there */	    \
    Thing *getOrMake##Thing(char const *name);

  SYMBOL_ACCESS(Symbol)	       // findSymbolC, findSymbol, getOrMakeSymbol
  SYMBOL_ACCESS(Terminal)      //   likewise
  SYMBOL_ACCESS(Nonterminal)   //   ..

  // -------- analyis init ---------
  // call this after grammar is completely built
  void initializeAuxData();

  // -------- deriability -------------
  // iteratively compute every pair A,B such that A can derive B
  void computeWhatCanDeriveWhat();
  void initDerivableRelation();

  // add a derivability relation; returns true if this makes a change
  bool addDerivable(Nonterminal const *left, Nonterminal const *right);
  bool addDerivable(int leftNtIndex, int rightNtIndex);

  // private deriability interface
  bool canDerive(int leftNtIndex, int rightNtIndex) const;
  bool sequenceCanDeriveEmpty(SymbolList const &list) const;
  bool iterSeqCanDeriveEmpty(SymbolListIter iter) const;

  // ---------- First --------------
  void computeFirst();
  bool addFirst(Nonterminal *NT, Terminal *term);
  void firstOfSequence(TerminalList &destList, SymbolList &sequence);
  void firstOfIterSeq(TerminalList &destList, SymbolListMutator sym);

  // ---------- Follow -------------
  void computeFollow();
  bool addFollow(Nonterminal *NT, Terminal *term);

  // misc
  void computePredictiveParsingTable();


public:
  Grammar();                            // set everything manually (except emptyString)
  ~Grammar();

  // essentially, my 'main()' while experimenting
  void exampleGrammar();


  // --------- building a grammar ---------
  // add a new production; the rhs arg list must be terminated with a NULL
  void addProduction(Nonterminal *lhs, Symbol *rhs, ...);

  // add a pre-constructed production
  void addProduction(Production *prod);

  // print the current list of productions
  void printProductions() const;


  // -------------- grammar parsing -------------------
  void readFile(char const *fname);

  // parse a line like "LHS -> R1 R2 R3", return false on parse error
  bool parseLine(char const *grammarLine);


  // ----------------- grammar queries -----------------------
  int numTerminals() const;
  int numNonterminals() const;

  bool canDerive(Nonterminal const *lhs, Nonterminal const *rhs) const;
  bool canDeriveEmpty(Nonterminal const *lhs) const;

  bool firstIncludes(Nonterminal const *NT, Terminal const *term) const;
  bool followIncludes(Nonterminal const *NT, Terminal const *term) const;


  #if 0
  // ---------------- grammar algorithms --------------------
  // true if the lhs can be rewritten as the rhs in zero
  // or more rewrite steps (the list versions are symbol
  // sequences to be matched)
  // [I need algorithms for these.. maybe dragon book?]
  bool canDerive(Symbol const *lhs, SymbolList const &rhs) const;
  bool canDerive(SymbolList const &lhs, Symbol const *rhs) const;
  bool canDerive(SymbolList const &lhs, SymbolList const &rhs) const;


  // we move the dot across all the symbols in 'symbols' (or the set of
  //   lhs symbols in 'symProds' with dots at the rightmost position),
  //   and across any symbols that can derive emptyString
  // this is the x operator in [GHR80]
  void advanceDot(DProductionList &final,             // out
                  DProductionList const &initial,
                  SymbolList const &symbols) const;
  void advanceDot(DProductionList &final,             // out
                  DProductionList const &initial,
                  DProductionList const &symProds) const;

  // similar to advanceDot, but we also advance past symbols that can
  //   derive symbols in 'symbols' (or lhs of 'symProds' with dots in
  //   rightmost position)
  // this is the * operator in [GHR80]
  void advanceDotStar(DProductionList &final,         // out
                      DProductionList const &initial,
                      SymbolList const &symbols) const;
  void advanceDotStar(DProductionList &final,         // out
                      DProductionList const &initial,
                      DProductionList const &symProds) const;

  // yield those nonterminals (as dotted productions with the dot moved
  //   beyond any initial nonterminals that can yield emptyString) that
  //   can be the first nonterminal of a string derived from something in
  //   'symbols' (or those nonterminals that follow dots in 'symProds')
  // this is the PREDICT function in [GHR80]
  void predict(DProductionList &prods,                // out
               SymbolList const &symbols) const;
  void predict(DProductionList &prods,                // out
               DProductionList const &symProds) const;

  #endif // 0
};


#endif // __GRAMMAR_H

