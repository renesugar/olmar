// useract.h
// declarations for user-supplied reduction action functions;

// the code appears in the .cc file generated by 'gramanl' from
// an associated .gr file

// the comments below are guidelines on writing grammar actions, since
// those grammar actions are composed to form the single-entry
// functions documented below

#ifndef USERACT_H
#define USERACT_H


// user-supplied semantic values:
//  - semantic values are represented as void* since their type is
//    determined by the user; I use the name here instead of 'void*'
//    is they're a little easier to recognize by visual inspection;
// -  each occurrance of this type is marked as to whether it is
//    owner or serf; note that "owner" can mean reference-counted
typedef void *SemanticValue;

// user-supplied reduction actions
//  - production 'id' is being used to reduce
//  - 'svals' contains an array of semantic values yielded by the RHS
//    symbols, such that the 0th element is the leftmost RHS element;
//    the pointers in the array are owner pointers (the array ptr itself
//    is a serf)
//  - this fn returns the semantic value for the reduction; this return
//    value is an owner pointer
SemanticValue doReductionAction(int productionId, SemanticValue *svals);

// duplication of semantic values:
//  - the given 'sval' is about to be passed to a reduction action
//    function.  the user must return a value to be stored in place
//    of the old one, in case it is needed to pass to another action
//    function in case of local ambiguity; 'sval' is a serf
//  - the return value will be yielded (if necessary) to the next
//    consumer action function, and is an owner ptr
//  - possible strategies:
//    - return NULL, in which case it is probably an error for the
//      value to be passed to another action (i.e. the grammar needs
//      to be LALR(1) near this semantic value); in this case, 'del'
//      will not be called on the NULL value
//    - increment a reference count and return 'sval'
//    - do nothing, and rely on some higher-level allocation scheme
//      such as full GC, or regions
SemanticValue duplicateTerminalValue(int termId, SemanticValue sval);
SemanticValue duplicateNontermValue(int nontermId, SemanticValue sval);

// a semantic value didn't get passed to an action function, either
// because it was never used at all (e.g. a semantic value for a
// punctuator token, which the user can simply ignore), or because we
// duplicated it in anticipation of a possible local ambiguity, but
// then that parse turned out not to happen, so we're cancelling
// the dup now; 'sval' is an owner pointer
void deallocateTerminalValue(int termId, SemanticValue sval);
void deallocateNontermValue(int nontermId, SemanticValue sval);

// this is called when there are two interpretations for the same
// sequence of ground terminals, culminating in two different reductions
// deriving the same left-hand-side nonterminal (identified by 'ntIndex');
// it should return a value to be used in the place where they conflict'
// both 'left' and 'right' are owner pointers, and the return value
// is also an owner pointer
SemanticValue mergeAlternativeParses(int ntIndex, SemanticValue left,
                                     SemanticValue right);


#endif // USERACT_H
