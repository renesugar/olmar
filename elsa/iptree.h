// iptree.h
// interval partition tree

#ifndef IPTREE_H
#define IPTREE_H

#include "objlist.h"        // ObjList
#include "str.h"            // rostring
#include "array.h"          // GrowArray
#include "bitstrmap.h"      // BitStrMap

#include <iostream.h>       // ostream
#include <stdio.h>          // FILE
#include <values.h>         // MAXINT


// node relevance
enum Relevance {
  R_IRRELEVANT=0,           // was irrelevant at least once
  R_UNKNOWN,                // never tested
  R_RELEVANT,               // has been relevant every time tested
  NUM_RELEVANCES
};

char const *toString(Relevance r);


// result of applying the test to a variant
enum VariantResult {
  VR_UNKNOWN=0,             // never tried
  VR_PASSED,                // test passed
  VR_FAILED,                // test failed
  NUM_VRESULTS
};

char const *toString(VariantResult r);


// map a variant bitstring to its result code
typedef BitStrMap<VariantResult> VariantResults;
typedef VariantResults::Node *VariantCursor;

// debugging: print the set of mapped bitvectors
void printResults(VariantResults &results);


// node in the interval tree
class Node {
public:
  // interval endpoints, inclusive; the root node has hi=MAXINT, to
  // indicate an open upper endpoint
  int lo, hi;

  // children (subintervals), in sorted order
  ObjList<Node> children;

  // state of search for minimal element
  Relevance rel;

public:
  Node(int lo, int hi);
  ~Node();

  // true if 'this' contains or equals 'n'
  bool contains(Node const *n) const;

  // true if 'this' contains the value 'n'
  bool contains(int n) const;

  // add 'n' somewhere in the subtree rooted at 'this'
  void insert(Node *n);

  // find the smallest interval containing 'n', which must at least
  // be contained within 'this'
  Node const *queryC(int n) const;
  Node *query(int n) { return const_cast<Node*>(queryC(n)); }

  // write this node's fragment from 'source' to 'fp', returning
  // the # of bytes written
  int write(FILE *fp, GrowArray<char> const &source,
            VariantCursor &cursor) const;

  // print the range as a string, like "[1, 2]"
  string rangeString() const;

  // print this subtree to the given stream at the given level
  // of indentation
  void debugPrint(ostream &os, int ind) const;
};


// interval partition tree
class IPTree {
private:     // data
  // (owner) top of the tree
  Node *top;

public:      // funcs
  IPTree();
  ~IPTree();

  Node *getTop() { return top; }

  // add a new interval; must nest properly w.r.t. the existing intervals
  Node *insert(int lo, int hi);

  // find the smallest interval containing the given value, or NULL if
  // it is outside all intervals
  Node const *queryC(int n) const;
  Node *query(int n) { return const_cast<Node*>(queryC(n)); }

  // given an array of source characters, write to 'fname' a file
  // consisting of all the intervals that are not irrelevant; the
  // cursor is advanced to reflect the navigation path; return the
  // # of bytes written
  int write(rostring fname, GrowArray<char> const &source,
            VariantCursor &cursor) const;
                                 
  // what is the largest endpoint, other than MAXINT?
  int getLargestFiniteEndpoint();

  // debugging: print the tree to stdout
  void gdb() const;
};


// read a file into memory
void readFile(rostring fname, GrowArray<char> &dest);


#endif // IPTREE_H
