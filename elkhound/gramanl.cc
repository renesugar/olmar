// gramanl.cc
// code for gramanl.h

#include "gramanl.h"     // this module

#include "bit2d.h"       // Bit2d
#include "strtokp.h"     // StrtokParse
#include "syserr.h"      // xsyserror
#include "trace.h"       // tracing system
#include "nonport.h"     // getMilliseconds
#include "crc.h"         // crc32
#include "flatutil.h"    // Flatten, xfer helpers
#include "grampar.h"     // readGrammarFile
#include "emitcode.h"    // EmitCode
#include "strutil.h"     // replace
#include "ckheap.h"      // numMallocCalls

#include <fstream.h>     // ofstream
#include <stdlib.h>      // getenv
#include <stdio.h>       // printf


// for now, we'll just have these be global variables; if I later
// decide I actually want more than one at a time, I can move these
// into GrammarAnalysis and push the interfaces to accomodate

// NOTE: only LALR(1) has been recently tested; in particular I
// know that LR(1) is broken (3/26/02)

// LR(0) does all reductions, regardless of what the next token is
static bool const LR0 = false;

// SLR(1) looks at a production's LHS's Follow
static bool const SLR1 = false;

// LR(1) computes context-sensitive follow for each item,
// depending on how that item arises in the item-set DFA
static bool const LR1 = false;

// LALR(1) is like LR(1), except two states are merged if
// they only differ in their items' lookaheads (so it has
// the same # of states as SLR(1), while having some of the
// context-sensitivity of LR(1))
static bool const LALR1 = true;


#if !defined(NDEBUG)     // track unauthorized malloc's
  #define TRACK_MALLOC
#endif

#ifdef TRACK_MALLOC
  // take initial snapsot
  #define INITIAL_MALLOC_STATS() \
    unsigned mallocCt = numMallocCalls();

  // nothing should have been allocated recently; if it has, then
  // print a warning
  #define CHECK_MALLOC_STATS(desc)                                              \
    {                                                                           \
      unsigned newCt = numMallocCalls();                                        \
      if (mallocCt != newCt) {                                                  \
        cout << (newCt - mallocCt) << " malloc calls during " << desc << endl;  \
        mallocCt = newCt;                                                       \
        breaker();                                                              \
      }                                                                         \
    }

  // some unavoidable allocation just happened, so just update counter
  #define UPDATE_MALLOC_STATS() \
    mallocCt = numMallocCalls();
#else
  #define INITIAL_MALLOC_STATS()
  #define CHECK_MALLOC_STATS(desc)
  #define UPDATE_MALLOC_STATS()
#endif


// ----------------- DottedProduction ------------------
#if 0    // used?
DottedProduction::DottedProduction(DottedProduction const &obj)
{
  prod = obj.prod;
  dot = obj.dot;
  afterDot = obj.afterDot;
  firstSet = obj.firstSet;
  canDeriveEmpty = obj.canDeriveEmpty;
}                
#endif // 0


DottedProduction::DottedProduction()
{
  init();
}

void DottedProduction::init()
{
  prod = NULL;
  dot = -1;
  afterDot = NULL;
  canDeriveEmpty = false;
  backPointer = NULL;
}


DottedProduction::~DottedProduction()
{}


#if 0
DottedProduction::DottedProduction(GrammarAnalysis const &g)
  : lookahead(g.numTerminals())
{
  init();
}

DottedProduction::DottedProduction(GrammarAnalysis const &g, Production *p, int d)
  : lookahead(g.numTerminals())
{
  init();
  setProdAndDot(p, d);
}


DottedProduction::DottedProduction(Flatten &flat)
  : prod(NULL),
    lookahead(flat)
{}

void DottedProduction::xfer(Flatten &flat)
{
  flat.xferInt(dot);
  lookahead.xfer(flat);
}


void DottedProduction::xferSerfs(Flatten &flat, GrammarAnalysis &g)
{
  xferSerfPtrToList(flat, prod, g.productions);

  // set 'afterDot'
  setProdAndDot(prod, dot);
}
#endif // 0


// arbitrary integer unique to every symbol and preserved
// across read/write
int symbolIndex(Symbol const *s)
{
  if (s->isTerminal()) {
    // make terminals negative since otherwise they'd
    // collide with nonterminals
    return -( s->asTerminalC().termIndex );
  }
  else {
    return s->asNonterminalC().ntIndex;
  }
}


#if 0
bool DottedProduction::isEqual(DottedProduction const &obj) const
{
  return dot == obj.dot &&
         prod == obj.prod;
}
#endif // 0


void DottedProduction::setProdAndDot(Production const *p, int d)
{
  prod = p;
  dot = d;

  // computing this each time turned out to be significant
  // according to the profiler, so we store it instead
  bool dotAtEnd = (dot == prod->rhsLength());
  afterDot = dotAtEnd? NULL : prod->right.nthC(dot)->sym;
}

Symbol const *DottedProduction::symbolBeforeDotC() const
{
  xassert(!isDotAtStart());
  return prod->right.nthC(dot-1)->sym;
}

#if 0
Symbol const *DottedProduction::symbolAfterDotC() const
{
  xassert(!isDotAtEnd());
  return prod->right.nthC(dot)->sym;
}
#endif // 0


void DottedProduction::print(ostream &os) const
{
  os << prod->left->name << " ->";

  int position = 0;
  for (ObjListIter<Production::RHSElt> iter(prod->right);
       !iter.isDone(); iter.adv(), position++) {
    if (position == dot) {
      os << " .";
    }
    os << " " << iter.data()->sym->toString();
  }
  if (position == dot) {
    os << " .";
  }
}


// ---------------------- LRItem -------------------
LRItem::LRItem(int numTerms, DottedProduction const *dp)
  : dprod(dp),
    lookahead(numTerms)
{}

LRItem::LRItem(LRItem const &obj)
  : dprod(obj.dprod),
    lookahead(obj.lookahead)
{}

LRItem::~LRItem()
{}

LRItem::LRItem(Flatten &flat)
  : dprod(NULL),
    lookahead(flat)
{}

void LRItem::xfer(Flatten &flat)
{
  lookahead.xfer(flat);
}

void LRItem::xferSerfs(Flatten &flat, GrammarAnalysis &g)
{
  if (flat.writing()) {
    flat.writeInt(prodIndex());
    flat.writeInt(getDot());
  }
  else {
    // originally had these directly in the argument list,
    // but order of eval is undefined!
    int idx = flat.readInt();
    int d = flat.readInt();
    dprod = g.getDProdIndex(idx, d);
  }
}


// compare two items in an arbitrary (but deterministic) way so that
// sorting will always put a list of items into the same order, for
// comparison purposes; this doesn't consider the lookahead
STATICDEF int LRItem::diff(LRItem const *a, LRItem const *b, void*)
{
  // check the prodIndex first
  int ret = a->prodIndex() - b->prodIndex();
  if (ret) { return ret; }

  // 'dot'
  ret = a->getDot() - b->getDot();
  return ret;
  
  #if 0    // old stuff
  if (ret) { return ret; }

  Production const *aProd = a->prod;
  Production const *bProd = b->prod;
  if (aProd == bProd) {
    // equal dotted productions
    return 0;
  }

  // LHS index
  ret = aProd->left->ntIndex - bProd->left->ntIndex;
  if (ret) { return ret; }

  // RHS indices
  RHSEltListIter aIter(aProd->right);
  RHSEltListIter bIter(bProd->right);

  while (!aIter.isDone() && !bIter.isDone()) {
    ret = symbolIndex(aIter.data()->sym) - symbolIndex(bIter.data()->sym);
    if (ret) { return ret; }

    aIter.adv();
    bIter.adv();
  }

  if (aIter.isDone() && !bIter.isDone()) {
    return -1;
  }
  if (!aIter.isDone() && bIter.isDone()) {
    return 1;
  }

  // needs a grammar to print them ...
  //cout << "a: "; a->print(cout);
  //cout << "\nb: "; b->print(cout);
  //cout << endl;

  // one way this can be caused is if the grammar input file actually
  // has the same production listed twice
  xfailure("two productions with diff addrs are equal!");
  #endif // 0
}


void LRItem::print(ostream &os, GrammarAnalysis const &g) const
{
  dprod->print(os);
  lookahead.print(os, g);      // prints the separating comma, if necessary
}


// ----------------- ItemSet -------------------
ItemSet::ItemSet(StateId anId, int numTerms, int numNonterms)
  : kernelItems(),
    nonkernelItems(),
    termTransition(NULL),      // inited below
    nontermTransition(NULL),   // inited below
    terms(numTerms),
    nonterms(numNonterms),
    dotsAtEnd(NULL),
    numDotsAtEnd(0),
    stateSymbol(NULL),
    id(anId),
    BFSparent(NULL)
{
  allocateTransitionFunction();
}

void ItemSet::allocateTransitionFunction()
{
  termTransition = new ItemSet* [terms];
  nontermTransition = new ItemSet* [nonterms];

  INTLOOP(t, 0, terms) {
    termTransition[t] = (ItemSet*)NULL;      // means no transition on t
  }
  INTLOOP(n, 0, nonterms) {
    nontermTransition[n] = (ItemSet*)NULL;
  }
}


ItemSet::~ItemSet()
{
  delete[] termTransition;
  delete[] nontermTransition;

  if (dotsAtEnd) {
    delete[] dotsAtEnd;
  }
}


ItemSet::ItemSet(Flatten &flat)
  : termTransition(NULL),
    nontermTransition(NULL),
    dotsAtEnd(NULL),
    numDotsAtEnd(0),
    stateSymbol(NULL),
    BFSparent(NULL)
{}


Production *getNthProduction(Grammar *g, int n)
{
  if (0 <= n && n < g->productions.count()) {
    return g->productions.nth(n);
  }
  else {
    // my access path functions' contract is to
    // return NULL on any error (as opposed to, say,
    // an exception or assertion failure); this serves two
    // purposes:
    //   - the writing code can use it to determine the
    //     maximum value of 'n'
    //   - the reading code can use it to validate 'n',
    //     since that comes from the input file
    return NULL;
  }
}

#if 0    // not needed, doesn't work
DottedProduction *getNthDottedProduction(Production *p, int n)
{
  if (0 <= n && n < (p->rhsLength() + 1)) {
    return p->getDProd(n);
  }
  else {
    return NULL;
  }                                 
}
#endif // 0


void ItemSet::xfer(Flatten &flat)
{
  xferObjList(flat, kernelItems);
  xferObjList(flat, nonkernelItems);

  flat.xferInt(terms);
  flat.xferInt(nonterms);

  // numDotsAtEnd and kernelItemsCRC are computed from
  // other data
  // NEW: but computing them requires the items, which I'm omitting

  flat.xferInt(numDotsAtEnd);
  flat.xferLong((long&)kernelItemsCRC);

  flat.xferInt((int&)id);
}


int ticksComputeNonkernel = 0;

void ItemSet::xferSerfs(Flatten &flat, GrammarAnalysis &g)
{
  // xfer the 'prod' fields of the items
  {
    MUTATE_EACH_OBJLIST(LRItem, kernelItems, k) {
      k.data()->xferSerfs(flat, g);
    }
    MUTATE_EACH_OBJLIST(LRItem, nonkernelItems, n) {
      n.data()->xferSerfs(flat, g);
    }
  }


  #if 0
    // 'kernelItems' and 'nonkernelItems': each one accessed as
    //   g.productions.nth(???)->getDProd(???)
    xferSObjList_twoLevelAccess(
      flat,
      kernelItems,               // serf list
      static_cast<Grammar*>(&g), // root of access path
      getNthProduction,          // first access path link
      getNthDottedProduction);   // second access path link

    #if 1
      xferSObjList_twoLevelAccess(
        flat,
        nonkernelItems,            // serf list
        static_cast<Grammar*>(&g), // root of access path
        getNthProduction,          // first access path link
        getNthDottedProduction);   // second access path link
    #else
      // instead of the above, let's try computing the nonkernel items
      if (flat.reading()) {
        int start = getMilliseconds();
        g.itemSetClosure(*this);
        ticksComputeNonkernel += (getMilliseconds() - start);
      }
    #endif
  #endif // 0

  // these need to be sorted for 'changedItems'; but since
  // we're sorting by *address*, that's not necessarily
  // preserved across read/write
  // NEW: it should be stable now
  //kernelItems.insertionSort(LRItem::diff);


  // transition functions
  if (flat.reading()) {
    allocateTransitionFunction();
  }
  INTLOOP(t, 0, terms) {
    //xferNullableSerfPtrToList(flat, termTransition[t], g.itemSets);
    xferNullableSerfPtr(flat, termTransition[t]);
  }
  INTLOOP(n, 0, nonterms) {
    //xferNullableSerfPtrToList(flat, nontermTransition[n], g.itemSets);
    xferNullableSerfPtr(flat, nontermTransition[n]);
  }


  // dotsAtEnd, numDotsAtEnd, kernelItemsCRC
  //if (flat.reading()) {
  //  changedItems();
  //}
                         
  if (flat.reading()) {
    dotsAtEnd = new LRItem const * [numDotsAtEnd];
  }
  INTLOOP(p, 0, numDotsAtEnd) {
    #if 0
    xferSerfPtr_twoLevelAccess(
      flat,
      const_cast<LRItem*&>(dotsAtEnd[p]),   // serf
      static_cast<Grammar*>(&g), // root of access path
      getNthProduction,          // first access path link
      getNthDottedProduction);   // second access path link
    #endif // 0
    xferSerfPtr(flat, dotsAtEnd[p]);
  }

  xferNullableSerfPtr(flat, stateSymbol);

  xferNullableSerfPtrToList(flat, BFSparent, g.itemSets);
}


Symbol const *ItemSet::computeStateSymbolC() const
{
  // need only check kernel items since all nonkernel items
  // have their dots at the left side
  FOREACH_OBJLIST(LRItem, kernelItems, item) {
    if (! item.data()->isDotAtStart() ) {
      return item.data()->symbolBeforeDotC();
    }
  }
  return NULL;
}


int ItemSet::bcheckTerm(int index)
{
  xassert(0 <= index && index < terms);
  return index;
}

int ItemSet::bcheckNonterm(int index)
{
  xassert(0 <= index && index < nonterms);
  return index;
}

ItemSet *&ItemSet::refTransition(Symbol const *sym)
{
  if (sym->isTerminal()) {
    Terminal const &t = sym->asTerminalC();
    return termTransition[bcheckTerm(t.termIndex)];
  }
  else {
    Nonterminal const &nt = sym->asNonterminalC();
    return nontermTransition[bcheckNonterm(nt.ntIndex)];
  }
}


ItemSet const *ItemSet::transitionC(Symbol const *sym) const
{
  return const_cast<ItemSet*>(this)->refTransition(sym);
}


void ItemSet::setTransition(Symbol const *sym, ItemSet *dest)
{
  refTransition(sym) = dest;
}


void ItemSet::removeShift(Terminal const *sym)
{
  refTransition(sym) = NULL;
}


void ItemSet::addKernelItem(LRItem *item)
{
  // add it
  kernelItems.appendUnique(item);
}


void ItemSet::sortKernelItems()
{
  // sort the items to facilitate equality checks
  kernelItems.mergeSort(LRItem::diff);

  // note: the caller must call changedItems
}


bool ItemSet::operator==(ItemSet const &obj) const
{
  // since common case is disequality, check the
  // CRCs first, and only do full check if they
  // match
  if (kernelItemsCRC == obj.kernelItemsCRC) {                 
    // since nonkernel items are entirely determined by kernel
    // items, and kernel items are sorted, it's sufficient to
    // check for kernel list equality
    // OLD: when pointer equality was sufficient
    //   return kernelItems.equalAsPointerLists(obj.kernelItems);
    // NEW: use deep equality check
    return kernelItems.equalAsLists(obj.kernelItems, LRItem::diff);
  }
  else {
    // can't possibly be equal if CRCs differ
    return false;
  }
}


void ItemSet::addNonkernelItem(LRItem *item)
{
  nonkernelItems.appendUnique(item);
  
  // note: the caller is supposed to call changedItems
}


void ItemSet::removeReduce(Production const *prod, Terminal const *sym)
{                       
  MUTATE_EACH_OBJLIST(LRItem, kernelItems, k) {
    if (k.data()->isDotAtEnd() &&
        k.data()->getProd() == prod) {
      k.data()->laRemove(sym->termIndex);
    }
  }

  MUTATE_EACH_OBJLIST(LRItem, nonkernelItems, n) {
    if (n.data()->isDotAtEnd() &&
        n.data()->getProd() == prod) {
      n.data()->laRemove(sym->termIndex);
    }
  }

  #if 0
  ObjListMutator<LRItem> k(kernelItems);
  while (!k.isDone()) {
    if (k.data()->isDotAtEnd() &&
        k.data()->getProd() == prod) {
      k.deleteIt();
    }
    else {
      k.adv();
    }
  }

  changedItems();
  #endif // 0
}


void ItemSet::getAllItems(SObjList<LRItem> &dest) const
{
  SObjListMutator<LRItem> mut(dest);

  FOREACH_OBJLIST(LRItem, kernelItems, k) {
    mut.append(const_cast<LRItem*>(k.data()));
  }
  FOREACH_OBJLIST(LRItem, nonkernelItems, n) {
    mut.append(const_cast<LRItem*>(n.data()));
  }
}


STATICDEF int ItemSet::diffById(ItemSet const *left, ItemSet const *right, void*)
{
  return left->id - right->id;
}


void ItemSet::throwAwayItems()
{
  // can't delete the whole lists because I need the
  // reductions; among other things, 'dotsAtEnd' refers to them
  deleteNonReductions(kernelItems);
  deleteNonReductions(nonkernelItems);
}

void ItemSet::deleteNonReductions(ObjList<LRItem> &list)
{
  ObjListMutator<LRItem> mut(list);
  while (!mut.isDone()) {
    if (mut.data()->isDotAtEnd()) {
      // keep it
      mut.adv();
    }
    else {
      // trash it
      mut.deleteIt();     // also advances
    }
  }
}


// return the reductions that are ready in this state, given
// that the next symbol is 'lookahead'
void ItemSet::getPossibleReductions(ProductionList &reductions,
                                    Terminal const *lookahead,
                                    bool parsing) const
{
  // for each item with dot at end
  loopi(numDotsAtEnd) {
    LRItem const *item = dotsAtEnd[i];

    if (LR0) {
      // don't check the lookahead
    }
    else if (SLR1) {
      // the follow of its LHS must include 'lookahead'
      if (!item->getProd()->left->follow.contains(lookahead->termIndex)) {    // (constness)
        if (parsing && tracingSys("parse")) {
          trace("parse") << "state " << id
                         << ", not reducing by " 
                         << item->getProd()->toString(false /*printType*/)
                         << " because " << lookahead->toString()
                         << " is not in follow of "
                         << item->getProd()->left->name << endl;
        }
        continue;
      }
    }
    else if (LALR1 || LR1) {
      // the item's lookahead must include 'lookahead'
      if (!item->laContains(lookahead->termIndex)) {
        if (parsing && tracingSys("parse")) {
          trace("parse") << "state " << id
                         << ", not reducing by "
                         << item->getProd()->toString(false /*printType*/)
                         << " because " << lookahead->toString()
                         << " is not in lookahead" << endl;
        }
        continue;
      }
    }
    else {
      xfailure("no LR variant specified?");
    }

    // ok, this one's ready
    reductions.append(const_cast<Production*>(item->getProd()));       // (constness)
  }
}


bool ItemSet::mergeLookaheadsInto(ItemSet &dest) const
{        
  // will return true if any changes made
  bool changes = false;

  // iterate over both kernel lists simultaneously
  ObjListIter<LRItem> srcIter(kernelItems);
  ObjListMutator<LRItem> destIter(dest.kernelItems);
  while (!srcIter.isDone() && !destIter.isDone()) {
    LRItem const &srcItem = *(srcIter.data());
    LRItem &destItem = *(destIter.data());

    // the caller should already have established equality of the
    // non-lookahead components of the kernel items
    xassert(srcItem.equalNoLA(destItem));

    // merge lookaheads
    if (destItem.laMerge(srcItem)) {
      changes = true;
    }
    
    srcIter.adv();
    destIter.adv();
  }

  // kernel list lengths are supposed to be the same
  xassert(srcIter.isDone() && destIter.isDone());

  return changes;
}


Production const *ItemSet::getFirstReduction() const
{
  xassert(numDotsAtEnd >= 1);
  return dotsAtEnd[0]->getProd();
}


void ItemSet::changedItems()
{
  // -- recompute dotsAtEnd --
  // collect all items
  SObjList<LRItem> items;      // (constness) 'items' shouldn't be used to modify the elements
  getAllItems(items);

  // count number with dots at end
  int count = 0;
  {
    SFOREACH_OBJLIST(LRItem, items, itemIter) {
      LRItem const *item = itemIter.data();

      if (item->isDotAtEnd()) {
        count++;
      }
    }
  }

  // get array of right size
  if (dotsAtEnd  &&  count == numDotsAtEnd) {
    // no need to reallocate, already correct size
  }
  else {
    // throw old away
    if (dotsAtEnd) {
      delete[] dotsAtEnd;
    }

    // allocate new array
    numDotsAtEnd = count;
    dotsAtEnd = new LRItem const * [numDotsAtEnd];
  }

  // fill array
  int index = 0;
  SFOREACH_OBJLIST(LRItem, items, itemIter) {
    LRItem const *item = itemIter.data();

    if (item->isDotAtEnd()) {
      dotsAtEnd[index] = item;
      index++;
    }
  }

  // verify both loops executed same number of times
  xassert(index == count);

  // compute CRC; in this function, I just allocate here since this
  // function is already allocation-happy
  GrowArray<DottedProduction const*> array(0 /*allocate later*/);
  computeKernelCRC(array);

  // compute this so we can throw away items later if we want to
  stateSymbol = computeStateSymbolC();
}


void ItemSet::computeKernelCRC(GrowArray<DottedProduction const*> &array)
{
  int numKernelItems = kernelItems.count();
  
  // expand as necessary, but don't get smaller
  array.ensureAtLeast(numKernelItems);

  // we will crc the prod/dot fields, using the pointer representation
  // of 'dprod'; assumes the items have already been sorted!
  int index = 0;
  FOREACH_OBJLIST(LRItem, kernelItems, kitem) {
    array[index] = kitem.data()->dprod;
    index++;
  }

  // CRC the buffer
  kernelItemsCRC = crc32((unsigned char const*)(array.getArray()),
                         sizeof(array[0]) * numKernelItems);
}


void ItemSet::print(ostream &os, GrammarAnalysis const &g) const
{
  os << "ItemSet " << id << ":\n";

  // collect all items
  SObjList<LRItem> items;     // (constness) don't use 'item' to modify elements
  getAllItems(items);

  // for each item  
  SFOREACH_OBJLIST(LRItem, items, itemIter) {
    LRItem const *item = itemIter.data();

    // print its text
    os << "  ";
    item->print(os, g);
    os << "      ";

    // print any transitions on its after-dot symbol
    if (!item->isDotAtEnd()) {
      ItemSet const *is = transitionC(item->symbolAfterDotC());
      if (is == NULL) {
        // this happens if I print the item set before running closure,
        // and also after prec/assoc disambiguation
        os << "(no transition)";
      }
      else {
        os << "--> " << is->id;
      }
    }
    os << endl;
  }

  // print transition function directly, since I'm now throwing
  // away items sometimes
  for (int t=0; t<terms; t++) {
    if (termTransition[t]) {
      os << "  on terminal " << g.getTerminal(t)->name 
         << " go to " << termTransition[t]->id << endl;
    }
  }

  for (int n=0; n<nonterms; n++) {
    if (nontermTransition[n]) {
      os << "  on nonterminal " << g.getNonterminal(n)->name 
         << " go to " << nontermTransition[n]->id << endl;
    }
  }
  
  for (int p=0; p<numDotsAtEnd; p++) {
    os << "  can reduce by " << dotsAtEnd[p]->getProd()->toString() << endl;
  }
}


void ItemSet::writeGraph(ostream &os, GrammarAnalysis const &g) const
{
  // node: n <name> <desc>
  os << "\nn ItemSet" << id << " ItemSet" << id << "/";
    // rest of desc will follow

  // collect all items
  SObjList<LRItem> items;         // (constness) don't use 'items' to modify elements
  getAllItems(items);

  // for each item, print the item text
  SFOREACH_OBJLIST(LRItem, items, itemIter) {
    LRItem const *item = itemIter.data();

    // print its text
    os << "   ";
    item->print(os, g);

    // THIS IS A PROBLEM!  the item's output will include
    // slashes too, if it has >1 lookahead token ... !
    os << "/";      // line separator in my node format
  }
  os << endl;

  // print transitions on terminals
  INTLOOP(t, 0, terms) {
    if (termTransition[t] != NULL) {
      os << "e ItemSet" << id
         << " ItemSet" << termTransition[t]->id << endl;
    }
  }

  // print transitions on nonterminals
  INTLOOP(nt, 0, nonterms) {
    if (nontermTransition[nt] != NULL) {
      os << "e ItemSet" << id
         << " ItemSet" << nontermTransition[nt]->id << endl;
    }
  }
}


// ------------------------- ParseTables -----------------------
ParseTables::ParseTables(int t, int nt, int s, int p)
{
  alloc(t, nt, s, p);
}

void ParseTables::alloc(int t, int nt, int s, int p)
{
  numTerms = t;
  numNonterms = nt;
  numStates = s;
  numProds = p;

  actionTable = new ActionEntry[actionTableSize()];
  memset(actionTable, 0, sizeof(actionTable[0]) * actionTableSize());

  gotoTable = new GotoEntry[gotoTableSize()];
  memset(gotoTable, 0, sizeof(gotoTable[0]) * gotoTableSize());
  
  prodInfo = new ProdInfo[numProds];
  memset(prodInfo, 0, sizeof(prodInfo[0]) * numProds);
  
  stateSymbol = new SymbolId[numStates];
  memset(stateSymbol, 0, sizeof(stateSymbol[0]) * numStates);
}


ParseTables::~ParseTables()
{
  delete[] actionTable;
  delete[] gotoTable;
  delete[] prodInfo;
  delete[] stateSymbol;

  for (int i=0; i<numAmbig(); i++) {
    delete[] ambigAction[i];
  }
}


ActionEntry ParseTables::validateAction(int code) const
{
  // make sure that 'code' is representable; if this fails, most likely
  // there are more than 32k states or productions; in turn, the most
  // likely cause of *that* would be the grammar is being generated
  // automatically from some other specification; you can change the
  // typedefs of ActionEntry and GotoEntry in gramanl.h to get more
  // capacity
  ActionEntry ret = (ActionEntry)code;
  xassert((int)ret == code);
  return ret;
}

GotoEntry ParseTables::validateGoto(int code) const
{
  // see above
  GotoEntry ret = (GotoEntry)code;
  xassert((int)ret == code);
  return ret;
}


ParseTables::ParseTables(Flatten&)
{
  actionTable = NULL;
  gotoTable = NULL;
  prodInfo = NULL;
}

     
template <class T>
void xferSimpleArray(Flatten &flat, T *array, int numElements)
{
  int len = sizeof(array[0]) * numElements;
  flat.xferSimple(array, len);
  flat.checkpoint(crc32((unsigned char const *)array, len));
}

void ParseTables::xfer(Flatten &flat)
{
  flat.checkpoint(0x1B2D2F16);

  flat.xferInt(numTerms);
  flat.xferInt(numNonterms);
  flat.xferInt(numStates);
  flat.xferInt(numProds);

  if (flat.reading()) {
    alloc(numTerms, numNonterms, numStates, numProds);
  }
           
  xferSimpleArray(flat, actionTable, actionTableSize());
  xferSimpleArray(flat, gotoTable, gotoTableSize());
  xferSimpleArray(flat, prodInfo, numProds);
  xferSimpleArray(flat, stateSymbol, numStates);

  // ambigAction
  if (flat.writing()) {
    flat.writeInt(numAmbig());
    for (int i=0; i<numAmbig(); i++) {
      flat.writeInt(ambigAction[i][0]);    // length of this entry

      for (int j=0; j<ambigAction[i][0]; j++) {
        flat.writeInt(ambigAction[i][j+1]);
      }
    }
  }

  else {
    int ambigs = flat.readInt();
    for (int i=0; i<ambigs; i++) {
      int len = flat.readInt();
      ActionEntry *entry = new ActionEntry[len+1];
      entry[0] = len;

      for (int j=0; j<len; j++) {
        entry[j+1] = flat.readInt();
      }

      ambigAction.push(entry);
    }
  }

  // make sure reading and writing agree
  flat.checkpoint(numAmbig());

}


// ------------------------ GrammarAnalysis --------------------
GrammarAnalysis::GrammarAnalysis()
  : derivable(NULL),
    indexedNonterms(NULL),
    indexedTerms(NULL),
    numNonterms(0),
    numTerms(0),
    productionsByLHS(NULL),
    dottedProds(NULL),
    indexedProds(NULL),
    numProds(0),
    initialized(false),
    nextItemSetId(0),    // [ASU] starts at 0 too
    itemSets(),
    startState(NULL),
    cyclic(false),
    symOfInterest(NULL),
    errors(0),
    tables(NULL)
{}


GrammarAnalysis::~GrammarAnalysis()
{
  if (indexedNonterms != NULL) {
    delete indexedNonterms;
  }

  if (indexedTerms != NULL) {
    delete indexedTerms;
  }

  if (productionsByLHS != NULL) {
    // empties all lists automatically because of "[]"
    delete[] productionsByLHS;
  }

  if (indexedProds != NULL) {
    delete[] indexedProds;
  }

  deleteDottedProductions();

  if (derivable != NULL) {
    delete derivable;
  }
  
  if (tables) {
    delete tables;
  }
}


Terminal const *GrammarAnalysis::getTerminal(int index) const
{
  xassert((unsigned)index < (unsigned)numTerms);
  return indexedTerms[index];
}

Nonterminal const *GrammarAnalysis::getNonterminal(int index) const
{
  xassert((unsigned)index < (unsigned)numNonterms);
  return indexedNonterms[index];
}

Production const *GrammarAnalysis::getProduction(int index) const
{
  xassert((unsigned)index < (unsigned)numProds);
  return indexedProds[index];
}

ItemSet const *GrammarAnalysis::getItemSet(int index) const
{
  // no pretense of efficiency; this is only used interactively
  FOREACH_OBJLIST(ItemSet, itemSets, iter) {
    if (iter.data()->id == index) {
      return iter.data();
    }
  }
  return NULL;
}


void GrammarAnalysis::xfer(Flatten &flat)
{
  Grammar::xfer(flat);

  xferOwnerPtr(flat, derivable);

  // delay indexed[Non]Terms, productionsByLHS,
  // and initialized

  flat.xferInt(nextItemSetId);

  xferObjList(flat, itemSets);
  xferSerfPtrToList(flat, startState, itemSets);

  flat.xferBool(cyclic);

  // don't bother xferring 'symOfInterest', since it's
  // only used for debugging

  xferOwnerPtr(flat, tables);

  // now do the easily-computable stuff
  // NOTE: these functions are also called by initializeAuxData,
  // so they need to serve both callers correctly
  computeIndexedNonterms();
  computeIndexedTerms();
  computeProductionsByLHS();
  createDottedProductions();

  // do serfs after because if I want to compute the
  // nonkernel items instead of storing them, I need
  // the indices
  MUTATE_EACH_OBJLIST(ItemSet, itemSets, iter) {
    iter.data()->xferSerfs(flat, *this);
  }

  flat.xferBool(initialized);
}


void GrammarAnalysis::
  printProductions(ostream &os, bool printCode) const
{
  if (cyclic) {
    os << "(cyclic!) ";
  }
  Grammar::printProductions(os, printCode);
}


void GrammarAnalysis::
  printProductionsAndItems(ostream &os, bool printCode) const
{
  printProductions(os, printCode);

  FOREACH_OBJLIST(ItemSet, itemSets, iter) {
    iter.data()->print(os, *this);
  }
}


void printSymbols(ostream &os, ObjList<Symbol> const &list)
{
  for (ObjListIter<Symbol> iter(list);
       !iter.isDone(); iter.adv()) {
    os << "  " << *(iter.data()) << endl;
  }
}


bool GrammarAnalysis::addDerivable(Nonterminal const *left, Nonterminal const *right)
{
  return addDerivable(left->ntIndex, right->ntIndex);
}

bool GrammarAnalysis::addDerivable(int left, int right)
{
  // Almost as an aside, I'd like to track cyclicity in grammars.
  // It's always true that N ->* N, because 0 steps are allowed.
  // A grammar is cyclic if N ->+ N, i.e. it derives itself in
  // 1 or more steps.
  //
  // We can detect that fairly easily by tracking calls to
  // this fn with left==right.  Since N ->* N in 0 steps is
  // recorded during init (and *not* by calling this fn), the
  // only calls to this with left==right will be when the
  // derivability code detects a nonzero-length path.

  if (left==right) {
    Nonterminal *NT = indexedNonterms[left];    // ==right
    if (!NT->cyclic) {
      trace("derivable")
        << "discovered that " << NT->name << " ->+ "
        << NT->name << " (i.e. is cyclic)\n";
      NT->cyclic = true;
      cyclic = true;     // for grammar as a whole

      // Even though we didn't know this already, it doesn't
      // constitute a change in the ->* relation (which is what the
      // derivability code cares about), so we do *not* report a
      // change for the cyclicty detection.
    }
  }

  // we only made a change, and hence should return true,
  // if there was a 0 here before
  return 0 == derivable->testAndSet(point(left, right));
}


bool GrammarAnalysis::canDerive(Nonterminal const *left, Nonterminal const *right) const
{
  return canDerive(left->ntIndex, right->ntIndex);
}

bool GrammarAnalysis::canDerive(int left, int right) const
{
  return 1 == derivable->get(point(left, right));
}


void GrammarAnalysis::initDerivableRelation()
{
  // two-dimensional matrix to represent token derivabilities
  int numNonterms = numNonterminals();
  derivable = new Bit2d(point(numNonterms, numNonterms));

  // initialize it
  derivable->setall(0);
  loopi(numNonterms) {
    derivable->set(point(i,i));
      // every nonterminal can derive itself in 0 or more steps
      // (specifically, in 0 steps, at least)
      //
      // NOTE: we do *not* call addDerivable because that would
      // mess up the cyclicity detection logic
  }
}


bool GrammarAnalysis::canDeriveEmpty(Nonterminal const *nonterm) const
{
  return canDerive(nonterm, &emptyString);
}


bool GrammarAnalysis::sequenceCanDeriveEmpty(RHSEltList const &list) const
{
  RHSEltListIter iter(list);
  return iterSeqCanDeriveEmpty(iter);
}

bool GrammarAnalysis::iterSeqCanDeriveEmpty(RHSEltListIter iter) const
{
  // look through the sequence beginning with 'iter'; if any members cannot
  // derive emptyString, fail
  for (; !iter.isDone(); iter.adv()) {
    if (iter.data()->sym->isTerminal()) {
      return false;    // terminals can't derive emptyString
    }

    if (!canDeriveEmpty(&( iter.data()->sym->asNonterminalC() ))) {
      return false;    // nonterminal that can't derive emptyString
    }
  }

  return true;
}


bool GrammarAnalysis::firstIncludes(Nonterminal const *NT, Terminal const *term) const
{
  return NT->first.contains(term->termIndex);
}

#if 0
bool GrammarAnalysis::addFirst(Nonterminal *NT, Terminal *term)
{
  return NT->first.prependUnique(term);

  // regarding non-constness of 'term':
  // highly nonideal.. the problem is that by using annotations in
  // the structures themselves, I have a hard time saying that I
  // intend to modify the annotations but not the "key" data...
  // this cast is really a symptom of that too.. (and, perhaps, also
  // that I don't have a List class that promises to never permit
  // modification of the pointed-to data.. but it's not clear I'd
  // be better of using it here even if I had it)
}
#endif // 0


bool GrammarAnalysis::followIncludes(Nonterminal const *NT, Terminal const *term) const
{
  return NT->follow.contains(term->termIndex);
}
 
#if 0
// returns true if Follow(NT) is changed by adding 'term' to it
bool GrammarAnalysis::addFollow(Nonterminal *NT, Terminal *term)
{
  return NT->follow.prependUnique(term);
}    
#endif // 0


// ----------------- Grammar algorithms --------------------------
// create and initialize 'indexedNonterms'
void GrammarAnalysis::computeIndexedNonterms()
{
  // map: ntIndex -> Nonterminal*
  numNonterms = Grammar::numNonterminals();
  indexedNonterms = new Nonterminal* [numNonterms];

  // fill it
  indexedNonterms[emptyStringIndex] = &emptyString;
  int index = emptyStringIndex;
  emptyString.ntIndex = index++;

  for (ObjListMutator<Nonterminal> sym(nonterminals);
       !sym.isDone(); index++, sym.adv()) {
    indexedNonterms[index] = sym.data();    // map: index to symbol
    sym.data()->ntIndex = index;            // map: symbol to index
  }
}


// create and initialize 'indexedTerms'
void GrammarAnalysis::computeIndexedTerms()
{
  // map: termIndex -> Terminal*
  // the ids have already been assigned; but I'm going to continue
  // to insist on a contiguous space starting at 0
  numTerms = Grammar::numTerminals();
  indexedTerms = new Terminal* [numTerms];
  loopi(numTerminals()) {
    indexedTerms[i] = NULL;      // used to track id duplication
  }
  for (ObjListMutator<Terminal> sym(terminals);
       !sym.isDone(); sym.adv()) {
    int index = sym.data()->termIndex;   // map: symbol to index
    if (indexedTerms[index] != NULL) {
      xfailure(stringc << "terminal index collision at index " << index);
    }
    indexedTerms[index] = sym.data();    // map: index to symbol
  }
}


// set the first/follow of all nonterminals to the correct size
void GrammarAnalysis::resetFirstFollow()
{
  MUTATE_EACH_NONTERMINAL(nonterminals, sym) {
    sym.data()->first.reset(numTerminals());
    sym.data()->follow.reset(numTerminals());
  }
}


// create and initialize 'productionsByLHS' and 'indexedProds'
void GrammarAnalysis::computeProductionsByLHS()
{
  // map: nonterminal -> productions with that nonterm on LHS
  productionsByLHS = new SObjList<Production> [numNonterms];
  
  // map: prodIndex -> production
  numProds = productions.count();
  indexedProds = new Production* [numProds];
  memset(indexedProds, 0, sizeof(*indexedProds) * numProds);

  // fill in both maps
  {
    MUTATE_EACH_PRODUCTION(productions, prod) {        // (constness)
      int LHSindex = prod.data()->left->ntIndex;
      xassert(LHSindex < numNonterms);

      productionsByLHS[LHSindex].append(prod.data());
      indexedProds[prod.data()->prodIndex] = prod.data();
    }
  }                                                      

  // verify we filled the 'prodIndex' map
  for (int id=0; id<numProds; id++) {
    xassert(indexedProds[id] != NULL);
  }
}


void GrammarAnalysis::createDottedProductions()
{
  // map: prodIndex x dotPosn -> DottedProduction
  //DottedProduction const **
  dottedProds = new DottedProduction* [numProds];
  memset(dottedProds, 0, sizeof(*dottedProds) * numProds);

  FOREACH_PRODUCTION(productions, iter) {
    Production const *prod = iter.data();
    int rhsLen = prod->rhsLength();
    xassert(rhsLen >= 0);
    int id = prod->prodIndex;

    // one dottedproduction for every dot position, which is one
    // more than the # of RHS elements
    DottedProduction *array = new DottedProduction[rhsLen + 1];
    dottedProds[id] = array;

    // fill in each one
    for (int posn=0; posn <= rhsLen; posn++) {
      array[posn].setProdAndDot(prod, posn);
    }
  }

  // verify we filled the whole table, i.e. that the production
  // indices form a dense map
  for (int id=0; id<numProds; id++) {
    xassert(dottedProds[id] != NULL);
  }
}


void GrammarAnalysis::deleteDottedProductions()
{
  if (dottedProds != NULL) {
    for (int id=0; id<numProds; id++) {
      delete[] dottedProds[id];
    }
    delete[] dottedProds;
    dottedProds = NULL;
  }
}


DottedProduction const *GrammarAnalysis::
  getDProd(Production const *prod, int posn) const
{
  xassert(posn <= prod->rhsLength());
  return &( dottedProds[prod->prodIndex][posn] );
}

DottedProduction const *GrammarAnalysis::
  getDProdIndex(int prodIndex, int posn) const
{
  // go through the other fn to bounds-check 'posn'
  return getDProd(getProduction(prodIndex), posn);
}


#ifndef NDEBUG
DottedProduction const *GrammarAnalysis::
  nextDProd(DottedProduction const *dp) const
{
  xassert(!dp->isDotAtEnd());
  return dp + 1;
}
#endif // !NDEBUG


// NOTE: the sequence of initialization actions in this function
// and the functions it calls must interact properly with the
// sequence in GrammarAnalysis::xfer
void GrammarAnalysis::initializeAuxData()
{
  // at the moment, calling this twice leaks memory
  xassert(!initialized);

  computeIndexedNonterms();
  computeIndexedTerms();
  resetFirstFollow();

  computeProductionsByLHS();
  computeReachable();

  // finish the productions before we compute the
  // dotted productions
  MUTATE_EACH_PRODUCTION(productions, prod) {
    prod.data()->finished(numTerminals());
  }

  createDottedProductions();

  // initialize the derivable relation
  initDerivableRelation();

  // mark the grammar as initialized
  initialized = true;
}


void GrammarAnalysis::computeWhatCanDeriveWhat()
{
  xassert(initialized);


  // iterate: propagate 'true' bits across the derivability matrix
  // (i.e. compute transitive closure on the canDerive relation)
  for (;;) {
    int changes = 0;       // for this iter, # of times we set a matrix bit

    // --------- first part: add new canDerive relations --------
    // loop over all productions
    for (ObjListIter<Production> prodIter(productions);
         !prodIter.isDone(); prodIter.adv()) {
      // convenient alias
      Production const *prod = prodIter.data();

      // since I don't include 'empty' explicitly in my rules, I won't
      // conclude that anything can derive empty, which is a problem;
      // so I special-case it here
      if (prod->right.isEmpty()) {
	addDerivable(prod->left, &emptyString);
        continue;      	// no point in looping over RHS symbols since there are none
      }

      // iterate over RHS symbols, seeing if the LHS can derive that
      // RHS symbol (by itself)
      for (RHSEltListIter rightSym(prod->right);
           !rightSym.isDone(); rightSym.adv()) {

        if (rightSym.data()->sym->isTerminal()) {
          // if prod->left derives a string containing a terminal,
          // then it can't derive any nontermial alone (using this
          // production, at least) -- empty is considered a nonterminal
          break;
        }

        // otherwise, it's a nonterminal
        Nonterminal const &rightNT = rightSym.data()->sym->asNonterminalC();

        // check if we already know that LHS derives rightNT
        if (canDerive(prod->left, &rightNT)) {
          // we already know that prod->left derives rightSym,
          // so let's not check it again
        }

        else {
          // we are wondering if prod->left can derive rightSym.. for
          // this to be true, every symbol that comes after rightSym
          // must be able to derive emptySymbol (we've already verified
          // by now that every symbol to the *left* can derive empty)
          RHSEltListIter afterRightSym(rightSym);
          bool restDeriveEmpty = true;
          for (afterRightSym.adv();    // *after* right symbol
               !afterRightSym.isDone(); afterRightSym.adv()) {

            if (afterRightSym.data()->sym->isTerminal()  ||
                  // if it's a terminal, it can't derive emptyString
                !canDeriveEmpty(&( afterRightSym.data()->sym->asNonterminalC() ))) {
                  // this symbol can't derive empty string (or, we don't
                  // yet know that it can), so we conclude that prod->left
                  // can't derive rightSym
              restDeriveEmpty = false;
              break;
            }
          }

          if (restDeriveEmpty) {
            // we have discovered that prod->left can derive rightSym
            bool chgd = addDerivable(prod->left, &rightNT);
            xassert(chgd);    // above, we verified we didn't already know this

            changes++;

            trace("derivable") 
              << "discovered (by production): " << prod->left->name
              << " ->* " << rightNT.name << "\n";
          }
        }

        // ok, we've considered prod->left deriving rightSym.  now, we
        // want to consider whether prod->left can derive any of the
        // symbols that follow rightSym in this production.  for this
        // to be true, rightSym itself must derive the emptyString
        if (!canDeriveEmpty(&rightNT)) {
          // it doesn't -- no point in further consideration of
          // this production
          break;
        }
      } // end of loop over RHS symbols
    } // end of loop over productions


    // -------- second part: compute closure over existing relations ------
    // I'll do this by computing R + R^2 -- that is, I'll find all
    // paths of length 2 and add an edge between their endpoints.
    // I do this, rather than computing the entire closure now, since
    // on the next iter I will add more relations and have to re-do
    // a full closure; iterative progress seems a better way.

    // I don't consider edges (u,u) because it messes up my cyclicty
    // detection logic.  (But (u,v) and (v,u) is ok, and in fact is
    // what I want, for detecting cycles.)

    // for each node u (except empty)
    int numNonterms = numNonterminals();
    for (int u=1; u<numNonterms; u++) {
      // for each edge (u,v) where u != v
      for (int v=0; v<numNonterms; v++) {
        if (u==v || !canDerive(u,v)) continue;

        // for each edge (v,w) where v != w
        for (int w=0; w<numNonterms; w++) {
          if (v==w || !canDerive(v,w)) continue;

          // add an edge (u,w), if there isn't one already
          if (addDerivable(u,w)) {
            changes++;
            trace("derivable") 
              << "discovered (by closure step): "
              << indexedNonterms[u]->name << " ->* "
              << indexedNonterms[w]->name << "\n";
          }
        }
      }
    }


    // ------ finally: iterate until no changes -------
    if (changes == 0) {
      // didn't make any changes during the last iter, so
      // everything has settled
      break;
    }
  } // end of loop until settles


  // I used to do all closure here and no closure in the loop.
  // But that fails in cases where closure (when it reveals
  // more things that derive emptyString) yields new opportunities
  // for derives-relation discovery.  Therefore I now alternate
  // between them, and at the end, no closure is necessary.
}


// Compute, for each nonterminal, the "First" set, defined as:
//
//   First(N) = { x | N ->* x alpha }, where alpha is any sequence
//                                     of terminals and nonterminals
//
// If N can derive emptyString, I'm going to say that empty is
// *not* in First, despite what Aho/Sethi/Ullman says.  I do this
// because I have that information readily as my derivable relation,
// and because it violates the type system I've devised.
//
// I also don't "compute" First for terminals, since they are trivial
// (First(x) = {x}).
void GrammarAnalysis::computeFirst()
{
  bool tr = tracingSys("first");
  int numTerms = numTerminals();

  // iterate, looking for new First members, until no changes
  int changes = 1;   // so the loop begins
  while (changes > 0) {
    changes = 0;

    // for each production
    for (ObjListMutator<Production> prodIter(productions);
         !prodIter.isDone(); prodIter.adv()) {
      // convenient aliases
      Production *prod = prodIter.data();
      Nonterminal *LHS = prod->left;
        // the list iter is mutating because I modify LHS's First set

      // compute First(RHS-sequence)
      TerminalSet firstOfRHS(numTerms);
      firstOfSequence(firstOfRHS, prod->right);

      // store this back into 'prod'
      prod->firstSet.merge(firstOfRHS);

      // add everything in First(RHS-sequence) to First(LHS)
      if (LHS->first.merge(firstOfRHS)) {
        changes++;
        if (tr) {
          ostream &trs = trace("first");
          trs << "added ";
          firstOfRHS.print(trs, *this);
          trs << " to " << LHS->name << " because of "
              << prod->toString() << endl;
        }
      }
    } // for (productions)
  } // while (changes)

  if (tr) {
    FOREACH_NONTERMINAL(nonterminals, iter) {
      Nonterminal const &nt = *(iter.data());

      ostream &trs = trace("first") << " " << nt.name << ": ";
      nt.first.print(trs, *this);
      trs << endl;
    }
  }
}


// 'sequence' isn't const because we need to hand pointers over to
// the 'destList', which isn't const; similarly for 'this'
// (what I'd like here is to say that 'sequence' and 'this' are const
// if 'destList' can't modify the things it contains)
void GrammarAnalysis::firstOfSequence(TerminalSet &destList,
                                      RHSEltList const &sequence)
{
  RHSEltListIter iter(sequence);
  firstOfIterSeq(destList, iter);
}

// similar to above, 'sym' needs to be a mutator
void GrammarAnalysis::firstOfIterSeq(TerminalSet &destList,
                                     RHSEltListIter sym)
{
  //int numTerms = numTerminals();

  // for each sequence member such that all
  // preceeding members can derive emptyString
  for (; !sym.isDone(); sym.adv()) {
    // LHS -> x alpha   means x is in First(LHS)
    if (sym.data()->sym->isTerminal()) {
      destList.add(sym.data()->sym->asTerminal().termIndex);
      break;    // stop considering RHS members since a terminal
                // effectively "hides" all further symbols from First
    }

    // sym must be a nonterminal
    Nonterminal const &nt = sym.data()->sym->asNonterminalC();

    // anything already in nt's First should be added to destList
    destList.merge(nt.first);

    // if nt can't derive emptyString, then it blocks further
    // consideration of right-hand side members
    if (!canDeriveEmpty(&nt)) {
      break;
    }
  } // for (RHS members)
}


void GrammarAnalysis::computeDProdFirsts()
{
  // for each production..
  FOREACH_PRODUCTION(productions, prodIter) {
    // for each dotted production where the dot is not at the end..
    int rhsLen = prodIter.data()->rhsLength();
    for (int posn=0; posn <= rhsLen; posn++) {
      DottedProduction *dprod = getDProd_nc(prodIter.data(), posn);

      // compute its first
      RHSEltListIter symIter(dprod->getProd()->right, posn);
      dprod->firstSet.reset(numTerms);
      firstOfIterSeq(dprod->firstSet, symIter);

      // can it derive empty?
      dprod->canDeriveEmpty = iterSeqCanDeriveEmpty(symIter);
    }
  }
}


void GrammarAnalysis::computeFollow()
{
  int numTerms = numTerminals();

  // loop until no changes
  int changes = 1;
  while (changes > 0) {
    changes = 0;

    // 'mutate' is needed because adding 'term' to the follow of 'nt'
    // needs a mutable 'term' and 'nt'

    // for each production
    MUTATE_EACH_PRODUCTION(productions, prodIter) {
      Production *prod = prodIter.data();

      // for each RHS nonterminal member
      MUTATE_EACH_OBJLIST(Production::RHSElt, prod->right, rightSym) {
        if (rightSym.data()->sym->isTerminal()) continue;

        // convenient alias
        Nonterminal &rightNT = rightSym.data()->sym->asNonterminal();

        // I'm not sure what it means to compute Follow(emptyString),
        // so let's just not do so
        if (&rightNT == &emptyString) {
          continue;
        }

        // an iterator pointing to the symbol just after
        // 'rightSym' will be useful below
        RHSEltListMutator afterRightSym(rightSym);
        afterRightSym.adv();    // NOTE: 'isDone()' may be true now

        // rule 1:
        // if there is a production A -> alpha B beta, then
        // everything in First(beta) is in Follow(B)
        {
          // compute First(beta)
          TerminalSet firstOfBeta(numTerms);
          firstOfIterSeq(firstOfBeta, afterRightSym);

          // put those into Follow(rightNT)
          if (rightNT.follow.merge(firstOfBeta)) {
            changes++;
            if (&rightNT == symOfInterest) {
              ostream &trs = trace("follow-sym");
              trs << "Follow(" << rightNT.name
                  << "): adding ";
              firstOfBeta.print(trs, *this);
              trs << " by first(RHS-tail) of " << *prod
                  << endl;
            }
          }
        }

        // rule 2:
        // if there is a production A -> alpha B, or a
        // production A -> alpha B beta where beta ->* empty ...
        if (iterSeqCanDeriveEmpty(afterRightSym)) {
          // ... then everything in Follow(A) is in Follow(B)
          if (rightNT.follow.merge(prod->left->follow)) {
            changes++;
            if (&rightNT == symOfInterest) {
              ostream &trs = trace("follow-sym");
              trs << "Follow(" << rightNT.name
                  << "): adding ";
              prod->left->follow.print(trs, *this);
              trs << " by follow(LHS) of " << *prod
                  << endl;
            }
          }
        }

      } // for each RHS nonterminal member
    } // for each production
  } // until no changes
}


// [ASU] alg 4.4, p.190
void GrammarAnalysis::computePredictiveParsingTable()
{
  int numTerms = numTerminals();
  int numNonterms = numNonterminals();

  // the table will be a 2d array of lists of productions
  ProductionList *table = new ProductionList[numTerms * numNonterms];     // (owner)
  #define TABLE(term,nt) table[(term) + (nt)*numNonterms]

  // for each production 'prod' (non-const iter because adding them
  // to ProductionList, which doesn't promise to not change them)
  MUTATE_EACH_PRODUCTION(productions, prodIter) {
    Production *prod = prodIter.data();

    // for each terminal 'term' in First(RHS)
    TerminalSet firsts(numTerms);
    firstOfSequence(firsts, prod->right);
    for (int termIndex=0; termIndex<numTerms; termIndex++) {
      if (!firsts.contains(termIndex)) continue;

      // add 'prod' to table[LHS,term]
      TABLE(prod->left->ntIndex, termIndex).prependUnique(prod);
    }

    // if RHS ->* emptyString, ...
    if (sequenceCanDeriveEmpty(prod->right)) {
      // ... then for each terminal 'term' in Follow(LHS), ...
      for (int termIndex=0; termIndex<numTerms; termIndex++) {
        if (!firsts.contains(termIndex)) continue;

        // ... add 'prod' to table[LHS,term]
        TABLE(prod->left->ntIndex, termIndex).prependUnique(prod);
      }
    }
  }


  // print the resulting table
  ostream &os = trace("pred-table") << endl;

  // for each nonterminal
  INTLOOP(nonterm, 0, numNonterms) {
    os << "Row " << indexedNonterms[nonterm]->name << ":\n";

    // for each terminal
    INTLOOP(term, 0, numTerms) {
      os << "  Column " << indexedTerms[term]->name << ":";

      // for each production in table[nonterm,term]
      SFOREACH_PRODUCTION(TABLE(nonterm,term), prod) {
        os << "   ";
        prod.data()->print(os);
      }

      os << endl;
    }
  }

  // cleanup
  #undef TABLE
  delete[] table;
}


// these hashtables are keyed using the DottedProduction,
// but yield LRItems as values

// for storing dotted productions in a hash table, this is
// the hash function itself
STATICDEF unsigned LRItem::hash(DottedProduction const *key)
{
  //DottedProduction const *dp = (DottedProduction const*)key;

  // on the assumption few productions have 20 RHS elts..
  //int val = dp->dot + (20 * dp->prod->prodIndex);

  // just use the address.. they're all shared..
  return HashTable::lcprngHashFn((void*)key);
}

// given the data, yield the key
STATICDEF DottedProduction const *LRItem::dataToKey(LRItem *it)
{
  return it->dprod;
}

// compare two dotted production keys for equality; since dotted
// productions are shared, pointer equality suffices
STATICDEF bool LRItem::dpEqual(DottedProduction const *key1, 
                               DottedProduction const *key2)
{
  return key1 == key2;
}


// based on [ASU] figure 4.33, p.223
// NOTE: sometimes this is called with nonempty nonkernel items...
void GrammarAnalysis::itemSetClosure(ItemSet &itemSet)
{
  bool const tr = tracingSys("closure");
  ostream &trs = trace("closure");     // trace stream
  if (tr) {
    trs << "computing closure of ";
    itemSet.print(trs, *this);
  }

  // hashtable, list of items still yet to close; items are
  // simultaneously in both the hash and the list, or not in either
  #if 0
  OwnerKHashArray<LRItem, DottedProduction> workhash(
    &LRItem::dataToKey,
    &LRItem::hash,
    &LRItem::dpEqual, 13);
  #endif // 0
  
  // every 'item' on the worklist has item->dprod->backPointer == item;
  // every 'dprod' not associated has dprod->backPointer == NULL
  ArrayStack<LRItem*> worklist;

  // scratch terminal set for singleItemClosure
  TerminalSet scratchSet(numTerminals());

  // and another for the items we've finished
  OwnerKHashTable<LRItem, DottedProduction> finished(
    &LRItem::dataToKey,
    &LRItem::hash,
    &LRItem::dpEqual, 13);
  finished.setEnableShrink(false);

  // put all the nonkernels we have into 'finished'
  while (itemSet.nonkernelItems.isNotEmpty()) {
    LRItem *dp = itemSet.nonkernelItems.removeFirst();
    finished.add(dp->dprod, dp);
  }

  // first, close the kernel items -> worklist
  FOREACH_OBJLIST(LRItem, itemSet.kernelItems, itemIter) {
    singleItemClosure(finished, worklist, itemIter.data(), scratchSet);
  }

  while (worklist.isNotEmpty()) {
    // pull the first production
    LRItem *item = worklist.pop();
    xassert(item->dprod->backPointer == item);     // was on worklist
    item->dprod->backPointer = NULL;               // now off of worklist

    // put it into list of 'done' items; this way, if this
    // exact item is generated during closure, it will be
    // seen and re-inserted (instead of duplicated)
    finished.add(item->dprod, item);

    // close it -> worklist
    singleItemClosure(finished, worklist, item, scratchSet);
  }

  // move everything from 'finished' to the nonkernel items list
  try {
    for (OwnerKHashTableIter<LRItem, DottedProduction> iter(finished);
         !iter.isDone(); iter.adv()) {
      // temporarily, the item is owned both by the hashtable
      // and the list
      itemSet.nonkernelItems.prepend(iter.data());
    }
    finished.disownAndForgetAll();
  }
  catch (...) {
    breaker();    // debug breakpoint

    // resolve the multiple ownership by leaking some
    finished.disownAndForgetAll();
    throw;
  }

  // we potentially added a bunch of things
  itemSet.changedItems();

  if (tr) {
    trs << "done with closure of state " << itemSet.id << endl;
    itemSet.print(trs, *this);
  }
}


void GrammarAnalysis
  ::singleItemClosure(OwnerKHashTable<LRItem, DottedProduction> &finished,
                      ArrayStack<LRItem*> &worklist,
                      //OwnerKHashArray<LRItem, DottedProduction> &workhash,
                      LRItem const *item, TerminalSet &newItemLA)
{
  INITIAL_MALLOC_STATS();

  bool const tr = tracingSys("closure");
  ostream &trs = trace("closure");     // trace stream

  if (tr) {
    trs << "  considering item ";
    item->print(trs, *this);
    trs << endl;
  }

  if (item->isDotAtEnd()) {
    if (tr) {
      trs << "    dot is at the end" << endl;
    }
    CHECK_MALLOC_STATS("return, dot at end");
    return;
  }

  // in comments that follow, 'item' is broken down as
  //   A -> alpha . B beta, LA

  // get the symbol B (the one right after the dot)
  Symbol const *B = item->symbolAfterDotC();
  if (B->isTerminal()) {
    if (tr) {
      trs << "    symbol after the dot is a terminal" << endl;
    }
    CHECK_MALLOC_STATS("return, dot sym is terminal");
    return;
  }
  int nontermIndex = B->asNonterminalC().ntIndex;

  // could pull this out of even this fn, to the caller, but I don't
  // see any difference in time when I make it static (which simulates
  // the effect, though static itself is a bad idea because it makes
  // the size constant through a whole run); but maybe when other things
  // are faster I will be able to notice the difference, so I might
  // revisit this
  //TerminalSet newItemLA(numTerminals());

  // for each production "B -> gamma"
  SMUTATE_EACH_PRODUCTION(productionsByLHS[nontermIndex], prodIter) {    // (constness)
    Production &prod = *(prodIter.data());
    if (tr) {
      trs << "    considering production " << prod << endl;
    }

    // key to good performance: do *no* dynamic allocation in this
    // loop (one of two inner loops in the grammar analysis), until a
    // new item is actually *needed* (which is the uncommon case); for
    // example, all debug output statements are guarded by 'if (tr)'
    // because otherwise they would allocate

    // invariant of the indexed productions list
    xassert(prod.left == B);

    // construct "B -> . gamma, First(beta LA)";
    // except, don't actually build it until later; in the meantime,
    // determine which DP and lookahead it would use if created
    DottedProduction const *newDP = getDProd(&prod, 0 /*dot at left*/);

    // get beta (what follows B in 'item')
    DottedProduction const *beta = nextDProd(item->dprod);

    // get First(beta) -> new item's lookahead
    newItemLA = beta->firstSet;

    // if beta ->* epsilon, add LA
    if (beta->canDeriveEmpty) {
      newItemLA.merge(item->lookahead);
    }

    if (tr) {
      trs << "      built item ";
      // this is what LRItem::print would do if I actually
      // constructed the object
      newDP->print(trs);
      trs << ", ";
      newItemLA.print(trs, *this);
      trs << endl;
    }

    // is 'newDP' already there?
    // check in working and finished tables
    bool inDoneList = true;
    LRItem *already = newDP->backPointer;   // workhash.lookup(newDP);
    if (already) {
      inDoneList = false;  
    }
    else {
      already = finished.get(newDP);
    }

    if (already) {
      // yes, it's already there
      if (tr) {
        trs << "      looks similar to ";
        already->print(trs, *this);
        trs << endl;
      }

      // but the new item may have additional lookahead
      // components, so merge them with the old
      if (already->lookahead.merge(newItemLA)) {
        // merging changed 'already'
        if (tr) {
          trs << "      (chg) merged it to make ";
          already->print(trs, *this);
          trs << endl;
        }

        if (inDoneList) {
          // pull from the 'done' list and put in worklist, since the
          // lookahead changed
          finished.remove(already->dprod);
          CHECK_MALLOC_STATS("before worklist push");
          worklist.push(already);
          xassert(already->dprod->backPointer == NULL);   // was not on
          already->dprod->backPointer = already;          // now is on worklist
          UPDATE_MALLOC_STATS();     // allow expansion
        }
        else {
          // 'already' is in the worklist, so that's fine
        }
      }
      else {
        if (tr) {
          trs << "      this dprod already existed" << endl;
        }
      }
    }
    else {
      CHECK_MALLOC_STATS("bunch of stuff before 'if'");

      // it's not already there, so add it to worklist (but first
      // actually create it!)
      LRItem *newItem = new LRItem(numTerms, newDP);
      newItem->lookahead.copy(newItemLA);
      if (tr) {
        trs << "      this dprod is new, queueing it to add" << endl;
      }

      worklist.push(newItem);
      xassert(newItem->dprod->backPointer == NULL);
      newItem->dprod->backPointer = newItem;

      UPDATE_MALLOC_STATS();     // "new LRItem" or expansion of worklist
    }

    CHECK_MALLOC_STATS("processing of production");
  } // for each production

  CHECK_MALLOC_STATS("end of singleItemClosure");
}


// -------------- START of construct LR item sets -------------------
ItemSet *GrammarAnalysis::makeItemSet()
{
  return new ItemSet((StateId)(nextItemSetId++), 
                     numTerminals(), numNonterminals());
}

void GrammarAnalysis::disposeItemSet(ItemSet *is)
{
  // we assume we're only doing this right after making it, as the
  // point of this exercise is to avoid fragmenting the id space
  nextItemSetId--;
  xassert(is->id == nextItemSetId);
  delete is;
}


// yield (by filling 'dest') a new itemset by moving the dot across
// the productions in 'source' that have 'symbol' to the right of the
// dot; do *not* compute the closure
//
// unusedTail:
//   since 'dest' comes with a bunch of kernel items, some of which we
//   most likely won't need, put the unused ones into 'unusedTail'
//
// array:
//   since I don't want to allocate anything in here, we need scratch
//   space for computing kernel CRCs
void GrammarAnalysis::moveDotNoClosure(ItemSet const *source, Symbol const *symbol,
                                       ItemSet *dest, ObjList<LRItem> &unusedTail,
                                       GrowArray<DottedProduction const*> &array)
{
  //ItemSet *ret = makeItemSet();

  // total # of items added
  int appendCt=0;

  // iterator for walking down dest's kernel list
  ObjListMutator<LRItem> destIter(dest->kernelItems);

  // iterator for walking both lists of items; switching from an
  // implementation which used 'getAllItems' for performance reasons
  ObjListIter<LRItem> srcIter(source->kernelItems);
  int passCt=0;    // 0=kernelItems, 1=nonkernelItems
  while (passCt < 2) {
    if (passCt++ == 1) {
      srcIter.reset(source->nonkernelItems);
    }

    // for each item
    for (; !srcIter.isDone(); srcIter.adv()) {
      LRItem const *item = srcIter.data();

      if (item->isDotAtEnd() ||
          item->symbolAfterDotC() != symbol) {
        continue;    // can't move dot
      }

      // need to access destIter; if there are no more items, make more
      if (destIter.isDone()) {
        // the new item becomes the current 'data()'
        destIter.insertBefore(new LRItem(numTerminals(), NULL /*dprod*/));
      }

      // move the dot; write dot-moved item into 'destIter'
      LRItem *dotMoved = destIter.data();
      dotMoved->dprod = nextDProd(item->dprod);
      dotMoved->lookahead = item->lookahead;

      // add the new item to the itemset I'm building
      //ret->addKernelItem(dotMoved);   // UPDATE: it's already in the list
      appendCt++;
      destIter.adv();
    }
  }

  // pull out any unused items into 'unusedItems'; it's important that
  // this action not have to look at each unused item, because I want
  // to be able to make a really big scratch item list and not pay for
  // items I don't end up using
  unusedTail.stealTailAt(appendCt, dest->kernelItems);

  // verify we actually got something
  xassert(appendCt > 0);

  // we added stuff; sorting is needed both for the CRC below, and also
  // for the lookahead merge step that follows a successful lookup
  dest->sortKernelItems();

  // recompute the one thing I need to do hashing
  dest->computeKernelCRC(array);
}


// if 'list' contains something equal to 'itemSet', return that
// equal object; otherwise, return NULL
// 'list' is non-const because might return an element of it
ItemSet *GrammarAnalysis::findItemSetInList(ObjList<ItemSet> &list,
                                            ItemSet const *itemSet)
{
  // inefficiency: using iteration to check set membership

  MUTATE_EACH_OBJLIST(ItemSet, list, iter) {
    if (itemSetsEqual(iter.data(), itemSet)) {
      return iter.data();
    }
  }
  return NULL;
}


STATICDEF bool GrammarAnalysis::itemSetsEqual(ItemSet const *is1, ItemSet const *is2)
{
  // checks for equality of the kernel items
  return *is1 == *is2;
}


// keys and data are the same
STATICDEF ItemSet const *ItemSet::dataToKey(ItemSet *data)
{
  return data;
}

STATICDEF unsigned ItemSet::hash(ItemSet const *key)
{
  unsigned crc = key->kernelItemsCRC;
  return HashTable::lcprngHashFn((void*)crc);
}

STATICDEF bool ItemSet::equalKey(ItemSet const *key1, ItemSet const *key2)
{
  return *key1 == *key2;
}


// [ASU] fig 4.34, p.224
// puts the finished parse tables into 'itemSetsDone'
void GrammarAnalysis::constructLRItemSets()
{
  bool tr = tracingSys("lrsets");

  enum { BIG_VALUE = 100 };

  // item sets yet to be processed; item sets are simultaneously in
  // both the hash and the list, or not in either
  OwnerKHashArray<ItemSet, ItemSet> itemSetsPending(
    &ItemSet::dataToKey,
    &ItemSet::hash,
    &ItemSet::equalKey);

  // item sets with all outgoing links processed
  OwnerKHashTable<ItemSet, ItemSet> itemSetsDone(
    &ItemSet::dataToKey,
    &ItemSet::hash,
    &ItemSet::equalKey);
  itemSetsDone.setEnableShrink(false);

  // to avoid allocating in the inner loop, we make a single item set
  // which we'll fill with kernel items every time we think we *might*
  // make a new state, and if it turns out we really do need a new
  // state, then the kernel items in this one will be copied elsewhere
  Owner<ItemSet> scratchState(
    new ItemSet((StateId)-1 /*id*/, numTerms, numNonterms));

  // fill the scratch state with lots of kernel items to start with;
  // since these items will be re-used over and over, filling it now
  // ensures good locality on those accesses (assuming malloc returns
  // objects close together)
  enum { INIT_LIST_LEN = BIG_VALUE };
  for (int i=0; i<INIT_LIST_LEN; i++) {
    // this is a dummy item; it allocates the bitmap for 'lookahead',
    // but those bits and the 'dprod' pointer will be overwritten
    // many times during the algorithm
    LRItem *item = new LRItem(numTerms, NULL /*dottedprod*/);
    scratchState->addKernelItem(item);
  }

  // similar to the scratch state, make a scratch array for the
  // kernel CRC computation
  GrowArray<DottedProduction const*> kernelCRCArray(BIG_VALUE);

  // start by constructing closure of first production
  // (basically assumes first production has start symbol
  // on LHS, and no other productions have the start symbol
  // on LHS)
  {
    ItemSet *is = makeItemSet();              // (owner)
    startState = is;
    LRItem *firstDP
      = new LRItem(numTerms, getDProd(productions.first(), 0 /*dot at left*/));
    //firstDP->laAdd(0 /*eof token id*/);
    is->addKernelItem(firstDP);
    is->sortKernelItems();                    // redundant, but can't hurt
    itemSetClosure(*is);                      // calls changedItems internally

    // this makes the initial pending itemSet
    itemSetsPending.push(is, is);             // (ownership transfer)
  }

  // track how much allocation we're doing
  INITIAL_MALLOC_STATS();

  // for each pending item set
  while (itemSetsPending.isNotEmpty()) {
    ItemSet *itemSet = itemSetsPending.pop();          // dequeue (owner)

    CHECK_MALLOC_STATS("top of pending list loop");

    // put it in the done set; note that we must do this *before*
    // the processing below, to properly handle self-loops
    itemSetsDone.add(itemSet, itemSet);                // (ownership transfer; 'itemSet' becomes serf)

    // allows for expansion of 'itemSetsDone' hash
    UPDATE_MALLOC_STATS();

    if (tr) {
      trace("lrsets") << "state " << itemSet->id
                      << ", " << itemSet->kernelItems.count()
                      << " kernel items and "
                      << itemSet->nonkernelItems.count()
                      << " nonkernel items" << endl;
    }

    // see below; this is part of a fix for a *very* subtle heisenbug
    bool mustCloseMyself = false;

    // for each production in the item set where the
    // dot is not at the right end
    //
    // explicitly iterate over both lists because 'getAllItems'
    // does allocation
    ObjListIter<LRItem> itemIter(itemSet->kernelItems);
    int passCt=0;    // 0=kernelItems, 1=nonkernelItems
    while (passCt < 2) {
      if (passCt++ == 1) {
        itemIter.reset(itemSet->nonkernelItems);
      }

      for (; !itemIter.isDone(); itemIter.adv()) {
        LRItem const *item = itemIter.data();
        if (item->isDotAtEnd()) continue;

        CHECK_MALLOC_STATS("top of item list loop");

        if (tr) {
          ostream &trs = trace("lrsets");
          trs << "considering item ";
          item->print(trs, *this);
          trs << endl;
        }

        // get the symbol 'sym' after the dot (next to be shifted)
        Symbol const *sym = item->symbolAfterDotC();

        // in LALR(1), two items might have different lookaheads; more
        // likely, re-expansions needs to propagate lookahead that
        // wasn't present from an earlier expansion
        if (!LALR1) {
          // if we already have a transition for this symbol,
          // there's nothing more to be done
          if (itemSet->transitionC(sym) != NULL) {
            continue;
          }
        }

        // compute the itemSet (into 'scratchState') produced by moving
        // the dot across 'sym'; don't take closure yet since we
        // first want to check whether it is already present
        //
        // this call also yields the unused remainder of the kernel items,
        // so we can add them back in at the end
        ObjList<LRItem> unusedTail;
        moveDotNoClosure(itemSet, sym, scratchState,
                         unusedTail, kernelCRCArray);
        ItemSet *withDotMoved = scratchState;    // clarify role from here down

        CHECK_MALLOC_STATS("moveDotNoClosure");

        // see if we already have it, in either set
        ItemSet *already = itemSetsPending.lookup(withDotMoved);
        bool inDoneList = false;
        if (already == NULL) {
          already = itemSetsDone.get(withDotMoved);
          inDoneList = true;    // used if 'already' != NULL
        }

        // have it?
        if (already != NULL) {
          // we already have a state with at least equal kernel items, not
          // considering their lookahead sets; so we have to merge the
          // computed lookaheads with those in 'already'
          if (withDotMoved->mergeLookaheadsInto(*already)) {
            if (tr) {
              trace("lrsets")
                << "from state " << itemSet->id << ", found that the transition "
                << "on " << sym->name << " yielded a state similar to "
                << already->id << ", but with different lookahead" << endl;
            }

            CHECK_MALLOC_STATS("mergeLookaheadsInto");

            // this changed 'already'; recompute its closure
            if (already != itemSet) {
              itemSetClosure(*already);
            }
            else {
              // DANGER!  I'm already iterating over 'itemSet's item lists,
              // and if I execute the closure algorithm it will invalidate
              // my iterator.  so, postpone it
              mustCloseMyself = true;
            }

            // and reconsider all of the states reachable from it
            if (!inDoneList) {
              // itemSetsPending contains 'already', it will be processed later
            }
            else {
              // we thought we were done with this
              xassertdb(itemSetsDone.get(already));

              // but we're not: move it back to the 'pending' list
              itemSetsDone.remove(already);
              itemSetsPending.push(already, already);
            }

            // it's ok if closure makes more items, or if
            // the pending list expands
            UPDATE_MALLOC_STATS();
          }

          // we already have it, so throw away one we made
          // UPDATE: we didn't allocate, so don't deallocate
          //disposeItemSet(withDotMoved);     // deletes 'withDotMoved'

          // and use existing one for setting the transition function
          withDotMoved = already;
        }
        else {
          // we don't already have it; need to actually allocate & copy
          withDotMoved = makeItemSet();
          FOREACH_OBJLIST(LRItem, scratchState->kernelItems, iter) {
            withDotMoved->addKernelItem(new LRItem( *(iter.data()) ));
          }

          // finish it by computing its closure
          itemSetClosure(*withDotMoved);

          // then add it to 'pending'
          itemSetsPending.push(withDotMoved, withDotMoved);

          // takes into account:
          //   - creation of 'withDotMoved' state
          //   - creation of items to fill its kernel
          //   - creation of nonkernel items during closure
          //   - possible expansion of the 'itemSetsPending' hash
          UPDATE_MALLOC_STATS();
        }

        // setup the transition function
        itemSet->setTransition(sym, withDotMoved);

        // finally, restore 'scratchState's kernel item list
        scratchState->kernelItems.concat(unusedTail);

        // make sure the link restoration process works as expected
        xassertdb(scratchState->kernelItems.count() >= INIT_LIST_LEN);

        CHECK_MALLOC_STATS("end of item loop");

      } // for each item
    } // 0=kernel, 1=nonkernel

    CHECK_MALLOC_STATS("end of item set loop");

    // now that we're finished iterating over the items, I can do the
    // postponed closure
    if (mustCloseMyself) {
      itemSetClosure(*itemSet);
      UPDATE_MALLOC_STATS();
    }

  } // for each item set

  // we're done constructing item sets, so move all of them out
  // of the 'itemSetsDone' hash and into 'this->itemSets'
  try {
    for (OwnerKHashTableIter<ItemSet, ItemSet> iter(itemSetsDone);
         !iter.isDone(); iter.adv()) {
      itemSets.prepend(iter.data());
    }
    itemSetsDone.disownAndForgetAll();
  }
  catch (...) {
    breaker();
    itemSetsDone.disownAndForgetAll();
    throw;
  }

  // since we sometimes consider a state more than once, the
  // states end up out of order; put them back in order
  itemSets.mergeSort(ItemSet::diffById);


  traceProgress(1) << "done with LR sets: " << itemSets.count()
                   << " states\n";


  // do the BFS now, since we want to print the sample inputs
  // in the loop that follows
  traceProgress(1) << "BFS tree on transition graph...\n";
  computeBFSTree();

  if (tracingSys("itemset-graph")) {
    // write this info to a graph applet file
    ofstream out("lrsets.g");
    if (!out) {
      xsyserror("ofstream open");
    }
    out << "# lr sets in graph form\n";

    FOREACH_OBJLIST(ItemSet, itemSets, itemSet) {
      itemSet.data()->writeGraph(out, *this);
    }
  }
}


// print each item set
void GrammarAnalysis::printItemSets(ostream &os) const
{
  FOREACH_OBJLIST(ItemSet, itemSets, itemSet) {
    os << "State " << itemSet.data()->id
       << ", sample input: " << sampleInput(itemSet.data()) << "\n"
       << "  and left context: " << leftContextString(itemSet.data()) << "\n"
       ;
    itemSet.data()->print(os, *this);
    os << "\n\n";
  }
}


// --------------- END of construct LR item sets -------------------


Symbol const *GrammarAnalysis::
  inverseTransitionC(ItemSet const *source, ItemSet const *target) const
{
  // for each symbol..
  FOREACH_TERMINAL(terminals, t) {
    // see if it is the one
    if (source->transitionC(t.data()) == target) {
      return t.data();
    }
  }

  FOREACH_NONTERMINAL(nonterminals, nt) {
    if (source->transitionC(nt.data()) == target) {
      return nt.data();
    }
  }

  xfailure("GrammarAnalysis::inverseTransitionC: no transition from source to target");
  return NULL;     // silence warning
}


void GrammarAnalysis::computeReachable()
{
  // start by clearing the reachability flags
  MUTATE_EACH_NONTERMINAL(nonterminals, iter) {
    iter.data()->reachable = false;
  }
  
  // do a DFS on the grammar, marking things reachable as
  // they're encountered
  computeReachableDFS(startSymbol);
}


void GrammarAnalysis::computeReachableDFS(Nonterminal *nt)
{
  if (nt->reachable) {
    // already looked at this nonterminal
    return;
  }
  nt->reachable = true;

  // iterate over this nonterminal's rules
  SFOREACH_PRODUCTION(productionsByLHS[nt->ntIndex], iter) {
    // iterate over symbols in the rule RHS
    FOREACH_OBJLIST(Production::RHSElt, iter.data()->right, jter) {
      Production::RHSElt const *elt = jter.data();

      if (elt->sym->isNonterminal()) {
        // recursively analyze nonterminal elements
        computeReachableDFS(elt->sym->ifNonterminal());
      }
      else {
        // just mark terminals
        elt->sym->reachable = true;
      }
    }
  }
}


// --------------- LR support -------------------
// find and print all the conflicts that would be reported by
// an SLR(1) parser; this is a superset of the conflicts reported
// by bison, which is LALR(1); found conflicts are printed with
// trace("conflict")
void GrammarAnalysis::findSLRConflicts(int &sr, int &rr)
{
  xfailure("this code is superceded by computeParseTables");

  // for every item set..
  MUTATE_EACH_OBJLIST(ItemSet, itemSets, itemSet) {

    // we want to print something special for the first conflict
    // in a state, so track which is first
    bool printedConflictHeader = false;

    // for every input symbol..
    FOREACH_TERMINAL(terminals, t) {
      // check it
      checkSLRConflicts(itemSet.data(), t.data(), printedConflictHeader, sr, rr);
    }
  }
}


// given a parser state and an input symbol determine if we will fork
// the parse stack; return true if there is a conflict
bool GrammarAnalysis
  ::checkSLRConflicts(ItemSet *state, Terminal const *sym,
                      bool &printedConflictHeader /*inout*/,
                      int &sr /*inout*/, int &rr /*inout*/)
{
  xfailure("this code is superceded by computeParseTables");

  // see where a shift would go
  ItemSet const *shiftDest = state->transition(sym);

  // get all possible reductions where 'sym' is in Follow(LHS)
  ProductionList reductions;
  state->getPossibleReductions(reductions, sym, false /*parsing*/);

  // how many actions are there?
  int actions = (shiftDest? 1 : 0) + reductions.count();
  if (actions <= 1) {
    return false;      // no conflict
  }

  // count how many warning suppressions we have
  int dontWarns = 0;

  if (shiftDest != NULL) {
    // we have (at least) a shift/reduce conflict, which is the
    // situation in which prec/assoc specifications are used; consider
    // all the possible reductions, so we can resolve S/R conflicts
    // even when there are R/R conflicts present too
    SObjListMutator<Production> mut(reductions);
    while (!mut.isDone() && shiftDest != NULL) {
      Production const *prod = mut.data();

      bool keepShift=true, keepReduce=true, dontWarn=false;
      handleShiftReduceConflict(keepShift, keepReduce, dontWarn, state, prod, sym);

      if (!keepShift) {
        state->removeShift(sym);
        actions--;
        shiftDest = NULL;
      }

      if (!keepReduce) {
        state->removeReduce(prod, sym);
        actions--;
        mut.remove();
      }
      else {
        mut.adv();
      }

      if (dontWarn) {
        dontWarns++;
      }
    }

    // there is still a potential for misbehavior.. e.g., if there are two
    // possible reductions (R1 and R2), and one shift (S), then the user
    // could have specified prec/assoc to disambiguate, e.g.
    //   R1 < S
    //   S < R2
    // so that R2 is the right choice; but if I consider (S,R2) first,
    // I'll simply drop S, leaving no way to disambiguate R1 and R2 ..
    // for now I'll just note the possibility...
  }

  // after the disambiguation, maybe now there's no conflicts?
  // or, if conflicts remain, did we get at least that many warning
  // suppressions?
  if ((actions-dontWarns) <= 1) {
    return false;
  }

  if (!printedConflictHeader) {
    trace("conflict")
      << "--------- state " << state->id << " ----------\n"
      << "left context: " << leftContextString(state)
      << endl
      << "sample input: " << sampleInput(state)
      << endl
      ;           
    printedConflictHeader = true;
  }

  trace("conflict")
    << "conflict for symbol " << sym->name
    << endl;

  if (shiftDest) {
    trace("conflict") << "  shift, and move to state " << shiftDest->id << endl;
    sr++;                 // shift/reduce conflict
    rr += actions - 2;    // any reduces beyond first are r/r errors
  }                                                                 
  else {
    rr += actions - 1;    // all reduces beyond first are r/r errors
  }

  SFOREACH_PRODUCTION(reductions, prod) {
    trace("conflict") << "  reduce by rule " << *(prod.data()) << endl;
  }

  return true;    // found conflict
}


// decide what to do, and record the result into the two
// boolean reference parameters
void GrammarAnalysis::handleShiftReduceConflict(
  bool &keepShift, bool &keepReduce, bool &dontWarn,
  ItemSet const *state, Production const *prod, Terminal const *sym)
{
  // say that we're considering this conflict
  trace("prec")
    << "in state " << state->id << ", S/R conflict on token "
    << sym->name << " with production " << *prod << endl;

  if (!( prod->precedence && sym->precedence )) {
    // one of the two doesn't have a precedence specification,
    // so we can do nothing
    trace("prec") << "will SPLIT because no disambiguation spec available" << endl;
    return;
  }

  if (prod->precedence > sym->precedence) {
    // production's precedence is higher, so we choose to reduce
    // instead of shift
    trace("prec") << "resolved in favor of REDUCE due to precedence\n";
    keepShift = false;
    return;
  }

  if (prod->precedence < sym->precedence) {
    // symbol's precedence is higher, so we shift
    trace("prec") << "resolved in favor of SHIFT due to precedence\n";
    keepReduce = false;
    return;
  }

  // precedences are equal, so we look at associativity (of token)
  switch (sym->associativity) {
    case AK_LEFT:
      trace("prec") << "resolved in favor of REDUCE due to associativity\n";
      keepShift = false;
      return;

    case AK_RIGHT:
      trace("prec") << "resolved in favor of SHIFT due to associativity\n";
      keepReduce = false;
      return;

    case AK_NONASSOC:
      trace("pred") << "removed BOTH alternatives due to nonassociativity\n";
      keepShift = false;
      keepReduce = false;
      return;

    case AK_NEVERASSOC:
      // the user claimed this token would never be involved in a conflict
      trace("pred") << "neverassoc specification ERROR\n";
      errors++;
      cout << "token " << sym->name << " was declared 'prec', "
           << "but it is involved in an associativity conflict with \""
           << *prod << "\" in state " << state->id << endl;
      return;

    case AK_SPLIT:
      // the user does not want disambiguation of this
      trace("pred") << "will SPLIT because user asked to\n";
      dontWarn = true;
      return;

    default:
      xfailure("bad assoc code");
  }
}


// given an LR transition graph, compute the BFS tree on top of it
// and set the parent links to record the tree
void GrammarAnalysis::computeBFSTree()
{
  // for the BFS, we need a queue of states yet to be processed, and a
  // pile of 'done' states
  SObjList<ItemSet> queue;
  SObjList<ItemSet> done;

  // initial entry in queue is root of BFS tree
  queue.append(startState);

  // it will be convenient to have all the symbols in a single list
  // for iteration purposes
  SymbolList allSymbols;       	  // (const list)
  {
    FOREACH_TERMINAL(terminals, t) {
      allSymbols.append(const_cast<Terminal*>(t.data()));
    }
    FOREACH_NONTERMINAL(nonterminals, nt) {
      allSymbols.append(const_cast<Nonterminal*>(nt.data()));
    }
  }

  // loop until the queue is exhausted
  while (queue.isNotEmpty()) {
    // dequeue first element
    ItemSet *source = queue.removeAt(0);

    // mark it as done so we won't consider any more transitions to it
    done.append(source);

    // for each symbol...
    SFOREACH_SYMBOL(allSymbols, sym) {
      // get the transition on this symbol
      ItemSet *target = source->transition(sym.data());

      // if the target is done or already enqueued, or there is no
      // transition on this symbol, we don't need to consider it
      // further
      if (target == NULL ||
          done.contains(target) ||
          queue.contains(target)) {
        continue;
      }

      // the source->target link just examined is the first time
      // we've encounted 'target', so that link becomes the BFS
      // parent link
      target->BFSparent = source;

      // finally, enqueue the target so we'll explore its targets too
      queue.append(target);
    }
  }
}


// --------------- parse table construction -------------------
// given some potential parse actions, apply available disambiguation
// to remove some of them; print warnings about conflicts, in some
// situations
void GrammarAnalysis::resolveConflicts(
  ItemSet const *state,        // parse state in which the actions are possible
  Terminal const *sym,         // lookahead symbol for these actions
  ItemSet const *&shiftDest,   // (inout) if non-NULL, the state to which we can shift
  ProductionList &reductions,  // (inout) list of possible reductions
  bool allowAmbig,             // if false, always return at most 1 action
  bool &printedConflictHeader, // (inout) true once we've printed the state header
  int &sr, int &rr)            // (inout) counts of S/R and R/R conflicts, resp.
{
  // how many actions are there?
  int actions = (shiftDest? 1 : 0) + reductions.count();
  if (actions <= 1) {
    return;      // no conflict
  }

  // count how many warning suppressions we have
  int dontWarns = 0;

  // at the moment, I only have static disambiguation for S/R conflicts
  if (shiftDest) {
    // we have (at least) a shift/reduce conflict, which is the
    // situation in which prec/assoc specifications are used; consider
    // all the possible reductions, so we can resolve S/R conflicts
    // even when there are R/R conflicts present too
    SObjListMutator<Production> mut(reductions);
    while (!mut.isDone() && shiftDest != NULL) {
      Production const *prod = mut.data();

      bool keepShift=true, keepReduce=true, dontWarn=false;
      handleShiftReduceConflict(keepShift, keepReduce, dontWarn, state, prod, sym);

      if (!keepShift) {
        //state->removeShift(sym);
        actions--;
        shiftDest = NULL;
      }

      if (!keepReduce) {
        //state->removeReduce(prod, sym);
        actions--;
        mut.remove();
      }
      else {
        mut.adv();
      }

      if (dontWarn) {
        dontWarns++;
      }
    }

    // there is still a potential for misbehavior.. e.g., if there are two
    // possible reductions (R1 and R2), and one shift (S), then the user
    // could have specified prec/assoc to disambiguate, e.g.
    //   R1 < S
    //   S < R2
    // so that R2 is the right choice; but if I consider (S,R2) first,
    // I'll simply drop S, leaving no way to disambiguate R1 and R2 ..
    // for now I'll just note the possibility...
  }

  // after the disambiguation, maybe now there's no conflicts?
  // or, if conflicts remain, did we get at least that many warning
  // suppressions?
  if ((actions-dontWarns) <= 1) {
    // don't print information about conflicts
  }
  else {
    // print conflict info
    if (!printedConflictHeader) {
      trace("conflict")
        << "--------- state " << state->id << " ----------\n"
        << "left context: " << leftContextString(state)
        << endl
        << "sample input: " << sampleInput(state)
        << endl
        ;
      printedConflictHeader = true;
    }

    trace("conflict")
      << "conflict for symbol " << sym->name
      << endl;

    if (shiftDest) {
      trace("conflict") << "  shift, and move to state " << shiftDest->id << endl;
      sr++;                 // shift/reduce conflict
      rr += actions - 2;    // any reduces beyond first are r/r errors
    }
    else {
      rr += actions - 1;    // all reduces beyond first are r/r errors
    }

    SFOREACH_PRODUCTION(reductions, prod) {
      trace("conflict") << "  reduce by rule " << *(prod.data()) << endl;
    }
  }

  if (!allowAmbig && actions > 1) {
    // force only one action, using Bison's disambiguation:
    //   - prefer shift to reduce
    //   - prefer the reduction which occurs first in the grammar file
    if (shiftDest) {
      reductions.removeAll();
    }
    else {
      while (reductions.count() >= 2) {
        // compare first and second
        Production const *first = reductions.nth(0);
        Production const *second = reductions.nth(1);

        // production indices happen to be assigned in file order
        if (first->prodIndex < second->prodIndex) {
          reductions.removeItem(second);
        }
        else {
          reductions.removeItem(first);
        }
      }
    }
  }
}


void GrammarAnalysis::computeParseTables(bool allowAmbig)
{
  tables = new ParseTables(numTerms, numNonterms, itemSets.count(), numProds);

  // count total number of conflicts of each kind
  int sr=0, rr=0;

  // for each state...
  FOREACH_OBJLIST(ItemSet, itemSets, stateIter) {
    ItemSet const *state = stateIter.data();
    bool printedConflictHeader = false;

    // ---- fill in this row in the action table ----
    // for each possible lookahead...
    for (int termId=0; termId < numTerms; termId++) {
      Terminal const *terminal = getTerminal(termId);

      // can shift?
      ItemSet const *shiftDest = state->transitionC(terminal);

      // can reduce?
      ProductionList reductions;
      state->getPossibleReductions(reductions, terminal,
                                   false /*parsing*/);

      // try to resolve conflicts; this may print warnings about
      // the conflicts, depending on various factors; if 'allowAmbig'
      // is false, this will remove all but one action
      resolveConflicts(state, terminal, shiftDest, reductions,
                       allowAmbig, printedConflictHeader, sr, rr);

      // what to do in this cell
      ActionEntry cellAction;

      // still conflicts?
      int actions = (shiftDest? 1 : 0) + reductions.count();
      if (actions >= 2) {
        // make a new ambiguous-action entry
        ActionEntry *entry = new ActionEntry[actions+1];
        entry[0] = actions;

        // fill in the actions
        int index = 1;
        if (shiftDest) {
          entry[index++] = tables->encodeShift(shiftDest->id);
        }
        SFOREACH_PRODUCTION(reductions, prodIter) {
          entry[index++] = tables->encodeReduce(prodIter.data()->prodIndex);
        }
        xassert(index == actions+1);

        // (prepare to) add this entry to the action table
        cellAction = tables->encodeAmbig(tables->numAmbig());
        tables->ambigAction.push(entry);
      }

      else {
        // single action
        if (shiftDest) {
          xassert(reductions.count() == 0);
          cellAction = tables->encodeShift(shiftDest->id);
        }
        else if (reductions.isNotEmpty()) {
          xassert(reductions.count() == 1);
          cellAction = tables->encodeReduce(reductions.first()->prodIndex);
        }
        else {
          cellAction = tables->encodeError();
        }
      }

      // add this entry to the table
      tables->actionEntry(state->id, termId) = cellAction;
    }

    // ---- fill in this row in the goto table ----
    // for each nonterminal...
    for (int nontermId=0; nontermId<numNonterms; nontermId++) {
      Nonterminal const *nonterminal = getNonterminal(nontermId);

      // where do we go when we reduce to this nonterminal?
      ItemSet const *gotoDest = state->transitionC(nonterminal);

      GotoEntry cellGoto;
      if (gotoDest) {
        cellGoto = tables->encodeGoto(gotoDest->id);
      }
      else {
        // this should never be accessed at parse time..
        cellGoto = tables->encodeGotoError();
      }

      // fill in entry
      tables->gotoEntry(state->id, nontermId) = cellGoto;
    }
    
    // get the state symbol
    tables->stateSymbol[state->id] = 
      encodeSymbolId(state->getStateSymbolC());
  }

  // report on conflict counts
  if (sr + rr > 0) {
    cout << sr << " shift/reduce conflicts and "
         << rr << " reduce/reduce conflicts\n";
  }

  // fill in 'prodInfo'
  for (int p=0; p<numProds; p++) {
    Production const *prod = getProduction(p);

    tables->prodInfo[p].rhsLen = prod->rhsLength();
    tables->prodInfo[p].lhsIndex = prod->left->ntIndex;
  }
}


SymbolId encodeSymbolId(Symbol const *sym)
{
  int ret;
  if (!sym) {
    ret = 0;
  }
  else if (sym->isTerminal()) {
    ret = sym->asTerminalC().termIndex + 1;
  }
  else /*nonterminal*/ {
    ret = - sym->asNonterminalC().ntIndex - 1;
  }      
  
  // verify encoding is lossless
  SymbolId ret2 = (SymbolId)ret;
  xassert((int)ret2 == ret);
  return ret2;
}


// --------------- sample inputs -------------------
// yield a sequence of names of symbols (terminals and nonterminals) that
// will lead to the given state, from the start state
string GrammarAnalysis::leftContextString(ItemSet const *state) const
{
  SymbolList ctx;
  leftContext(ctx, state);                // get as list
  return symbolSequenceToString(ctx);	  // convert to string
}


// yield the left-context as a sequence of symbols
// CONSTNESS: want output as list of const pointers
void GrammarAnalysis::leftContext(SymbolList &output,
                                  ItemSet const *state) const
{
  // since we have the BFS tree, generating sample input (at least, if
  // it's allowed to contain nonterminals) is a simple matter of walking
  // the tree towards the root

  // for each parent..
  while (state->BFSparent) {
    // get that parent
    ItemSet *parent = state->BFSparent;

    // find a symbol on which we would transition from the parent
    // to the current state
    Symbol const *sym = inverseTransitionC(parent, state);

    // prepend that symbol's name to our current context
    output.prepend(const_cast<Symbol*>(sym));

    // move to our parent and repeat
    state = parent;
  }
}


// compare two-element quantities where one dominates and the other is
// only for tie-breaking; return <0/=0/>0 if a's quantities are
// fewer/equal/grearter (this fn is a candidate for adding to a
// library somewhere)
int priorityCompare(int a_dominant, int b_dominant,
                    int a_recessive, int b_recessive)
{
  if (a_dominant < b_dominant) return -1;
  if (a_dominant > b_dominant) return +1;
  return a_recessive - b_recessive;
}

int priorityFewer(int a_dominant, int b_dominant,
                  int a_recessive, int b_recessive)
{
  return priorityCompare(a_dominant, b_dominant,
                         a_recessive, b_recessive) < 1;
}


// sample input (terminals only) that can lead to a state
string GrammarAnalysis::sampleInput(ItemSet const *state) const
{
  // get left-context as terminals and nonterminals
  SymbolList symbols;
  leftContext(symbols, state);

  // reduce the nonterminals to terminals
  TerminalList terminals;
  if (!rewriteAsTerminals(terminals, symbols)) {
    return string("(failed to reduce!!)");
  }
  
  // convert to a string
  return terminalSequenceToString(terminals);
}


// given a sequence of symbols (terminals and nonterminals), use the
// productions to rewrite it as a (hopefully minimal) sequence of
// terminals only; return true if it works, false if we get stuck
// in an infinite loop
// CONSTNESS: ideally, 'output' would contain const ptrs to terminals
bool GrammarAnalysis::rewriteAsTerminals(TerminalList &output, SymbolList const &input) const
{
  // we detect looping by noticing if we ever reduce via the same
  // production more than once in a single vertical recursive slice
  ProductionList reductionStack;      // starts empty

  // start the recursive version
  return rewriteAsTerminalsHelper(output, input, reductionStack);
}


// (nonterminals and terminals) -> terminals;
// if this returns false, it's guaranteed to return with 'output'
// unchanged from when the function was invoked
bool GrammarAnalysis::
  rewriteAsTerminalsHelper(TerminalList &output, SymbolList const &input,
                           ProductionList &reductionStack) const
{
  // remember the initial 'output' length so we can restore
  int origLength = output.count();

  // walk down the input list, creating the output list by copying
  // terminals and reducing nonterminals
  SFOREACH_SYMBOL(input, symIter) {
    Symbol const *sym = symIter.data();

    if (sym->isEmptyString) {
      // easy; no-op
    }

    else if (sym->isTerminal()) {
      // no sweat, just copy it (er, copy the pointer)
      output.append(const_cast<Terminal*>(&sym->asTerminalC()));
    }

    else {
      // not too bad either, just reduce it, sticking the result
      // directly into our output list
      if (!rewriteSingleNTAsTerminals(output, &sym->asNonterminalC(),
                                      reductionStack)) {
        // oops.. restore 'output'
        while (output.count() > origLength) {
          output.removeAt(origLength);
        }
        return false;
      }
    }
  }

  // ok!
  return true;
}


// for rewriting into sequences of terminals, we prefer rules with
// fewer nonterminals on the RHS, and then (to break ties) rules with
// fewer RHS symbols altogether; overriding all of this, if one
// production's RHS contains a symbol already expanded, and the other
// does not, then prefer the RHS which hasn't already been expanded
int compareProductionsForRewriting(Production const *p1, Production const *p2, 
                                   void *extra)
{                                             
  ProductionList *reductionStack = (ProductionList*)extra;
   
  bool p1RHSSeen=false, p2RHSSeen=false;
  SFOREACH_PRODUCTION(*reductionStack, iter) {
    if (p1->rhsHasSymbol( iter.data()->left )) {
      p1RHSSeen = true;
    }
    if (p2->rhsHasSymbol( iter.data()->left )) {
      p2RHSSeen = true;
    }
  }                                     
  
  if (p1RHSSeen != p2RHSSeen) {  
    // e.g.: p1RHSSeen=true, so p2 is preferred; this will yield +1,
    // meaning p1>p2, so p2 comes first in an increasing order sort
    return (int)p1RHSSeen - (int)p2RHSSeen;
  }

  return priorityCompare(p1->numRHSNonterminals(), p2->numRHSNonterminals(),
                         p1->rhsLength(), p2->rhsLength());
}

// nonterminal -> terminals
// CONSTNESS: want 'reductionStack' to be list of const ptrs
bool GrammarAnalysis::
  rewriteSingleNTAsTerminals(TerminalList &output, Nonterminal const *nonterminal,
                             ProductionList &reductionStack) const
{
  // get all of 'nonterminal's productions that are not recursive
  ProductionList candidates;
  FOREACH_PRODUCTION(productions, prodIter) {
    Production const *prod = prodIter.data();
    if (prod->left != nonterminal) continue;

    // if 'prod' has 'nonterminal' on RHS, that would certainly
    // lead to looping (though it's not the only way -- consider
    // mutual recursion), so don't even consider it
    if (prod->rhsHasSymbol(nonterminal)) {
      continue;
    }

    // if this production has already been used, don't use it again
    if (reductionStack.contains(prod)) {
      continue;
    }

    // it's a candidate
    candidates.prepend(const_cast<Production*>(prod));   // constness
  }

  if (candidates.isEmpty()) {
    // I don't expect this... either the NT doesn't have any rules,
    // or all of them are recursive (which means the language doesn't
    // have any finite sentences)
    trace("rewrite") << "couldn't find any unused, non-recursive rules for "
                     << nonterminal->name << endl;
    return false;
  }

  // sort them into order of preference
  candidates.mergeSort(compareProductionsForRewriting, &reductionStack);

  // try each in turn until one succeeds; this effectively uses
  // backtracking when one fails
  bool retval = false;
  SFOREACH_PRODUCTION(candidates, candIter) {
    Production const *prod = candIter.data();

    // add chosen production to the stack
    reductionStack.prepend(const_cast<Production*>(prod));

    // now, the chosen rule provides a RHS, which is a sequence of
    // terminals and nonterminals; recursively reduce that sequence
    SymbolList rhsSymbols;
    prod->getRHSSymbols(rhsSymbols);
    retval = rewriteAsTerminalsHelper(output, rhsSymbols, reductionStack);

    // remove chosen production from stack
    Production *temp = reductionStack.removeFirst();
    xassert(temp == prod);

    if (retval) {
      // success!
      break;
    }
    else {
      // failed; try the next production
    }
  }

  // and we succeed only if we found a valid rewriting
  return retval;
}

// --------------- END of sample inputs -------------------


// this is mostly [ASU] algorithm 4.7, p.218-219: an SLR(1) parser
void GrammarAnalysis::lrParse(char const *input)
{
  // tokenize the input
  StrtokParse tok(input, " \t");

  // parser state
  int currentToken = 0;               // index of current token
  int state = startState->id;         // current parser state
  ArrayStack<int> stateStack;         // stack of parser states; top==state
  stateStack.push(state);
  ArrayStack<Symbol const*> symbolStack;    // stack of shifted symbols

  // for each token of input
  while (currentToken < tok) {
    // map the token text to a symbol
    Terminal *symbol = findTerminal(tok[currentToken]);     // (constness)

    // consult action table
    ActionEntry action = tables->actionEntry(state, symbol->termIndex);

    // see what kind of action it is
    if (tables->isShiftAction(action)) {
      // shift
      int destState = tables->decodeShift(action);

      // push current state and symbol
      state = destState;
      stateStack.push(state);
      symbolStack.push(symbol);

      // next input symbol
      currentToken++;

      // debugging
      trace("parse")
        << "moving to state " << state
        << " after shifting symbol " << symbol->name << endl;
    }

    else if (tables->isReduceAction(action)) {
      // reduce
      int prodIndex = tables->decodeReduce(action);
      ParseTables::ProdInfo const &info = tables->prodInfo[prodIndex];

      // it is here that an action or tree-building step would
      // take place

      // pop as many symbols off stacks as there are symbols on
      // the right-hand side of 'prod'
      stateStack.popMany(info.rhsLen);
      state = stateStack.top();
      symbolStack.popMany(info.rhsLen);

      // find out where to go
      int destState = tables->decodeGoto(
        tables->gotoEntry(state, info.lhsIndex));

      // go there
      state = destState;
      stateStack.push(state);

      // and push the reduced nonterminal
      symbolStack.push(getNonterminal(info.lhsIndex));

      // debugging
      trace("parse")
        << "moving to state " << state
        << " after reducing by rule id " << prodIndex << endl;
    }

    else if (tables->isErrorAction(action)) {
      // error
      trace("parse")
        << "no actions defined for symbol " << symbol->name
        << " in state " << state << endl;
      break;       // stop parsing
    }

    else {
      // conflict
      trace("parse")
        << "conflict for symbol " << symbol->name
        << " in state " << state
        << "; possible actions:\n";

      // get actions
      int ambigId = tables->decodeAmbigAction(action);
      ActionEntry *entry = tables->ambigAction[ambigId];

      // explain each one
      for (int i=0; i<entry[0]; i++) {
        action = entry[i+1];
        if (tables->isShiftAction(action)) {
          trace("parse") << "  shift, and move to state "
                         << tables->decodeShift(action) << endl;
        }
        else if (tables->isReduceAction(action)) {
          trace("parse") << "  reduce by rule id "
                         << tables->decodeReduce(action) << endl;
        }
        else {
          // no other alternative makes sense
          xfailure("bad code in ambigAction table");
        }
      }

      break;       // stop parsing
    }
  }

  // print final contents of stack; if the parse was successful,
  // I want to see what remains; if not, it's interesting anyway
  trace("parse") << "final contents of stacks (right is top):\n";

  ostream &os = trace("parse") << "  state stack:";
  int i;
  for (i=0; i < stateStack.length(); i++) {
    os << " " << stateStack[i];
  }
  os << " <-- current" << endl;

  os << "  symbol stack:";
  for (i=0; i < symbolStack.length(); i++) {
    os << " " << symbolStack[i]->name;
  }
  os << endl;
}


// ---------------------------- main --------------------------------
void pretendUsed(...)
{}


void GrammarAnalysis::exampleGrammar()
{
  // for now, let's use a hardcoded grammar



  #if 0
    // grammar 4.13 of [ASU] (p.191)
    parseLine("Start  ->  S $                ");
    parseLine("S  ->  i E t S S'   |  a      ");
    parseLine("S' ->  e S          |  empty  ");
    parseLine("E  ->  b                      ");
  #endif // 0


  #if 0
    // grammar 4.11 of [ASU] (p.189), designed to show First and Follow
    parseLine("S  ->  E $                ");
    parseLine("E  ->  T E'               ");
    parseLine("E' ->  + T E'  | empty    ");
    parseLine("T  ->  F T'               ");
    parseLine("T' ->  * F T'  | empty    ");
    parseLine("F  ->  ( E )   | id       ");
  #endif // 0


  #if 0
    // terminals: "a", "b", .. "e"
    char const termLetters[] = "abcde";
    Terminal *terms[5];
    loopi(5) {
      char s[2];          // will be e.g. "b\0"
      s[0] = termLetters[i];
      s[1] = 0;
      terms[i] = new Terminal(s);
      terminals.append(terms[i]);
    }

    // give then convenient names
    Terminal *a = terms[0];
    Terminal *b = terms[1];
    Terminal *c = terms[2];
    Terminal *d = terms[3];
    Terminal *e = terms[4];

    // nonterminals
    char const * const nontermNames[] = {
      "Start", "A", "B", "C", "D"
    };
    Nonterminal *nonterms[5];
    loopi(5) {
      nonterms[i] = new Nonterminal(nontermNames[i]);
      nonterminals.append(nonterms[i]);
    }

    // give them convenient names
    Nonterminal *S = nonterms[0];
    Nonterminal *A = nonterms[1];
    Nonterminal *B = nonterms[2];
    Nonterminal *C = nonterms[3];
    Nonterminal *D = nonterms[4];

    // start symbol
    startSymbol = S;

    // productions
    #define E  S
    #define Ep A
    #define T  B
    #define Tp C
    #define F  D
    #define plus a
    #define times b
    #define lparen c
    #define rparen d
    #define id e
    addProduction(E,  /* -> */   T, Ep,               NULL);
    addProduction(Ep, /* -> */   plus, T, Ep,         NULL);
    addProduction(Ep, /* -> */   &emptyString,        NULL);
    addProduction(T,  /* -> */   F, Tp,               NULL);
    addProduction(Tp, /* -> */   times, F, Tp,        NULL);
    addProduction(Tp, /* -> */   &emptyString,        NULL);
    addProduction(F,  /* -> */   lparen, E, rparen,   NULL);
    addProduction(F,  /* -> */   id,                  NULL);
  #endif // 0

  #if 0
    addProduction(S,  /* -> */   A, B, A, C,    NULL);
    addProduction(A,  /* -> */   B,             NULL);
    addProduction(B,  /* -> */   &emptyString,  NULL);
    addProduction(C,  /* -> */   A, D,          NULL);
    addProduction(D,  /* -> */   a,             NULL);
  #endif // 0

  #if 0
    addProduction(S,  /* -> */   A, B, A, C,    NULL);
    addProduction(A,  /* -> */   &emptyString,  NULL);
    addProduction(B,  /* -> */   C, D, C,       NULL);
    addProduction(C,  /* -> */   &emptyString,  NULL);
    addProduction(D,  /* -> */   a,             NULL);
  #endif // 0

  #if 1
    #if 0
      // [ASU] grammar 4.19, p.222: demonstrating LR sets-of-items construction
      parseLine("E' ->  E $                ");
      parseLine("E  ->  E + T  |  T        ");
      parseLine("T  ->  T * F  |  F        ");
      parseLine("F  ->  ( E )  |  id       ");
    #else
      // get that grammar from a file instead
      readGrammarFile(*this, "asu419.gr");
    #endif

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
    // [ASU] grammar 4.20, p.229: more sets-of-items
    parseLine("S' ->  S $                 ");
    parseLine("S  ->  L = R               ");
    parseLine("S  ->  R                   ");
    parseLine("L  ->  * R                 ");
    parseLine("L  ->  id                  ");
    parseLine("R  ->  L                   ");

    char const *input[] = {
      " id                 $",
      " id = id            $",
      " * id = id          $",
      " id = * id          $",
      " * id = * id        $",
      " * * id = * * id    $"
    };
  #endif // 0


  // verify we got what we expected
  printProductions(trace("grammar") << endl);


  // run analyses
  runAnalyses(NULL);


  // do some test parses
  INTLOOP(i, 0, (int)TABLESIZE(input)) {
    trace("parse") << "------ parsing: `" << input[i] << "' -------\n";
    lrParse(input[i]);
  }
}


void GrammarAnalysis::runAnalyses(char const *setsFname)
{            
  // prepare for symbol of interest
  {
    char const *name = getenv("SYM_OF_INTEREST");
    if (name != NULL) {
      symOfInterest = findSymbolC(name);
      if (!symOfInterest) {
        cout << "warning: " << name << " isn't in the grammar\n";
      }
    }
  }

  // reset error count so it might be possible to reuse the object
  // for another grammar
  errors = 0;

  checkWellFormed();

  // precomputations
  traceProgress(1) << "init...\n";
  initializeAuxData();

  traceProgress(1) << "derivability relation...\n";
  computeWhatCanDeriveWhat();

  traceProgress(1) << "first...\n";
  computeFirst();
  computeDProdFirsts();

  traceProgress(1) << "follow...\n";
  computeFollow();

  // print results
  {
    ostream &tracer = trace("terminals") << "Terminals:\n";
    printSymbols(tracer, toObjList(terminals));
  }
  {
    ostream &tracer = trace("nonterminals") << "Nonterminals:\n";
    tracer << "  " << emptyString << endl;
    printSymbols(tracer, toObjList(nonterminals));
  }

  if (tracingSys("derivable")) {
    derivable->print();
  }

  // testing closure
  #if 0
  {
    // make a singleton set out of the first production, and
    // with the dot at the start
    ObjList<LRItem> itemSet;
    LRItem *kernel = productions.nth(0)->getDProd(0);  // (serf)
    itemSet.append(kernel);

    // compute its closure
    itemSetClosure(itemSet);

    // print it
    cout << "Closure of: ";
    kernel->print(cout);
    cout << endl;
                 
    SFOREACH_OBJLIST(LRItem, itemSet, dprod) {
      cout << "  ";
      dprod.data()->print(cout);
      cout << endl;
    }
  }
  #endif // 0


  // LR stuff
  traceProgress(1) << "LR item sets...\n";
  constructLRItemSets();

  traceProgress(1) << "parse tables...\n";
  computeParseTables(!tracingSys("deterministic"));

  #if 0     // old code; need it for just a while longer
  {
    int sr=0, rr=0;           // numbers of each kind of conflict
    findSLRConflicts(sr, rr);
    if (sr + rr > 0) {
      cout << sr << " shift/reduce conflicts and "
           << rr << " reduce/reduce conflicts\n";
    }
  }
  #endif // 0

  // if we want to print, do so before throwing away the items
  if (tracingSys("itemsets")) {
    printProductionsAndItems(cout, true /*code*/);
  }

  // open debug output file
  ofstream *setsOutput = NULL;
  if (setsFname) {
    setsOutput = new ofstream(setsFname);
    if (!*setsOutput) {
      cout << "couldn't open " << setsFname << " to write item sets\n";
      delete setsOutput;
      setsOutput = NULL;
    }
  }

  // count the number of unreachable nonterminals & terminals
  {                   
    if (setsOutput) {
      *setsOutput << "unreachable nonterminals:\n";
    }
    int ct=0;
    FOREACH_NONTERMINAL(nonterminals, iter) {
      if (!iter.data()->reachable) {
        ct++;

        if (setsOutput) {
          *setsOutput << "  " << iter.data()->name << "\n";
        }
      }
    }

    if (ct > 0) {
      cout << "grammar contains " << ct << " unreachable nonterminals\n";
      // bison also reports the number of productions under all the
      // unreachable nonterminals, but that doesn't seem especially
      // useful to me
    }

    if (setsOutput) {
      *setsOutput << "unreachable terminals:\n";
    }
    ct=0;
    FOREACH_TERMINAL(terminals, jter) {
      if (!jter.data()->reachable) {
        ct++;

        if (setsOutput) {
          *setsOutput << "  " << jter.data()->name << "\n";
        }
      }
    }

    if (ct > 0) {
      cout << "grammar contains " << ct << " unreachable terminals\n";
    }
  }

  // print the item sets
  if (setsOutput) {
    traceProgress() << "printing item sets to " << setsFname << " ..." << endl;
    *setsOutput << "NOTE: Item set numbers can change depending on what flags\n"
                << "are passed to 'gramanl'!\n";
    printItemSets(*setsOutput);
  }

  // print information about all tokens
  if (setsOutput) {
    *setsOutput << "terminals:\n";
    FOREACH_TERMINAL(terminals, iter) {
      Terminal const *t = iter.data();
      *setsOutput << "  ";
      t->print(*setsOutput);
      *setsOutput << "\n";
    }

    // and nonterminals
    *setsOutput << "nonterminals:\n";
    FOREACH_NONTERMINAL(nonterminals, ntIter) {
      Nonterminal const *nt = ntIter.data();
      *setsOutput << "  ";
      nt->print(*setsOutput);
      *setsOutput << "\n";
    }

    // and productions
    *setsOutput << "productions:\n";
    for (int p=0; p<numProds; p++) {
      *setsOutput << "  ";
      getProduction(p)->print(*setsOutput);
      *setsOutput << "\n";
    }
  }


  delete setsOutput;

  // I don't need (most of) the item sets during parsing, so
  // throw them away once I'm done analyzing the grammar
  MUTATE_EACH_OBJLIST(ItemSet, itemSets, iter) {
    iter.data()->throwAwayItems();
  }


  // another analysis
  //computePredictiveParsingTable();

  // silence warnings
  //pretendUsed(a,b,c,d,e, S,A,B,C,D);
}


// ------------------ emitting action code -----------------------
// prototypes for this section
void emitActionCode(Grammar const &g, char const *hFname,
                    char const *ccFname, char const *srcFname);
void emitUserCode(EmitCode &out, LocString const &code);
void emitActions(Grammar const &g, EmitCode &out, EmitCode &dcl);
void emitDupDelMerge(Grammar const &g, EmitCode &out, EmitCode &dcl);
void emitFuncDecl(Grammar const &g, EmitCode &out, EmitCode &dcl,
                  char const *rettype, char const *params);
void emitDDMInlines(Grammar const &g, EmitCode &out, EmitCode &dcl,
                    Symbol const &sym);
void emitSwitchCode(Grammar const &g, EmitCode &out,
                    char const *signature, char const *switchVar,
                    ObjList<Symbol> const &syms, int whichFunc,
                    char const *templateCode, char const *actUpon);


// yield the name of the inline function for this production; naming
// design motivated by desire to make debugging easier
string actionFuncName(Production const &prod)
{
  return stringc << "action" << prod.prodIndex
                 << "_" << prod.left->name;
}


// emit the user's action code to a file
void emitActionCode(Grammar const &g, char const *hFname,
                    char const *ccFname, char const *srcFname)
{
  EmitCode dcl(hFname);
  if (!dcl) {
    throw_XOpen(hFname);
  }

  dcl << "// " << hFname << "\n";
  dcl << "// *** DO NOT EDIT BY HAND ***\n";
  dcl << "// automatically generated by gramanl, from " << srcFname << "\n";
  dcl << "\n";
  dcl << "// the following declarations are intended to be #included\n";
  dcl << "// directly into the " << g.actionClassName << " class declaration\n";
  dcl << "\n";
  dcl << "USER_ACTION_FUNCTIONS      // see useract.h\n";
  dcl << "\n";

  EmitCode out(ccFname);
  if (!out) {
    throw_XOpen(ccFname);
  }

  out << "// " << ccFname << "\n";
  out << "// *** DO NOT EDIT BY HAND ***\n";
  out << "// automatically generated by gramanl, from " << srcFname << "\n";
  out << "\n";
  out << "#include <assert.h>      // assert\n";
  out << "#include \"useract.h\"     // SemanticValue\n";
  out << "\n";

  // insert user's verbatim code at top
  if (g.verbatim[0]) {
    out << lineDirective(g.verbatim);
    out << g.verbatim << "\n";
    out << restoreLine;
    out << "\n";
  }

  NOSOURCELOC(
    out << "// parser-originated location information is disabled by\n"
        << "// NO_GLR_SOURCELOC; any rule which refers to 'loc' will get this one\n"
        << "SourceLocation loc;      // defaults to no-location\n"
        << "\n\n";
  )

  out << "// ------------------- actions ------------------\n";
  emitActions(g, out, dcl);
  out << "\n";
  out << "\n";

  emitDupDelMerge(g, out, dcl);
}


void emitUserCode(EmitCode &out, LocString const &code)
{
  out << "{\n";
  if (code.validLoc()) {
    out << lineDirective(code);
  }

  // the final brace is on the same line so errors reported at the
  // last brace go to user code
  out << code << " }\n";

  if (code.validLoc()) {
    out << restoreLine;
  }
  out << "\n";
}


// yield the given type, but if it's NULL, then yield
// something to use instead
char const *typeString(char const *type, LocString const &tag)
{
  if (!type) {
    cout << tag.locString() << ": Production tag \"" << tag
         << " on a symbol with no type.\n";
    return "__error_no_type__";     // will make compiler complain
  }
  else {
    return type;
  }
}


// return true if the type starts with the word "enum"
bool isEnumType(char const *type)
{
  return 0==strncmp(type, "enum", 4);
}


void emitActions(Grammar const &g, EmitCode &out, EmitCode &dcl)
{
  // iterate over productions, emitting inline action functions
  {FOREACH_OBJLIST(Production, g.productions, iter) {
    Production const &prod = *(iter.data());

    // there's no syntax for a typeless nonterminal, so this shouldn't
    // be triggerable by the user
    xassert(prod.left->type);

    // put the production in comments above the defn
    out << "// " << prod.toString() << "\n";

    out << "inline " << prod.left->type << " "
        << g.actionClassName << "::" << actionFuncName(prod)
        << "("
        SOURCELOC( << "SourceLocation const &loc" )
        ;

    dcl << prod.left->type << " " << actionFuncName(prod) << "("
        SOURCELOC( << "SourceLocation const &loc" )
        ;

    int ct=0;
    SOURCELOC( ct++ );    // if we printed the 'loc' param, count it

    // iterate over RHS elements, emitting formals for each with a tag
    FOREACH_OBJLIST(Production::RHSElt, prod.right, rhsIter) {
      Production::RHSElt const &elt = *(rhsIter.data());
      if (elt.tag.length() == 0) continue;

      if (ct++ > 0) {
        out << ", ";
        dcl << ", ";
      }

      out << typeString(elt.sym->type, elt.tag);
      dcl << typeString(elt.sym->type, elt.tag);

      // the tag becomes the formal parameter's name
      out << " " << elt.tag;
      dcl << " " << elt.tag;
    }

    out << ")\n";
    dcl << ");\n";

    // now insert the user's code, to execute in this environment of
    // properly-typed semantic values
    emitUserCode(out, prod.action);
  }}

  out << "\n";

  // main action function; calls the inline functions emitted above
  out << "SemanticValue " << g.actionClassName << "::doReductionAction(\n"
      << "  int productionId, SemanticValue const *semanticValues"
      SOURCELOC( << ",\n  SourceLocation const &loc" )
      << ")\n";
  out << "{\n";
  out << "  switch (productionId) {\n";

  // iterate over productions
  FOREACH_OBJLIST(Production, g.productions, iter) {
    Production const &prod = *(iter.data());

    out << "    case " << prod.prodIndex << ":\n";
    out << "      return (SemanticValue)" << actionFuncName(prod) << "(" 
        SOURCELOC( << "loc" )
        ;

    // iterate over RHS elements, emitting arguments for each with a tag
    int index = -1;      // index into 'semanticValues'
    int ct=0;
    SOURCELOC( ct++ );   // count 'loc' if it is passed
    FOREACH_OBJLIST(Production::RHSElt, prod.right, rhsIter) {
      Production::RHSElt const &elt = *(rhsIter.data());

      // we have semantic values in the array for all RHS elements,
      // even if they didn't get a tag
      index++;

      if (elt.tag.length() == 0) continue;

      if (ct++ > 0) {
        out << ", ";
      }

      // cast SemanticValue to proper type
      out << "(" << typeString(elt.sym->type, elt.tag) << ")";
      if (isEnumType(elt.sym->type)) {
        // egcs-1.1.2 complains when I cast from void* to enum, even
        // when there is a cast!  so let's put an intermediate cast
        // to int
        out << "(int)";
      }
      out << "(semanticValues[" << index << "])";
    }

    out << ");\n";
  }

  out << "    default:\n";
  out << "      assert(!\"invalid production code\");\n";
  out << "      return (SemanticValue)0;   // silence warning\n";
  out << "  }\n";
  out << "}\n";
}


void emitDupDelMerge(Grammar const &g, EmitCode &out, EmitCode &dcl)
{
  out << "// ---------------- dup/del/merge/keep nonterminals ---------------\n";
  // emit inlines for dup/del/merge of nonterminals
  FOREACH_OBJLIST(Nonterminal, g.nonterminals, ntIter) {
    emitDDMInlines(g, out, dcl, *(ntIter.data()));
  }

  // emit dup-nonterm
  emitSwitchCode(g, out,
    "SemanticValue $acn::duplicateNontermValue(int nontermId, SemanticValue sval)",
    "nontermId",
    (ObjList<Symbol> const&)g.nonterminals,
    0 /*dupCode*/,
    "      return (SemanticValue)dup_$symName(($symType)sval);\n",
    NULL);

  // emit del-nonterm
  emitSwitchCode(g, out,
    "void $acn::deallocateNontermValue(int nontermId, SemanticValue sval)",
    "nontermId",
    (ObjList<Symbol> const&)g.nonterminals,
    1 /*delCode*/,
    "      del_$symName(($symType)sval);\n"
    "      return;\n",
    "deallocate nonterm");

  // emit merge-nonterm
  emitSwitchCode(g, out,
    "SemanticValue $acn::mergeAlternativeParses(int nontermId, SemanticValue left,\n"
    "                                           SemanticValue right)",
    "nontermId",
    (ObjList<Symbol> const&)g.nonterminals,
    2 /*mergeCode*/,
    "      return (SemanticValue)merge_$symName(($symType)left, ($symType)right);\n",
    "merge nonterm");

  // emit keep-nonterm
  emitSwitchCode(g, out,
    "bool $acn::keepNontermValue(int nontermId, SemanticValue sval)",
    "nontermId",
    (ObjList<Symbol> const&)g.nonterminals,
    3 /*keepCode*/,
    "      return keep_$symName(($symType)sval);\n",
    NULL);


  out << "\n";
  out << "// ---------------- dup/del/classify terminals ---------------\n";
  // emit inlines for dup/del of terminals
  FOREACH_OBJLIST(Terminal, g.terminals, termIter) {
    emitDDMInlines(g, out, dcl, *(termIter.data()));
  }

  // emit dup-term
  emitSwitchCode(g, out,
    "SemanticValue $acn::duplicateTerminalValue(int termId, SemanticValue sval)",
    "termId",
    (ObjList<Symbol> const&)g.terminals,
    0 /*dupCode*/,
    "      return (SemanticValue)dup_$symName(($symType)sval);\n",
    NULL);

  // emit del-term
  emitSwitchCode(g, out,
    "void $acn::deallocateTerminalValue(int termId, SemanticValue sval)",
    "termId",
    (ObjList<Symbol> const&)g.terminals,
    1 /*delCode*/,
    "      del_$symName(($symType)sval);\n"
    "      return;\n",
    "deallocate terminal");

  // emit classify-term
  emitSwitchCode(g, out,
    "int $acn::reclassifyToken(int oldTokenType, SemanticValue sval)",
    "oldTokenType",
    (ObjList<Symbol> const&)g.terminals,
    4 /*classifyCode*/,
    "      return classify_$symName(($symType)sval);\n",
    NULL);
}


// emit both the function decl for the .h file, and the beginning of
// the function definition for the .cc file
void emitFuncDecl(Grammar const &g, EmitCode &out, EmitCode &dcl,
                  char const *rettype, char const *params)
{
  out << "inline " << rettype << " " << g.actionClassName
      << "::" << params;

  dcl << rettype << " " << params << ";\n";
}


void emitDDMInlines(Grammar const &g, EmitCode &out, EmitCode &dcl,
                    Symbol const &sym)
{
  Terminal const *term = sym.ifTerminalC();
  Nonterminal const *nonterm = sym.ifNonterminalC();

  if (sym.dupCode) {
    emitFuncDecl(g, out, dcl, sym.type,
      stringc << "dup_" << sym.name
              << "(" << sym.type << " " << sym.dupParam << ") ");
    emitUserCode(out, sym.dupCode);
  }

  if (sym.delCode) {
    emitFuncDecl(g, out, dcl, "void",
      stringc << "del_" << sym.name
              << "(" << sym.type << " "
              << (sym.delParam? sym.delParam : "") << ") ");
    emitUserCode(out, sym.delCode);
  }

  if (nonterm && nonterm->mergeCode) {
    emitFuncDecl(g, out, dcl, sym.type,
      stringc << "merge_" << sym.name
              << "(" << sym.type << " " << nonterm->mergeParam1
              << ", " << sym.type << " " << nonterm->mergeParam2 << ") ");
    emitUserCode(out, nonterm->mergeCode);
  }

  if (nonterm && nonterm->keepCode) {
    emitFuncDecl(g, out, dcl, "bool",
      stringc << "keep_" << sym.name
              << "(" << sym.type << " " << nonterm->keepParam << ") ");
    emitUserCode(out, nonterm->keepCode);
  }

  if (term && term->classifyCode) {
    emitFuncDecl(g, out, dcl, "int",
      stringc << "classify_" << sym.name
              << "(" << sym.type << " " << term->classifyParam << ") ");
    emitUserCode(out, term->classifyCode);
  }
}

void emitSwitchCode(Grammar const &g, EmitCode &out,
                    char const *signature, char const *switchVar,
                    ObjList<Symbol> const &syms, int whichFunc,
                    char const *templateCode, char const *actUpon)
{
  out << replace(signature, "$acn", g.actionClassName) << "\n"
         "{\n"
         "  switch (" << switchVar << ") {\n";

  FOREACH_OBJLIST(Symbol, syms, symIter) {
    Symbol const &sym = *(symIter.data());

    if (whichFunc==0 && sym.dupCode ||
        whichFunc==1 && sym.delCode ||
        whichFunc==2 && sym.asNonterminalC().mergeCode ||
        whichFunc==3 && sym.asNonterminalC().keepCode ||
        whichFunc==4 && sym.asTerminalC().classifyCode) {
      out << "    case " << sym.getTermOrNontermIndex() << ":\n";
      out << replace(replace(templateCode,
               "$symName", sym.name),
               "$symType", sym.type);
    }
  }

  out << "    default:\n";
  if (whichFunc == 0) {
    // unspecified dup: return NULL
    out << "      return (SemanticValue)0;\n";
  }
  else if (whichFunc == 3) {
    // unspecified keep functions default to keep
    out << "      return true;\n";
  }
  else if (whichFunc == 4) {
    // default classifier keeps existing classification
    out << "      return oldTokenType;\n";
  }
  else {
    out << "      cout << \"there is no action to " << actUpon << " id \"\n"
           "           << " << switchVar << " << endl;\n"
           "      abort();\n";
  }

  out << "  }\n"
         "}\n"
         "\n";
}


// ------------------------- main --------------------------
#ifdef GRAMANL_MAIN

#include "grampar.h"      // readGrammarFile
#include "bflatten.h"     // BFlatten
#include <stdio.h>        // remove
#include <stdlib.h>       // system


int main(int argc, char **argv)
{
  char const *progName = argv[0];
  TRACE_ARGS();
  traceAddSys("progress");

  #if 0     // useful for testing lrParse()
  // for now
  GrammarAnalysis g;
  g.exampleGrammar();
  pretendUsed(progName);
  #endif // 0

  #if 1
  bool testRW = false;
  if (argc >= 2 &&
      0==strcmp(argv[1], "--testRW")) {
    testRW = true;
    argc--;
    argv++;
  }

  if (argc != 2) {
    cout << "usage: " << progName << " [-tr traceFlags] [--testRW] prefix\n"
            "  processes prefix.gr to make prefix.{gr.{gen.h,gen.cc},bin}\n"
            "  useful tracing flags (separate with commas):\n"
            "    conflict      : print LALR(1) conflicts\n"
            "    deterministic : force a deterministic parser\n"
            "    closure       : details of item-set closure algorithm\n"
            "    prec          : show how prec/assoc are used to resolve conflicts\n"
            "    explore       : start the interactive grammar explorer at the end\n"
            "    lrtable       : print entire LR parsing tables to prefix.gr.gen.out\n"
            ;
    return 0;
  }
  string prefix = argv[1];


  bool printCode = true;

  string grammarFname = stringc << prefix << ".gr";
  GrammarAnalysis g;
  readGrammarFile(g, grammarFname);
  g.printProductions(trace("grammar") << endl);

  string setsFname = stringc << prefix << ".gr.gen.out";
  g.runAnalyses(tracingSys("lrtable")? setsFname.pcharc() : NULL);
  if (g.errors) {
    return 2;
  }

  // print some stuff to a test file
  char const g1Fname[] = "gramanl.g1.tmp";
  if (testRW) {
    traceProgress() << "printing ascii grammar " << g1Fname << endl;
    {
      ofstream out(g1Fname);
      g.printProductionsAndItems(out, printCode);
    }
  }

  // emit some C++ code
  string hFname = stringc << prefix << ".gr.gen.h";
  string ccFname = stringc << prefix << ".gr.gen.cc";
  traceProgress() << "emitting C++ code to " << ccFname
                  << " and " << hFname << " ...\n";

  emitActionCode(g, hFname, ccFname, grammarFname);

  // before using 'xfer' we have to tell it about the string table
  flattenStrTable = &grammarStringTable;

  // write the analyzed grammar to a file
  string binFname = stringc << prefix << ".bin";
  traceProgress() << "writing binary grammar file " << binFname << endl;
  {
    BFlatten flatOut(binFname, false /*reading*/);
    g.xfer(flatOut);
  }

  // write it in a bison-compatible format as well
  {                     
    string bisonFname = stringc << prefix << ".gr.gen.y";
    traceProgress() << "writing bison-compatible grammar to " << bisonFname << endl;
    ofstream out(bisonFname);
    g.printAsBison(out);
  }

  if (testRW) {
    // to test what happens on a fresh read, blank the string table
    grammarStringTable.clear();

    // read in the binary file
    traceProgress() << "reading binary grammar file " << binFname << endl;
    BFlatten flatIn(binFname, true /*reading*/);
    GrammarAnalysis g2;
    g2.xfer(flatIn);

    // print same stuff to another file
    char const g2Fname[] = "gramanl.g2.tmp";
    traceProgress() << "printing ascii grammar " << g2Fname << endl;
    {
      ofstream out(g2Fname);
      g2.printProductionsAndItems(out, printCode);
    }

    // diff 'em
    traceProgress() << "comparing ascii grammar\n";
    if (system(stringc << "diff " << g1Fname << " " << g2Fname) != 0) {
      cout << "the grammars differ!!\n";
      return 4;
    }

    // remove the temps
    if (!tracingSys("keep-tmp")) {
      remove(g1Fname);
      remove(g2Fname);
    }

    cout << "ticksComputeNonkernel: " << ticksComputeNonkernel << endl;
    cout << "testRW SUCCESS!\n";
  }

  else {
    // I want to know how long writing takes
    traceProgress() << "done\n";
  }

  if (tracingSys("explore")) {
    grammarExplorer(g);
  }
  #endif // 1

  return 0;
}

#endif // GRAMANL_MAIN
