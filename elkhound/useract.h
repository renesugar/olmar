// useract.h            see license.txt for copyright and terms of use
// interface to an object containing user-defined action functions

// the code appears in the .cc file generated by 'gramanl' from
// an associated .gr file

// the comments below are guidelines on writing grammar actions, since
// those grammar actions are composed to form the single-entry
// functions documented below

#ifndef USERACT_H
#define USERACT_H

#include "glrconfig.h"     // SOURCELOC
#include "str.h"           // string
#include "srcloc.h"        // SourceLoc

class ParseTables;         // parsetables.h

// user-supplied semantic values:
//  - Semantic values are an arbitrary word, that the user can then
//    use as a pointer or an integer or whatever.  The parser
//    generator inserts the appropriate casts, so the actual type
//    I use here shouldn't ever be visible to the user.
//  - Usually, SemanticValues that are used as pointers are considered
//    to be owner pointers, but only in the sense that del() will be
//    called.  It's up to the user to decide if del() actually does
//    anything.
typedef unsigned long SemanticValue;

// name of a null sval; can't use "NULL" because of __null weirdness in gcc-3...
#define NULL_SVAL 0


// package of functions; the user will create an instance of a class
// derived from this, and the parser will carry it along to invoke
// the various action functions
class UserActions {
public:
  // allow abstract user to delete
  virtual ~UserActions();

  // user-supplied reduction actions
  //  - production 'id' is being used to reduce
  //  - 'svals' contains an array of semantic values yielded by the RHS
  //    symbols, such that the 0th element is the leftmost RHS element;
  //    the pointers in the array are owner pointers (the array ptr itself
  //    is a serf)
  //  - 'loc' is the location of the left edge of the parse subtree
  //  - this fn returns the semantic value for the reduction; this return
  //    value is an owner pointer
  typedef SemanticValue (*ReductionActionFunc)(
    UserActions *context,         // parser context class object
    int productionId,             // production being used to reduce
    SemanticValue const *svals    // array of semantic values
    SOURCELOCARG( SourceLoc loc )
    ENDSOURCELOCARG( SourceLoc endloc ) );
                                                     
  // get the actual function; two-step to avoid virtual call in inner loop
  virtual ReductionActionFunc getReductionAction()=0;

  // duplication of semantic values:
  //  - the given 'sval' is about to be passed to a reduction action
  //    function.  the user must return a value to be stored in place
  //    of the old one, in case it is needed to pass to another action
  //    function in case of local ambiguity; 'sval' is a serf
  //  - the return value will be yielded (if necessary) to the next
  //    consumer action function, and is an owner ptr
  //  - some possible strategies:
  //    - return NULL, in which case it is probably an error for the
  //      value to be passed to another action (i.e. the grammar needs
  //      to be LALR(1) near this semantic value); in this case, 'del'
  //      will not be called on the NULL value
  //    - increment a reference count and return 'sval'
  //    - do nothing, and rely on some higher-level allocation scheme
  //      such as full GC, or regions
  virtual SemanticValue duplicateTerminalValue(
    int termId, SemanticValue sval)=0;
  virtual SemanticValue duplicateNontermValue(
    int nontermId, SemanticValue sval)=0;

  // a semantic value didn't get passed to an action function, either
  // because it was never used at all (e.g. a semantic value for a
  // punctuator token, which the user can simply ignore), or because we
  // duplicated it in anticipation of a possible local ambiguity, but
  // then that parse turned out not to happen, so we're cancelling
  // the dup now; 'sval' is an owner pointer
  virtual void deallocateTerminalValue(int termId, SemanticValue sval)=0;
  virtual void deallocateNontermValue(int nontermId, SemanticValue sval)=0;

  // this is called when there are two interpretations for the same
  // sequence of ground terminals, culminating in two different reductions
  // deriving the same left-hand-side nonterminal (identified by 'ntIndex');
  // it should return a value to be used in the place where they conflict'
  // both 'left' and 'right' are owner pointers, and the return value
  // is also an owner pointer
  //
  // NOTE: the 'left' value is always the node which came first, and
  // might even have been yielded to another reduction already
  // (depending on the grammar), whereas the 'right' value is always a
  // node which was just created, and has definitely *not* been
  // yielded to anything (this fact is critical to solving the general
  // yield-then-merge problem)
  virtual SemanticValue mergeAlternativeParses(
    int ntIndex, SemanticValue left, SemanticValue right
    SOURCELOCARG( SourceLoc loc )
  )=0;

  // after every reduction, the semantic value is passed to this function,
  // which returns 'false' if the reduction should be cancelled; if it
  // does return false, then 'sval' is an owner pointer (the parser engine
  // will drop the value on the floor)
  virtual bool keepNontermValue(int nontermId, SemanticValue sval)=0;

  // every time a token is pulled from the lexer, this reclassifier is
  // used to give the user a chance to reinterpret the token, before it
  // is used for reduction lookahead comparisons; it returns the
  // reclassified token type, or 'oldTokenType' to leave it unchanged
  typedef int (*ReclassifyFunc)(UserActions *ths, int oldTokenType, SemanticValue sval);

  // get the reclassifier
  virtual ReclassifyFunc getReclassifier()=0;

  // descriptions of symbols with their semantic values; this is useful
  // for the ACTION_TRACE function of the parser
  virtual string terminalDescription(int termId, SemanticValue sval)=0;
  virtual string nonterminalDescription(int nontermId, SemanticValue sval)=0;

  // get static names for all of the symbols
  virtual char const *terminalName(int termId)=0;
  virtual char const *nonterminalName(int termId)=0;

  // get the parse tables for this grammar; the default action
  // complains that no tables are defined
  virtual ParseTables *makeTables();
};


// for derived classes, the list of functions to be declared
// (this macro is used by the generated code)
#define USER_ACTION_FUNCTIONS                                          \
  virtual ReductionActionFunc getReductionAction();                    \
                                                                       \
  virtual SemanticValue duplicateTerminalValue(                        \
    int termId, SemanticValue sval);                                   \
  virtual SemanticValue duplicateNontermValue(                         \
    int nontermId, SemanticValue sval);                                \
                                                                       \
  virtual void deallocateTerminalValue(                                \
    int termId, SemanticValue sval);                                   \
  virtual void deallocateNontermValue(                                 \
    int nontermId, SemanticValue sval);                                \
                                                                       \
  virtual SemanticValue mergeAlternativeParses(                        \
    int ntIndex, SemanticValue left, SemanticValue right               \
    SOURCELOCARG( SourceLoc loc )                                      \
  );                                                                   \
                                                                       \
  virtual bool keepNontermValue(int nontermId, SemanticValue sval);    \
                                                                       \
  virtual ReclassifyFunc getReclassifier();                            \
                                                                       \
  virtual string terminalDescription(int termId, SemanticValue sval);  \
  virtual string nonterminalDescription(int nontermId, SemanticValue sval);  \
                                                                       \
  virtual char const *terminalName(int termId);                        \
  virtual char const *nonterminalName(int termId);


// a useraction class which has only trivial actions
class TrivialUserActions : public UserActions {
public:
  USER_ACTION_FUNCTIONS

  static SemanticValue doReductionAction(
    UserActions *ths,
    int productionId, SemanticValue const *svals
    SOURCELOCARG( SourceLoc loc ) 
    ENDSOURCELOCARG( SourceLoc ) );

  static int reclassifyToken(UserActions *ths, 
    int oldTokenType, SemanticValue sval);
};


#endif // USERACT_H
