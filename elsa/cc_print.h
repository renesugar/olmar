// cc_print.h            see license.txt for copyright and terms of use
// declarations for C++ pretty-printer; the AST entry
// points are declared in cc.ast

#ifndef CC_PRINT_H
#define CC_PRINT_H

#include "cc_ast.h"             // C++ AST; this module
#include "str.h"                // stringBuilder

#include <iostream.h>           // ostream

// this virtual semi-abstract class is intended to act as a
// "superclass" for ostream, stringBuilder, and any other "output
// stream" classes
class OutStream {
  public:
  virtual ~OutStream() {}

  // special-case methods
  virtual OutStream & operator << (ostream& (*manipfunc)(ostream& outs)) = 0;
  virtual void flush() = 0;

  // special method to support rostring
  virtual OutStream & operator << (rostring message) = 0;

  // generic methods
  #define MAKE_INSERTER(type) \
    virtual OutStream &operator << (type message) = 0;
  MAKE_INSERTER(char const *)
  MAKE_INSERTER(char)
  MAKE_INSERTER(bool)
  MAKE_INSERTER(int)
  MAKE_INSERTER(unsigned int)
  MAKE_INSERTER(long)
  MAKE_INSERTER(unsigned long)
  MAKE_INSERTER(double)
  #undef MAKE_INSERTER
};

class StringBuilderOutStream : public OutStream {
  stringBuilder &buffer;

  public:
  StringBuilderOutStream(stringBuilder &buffer0) : buffer(buffer0) {}

  // special-case methods
  virtual StringBuilderOutStream & operator << (ostream& (*manipfunc)(ostream& outs)) {
    buffer << "\n";             // assume that it is endl
    return *this;
  }
  virtual void flush() {}       // no op

  // special method to support rostring
  virtual OutStream & operator << (rostring message) {return operator<< (message.c_str());}

  // generic methods
  #define MAKE_INSERTER(type)        \
    virtual StringBuilderOutStream &operator << (type message) \
    {                                \
      buffer << message;             \
      return *this;                  \
    }
  MAKE_INSERTER(char const *)
  MAKE_INSERTER(char)
  MAKE_INSERTER(bool)
  MAKE_INSERTER(int)
  MAKE_INSERTER(unsigned int)
  MAKE_INSERTER(long)
  MAKE_INSERTER(unsigned long)
  MAKE_INSERTER(double)
  #undef MAKE_INSERTER
};

class OStreamOutStream : public OutStream {
  ostream &out;

  public:
  OStreamOutStream(ostream &out0) : out(out0) {}

  // special-case methods
  virtual OStreamOutStream & operator << (ostream& (*manipfunc)(ostream& outs)) {
    out << manipfunc;
    return *this;
  }
  virtual void flush() { out.flush(); }

  // special method to support rostring
  virtual OutStream & operator << (rostring message) {return operator<< (message.c_str());}

  // generic methods
  #define MAKE_INSERTER(type)        \
    virtual OStreamOutStream &operator << (type message) \
    {                                \
      out << message;                \
      return *this;                  \
    }
  MAKE_INSERTER(char const *)
  MAKE_INSERTER(char)
  MAKE_INSERTER(bool)
  MAKE_INSERTER(int)
  MAKE_INSERTER(unsigned int)
  MAKE_INSERTER(long)
  MAKE_INSERTER(unsigned long)
  MAKE_INSERTER(double)
  #undef MAKE_INSERTER
};

// indents the source code sent to it
class CodeOutStream : public OutStream {
  OutStream &out;           // output to here
  int depth;                    // depth of indentation
  int buffered_newlines;        // number of buffered trailing newlines

  public:
  CodeOutStream(OutStream &out0)
    : out(out0), depth(0), buffered_newlines(0)
  {}
  virtual ~CodeOutStream();

  // manipulate depth
  virtual void up()   {depth--;}
  virtual void down() {depth++;}

  // indentation and formatting support
  void printIndentation(int n);
  void printWhileInsertingIndentation(int n, rostring s);
  void finish();

  // OutStream methods
  CodeOutStream & operator << (ostream& (*manipfunc)(ostream& outs));
  void flush() { out.flush(); }
  CodeOutStream & operator << (char const *message);

  // special method to support rostring
  virtual CodeOutStream & operator << (rostring message) {return operator<< (message.c_str());}

  // generic methods
  #define MAKE_INSERTER(type)                     \
    CodeOutStream & operator << (type message) {  \
      out << message;                             \
      return *this;                               \
    }
  MAKE_INSERTER(char)
  MAKE_INSERTER(bool)
  MAKE_INSERTER(int)
  MAKE_INSERTER(unsigned int)
  MAKE_INSERTER(long)
  MAKE_INSERTER(unsigned long)
  MAKE_INSERTER(double)
  #undef MAKE_INSERTER
};

// print paired delimiters, the second one is delayed until the end of
// the stack frame; that is, it is printed in the destructor.
class PairDelim {
  char const *close;            // FIX: why can't I use an rostring?
  CodeOutStream &out;

  public:
  PairDelim(CodeOutStream &out, rostring message, rostring open, char const *close);
  PairDelim(CodeOutStream &out, rostring message);
  ~PairDelim();
};

// an output stream for printing comments that will indent them
// according to the level of the tree walk
class TreeWalkOutStream : public OutStream {
  OutStream &out;
  bool on;
  int depth;

  public:
  TreeWalkOutStream(OutStream &out, bool on = true)
    : out(out), on(on), depth(0)
  {}

  public:
  // manipulate depth
  virtual void down() {++depth;}
  virtual void up()   {--depth;}

  private:
  // indentation and formatting support
  void indent();

  public:
  // OutStream methods
  virtual TreeWalkOutStream & operator << (ostream& (*manipfunc)(ostream& outs));
  virtual void flush() { out.flush(); }

  // special method to support rostring
  virtual TreeWalkOutStream & operator << (rostring message) {return operator<< (message.c_str());}

  // generic methods
  #define MAKE_INSERTER(type)                     \
    TreeWalkOutStream & operator << (type message) { \
      if (on) {                                   \
        indent();                                 \
        out << message;                           \
      }                                           \
      return *this;                               \
    }
  MAKE_INSERTER(char const *)
  MAKE_INSERTER(char)
  MAKE_INSERTER(bool)
  MAKE_INSERTER(int)
  MAKE_INSERTER(unsigned int)
  MAKE_INSERTER(long)
  MAKE_INSERTER(unsigned long)
  MAKE_INSERTER(double)
  #undef MAKE_INSERTER
};

extern TreeWalkOutStream treeWalkOut;

// a class to make on the stack at ever frame of the tree walk that
// will automatically manage the indentation level of the
// TreeWalkOutStream given
class TreeWalkDebug {
  TreeWalkOutStream &out;
  public:
  TreeWalkDebug(char *message, TreeWalkOutStream &out = treeWalkOut);
  ~TreeWalkDebug();
};

// This class knows how to print out Types; Note: the point is to
// duplicate this class and hack on it, not to insert a bunch of hooks
// an inherit from it.  FIX: Therefore eventually, all the methods
// should be private and non-virtual and the dtor private so you can't
// inherit from it.
class TypePrinterC {
  // FIX: make this class not vtabled: remove the 'virtual' from the
  // methods; also, there is no longer a need for a dtor; make the
  // methods private.
  public:
  virtual ~TypePrinterC() {}

  public:
  void print(OutStream &out, Type const *type, char const *name = NULL);

  protected:
  // **** AtomicType
  virtual string print(AtomicType const *atomic);

  virtual string print(SimpleType const *);
  virtual string print(CompoundType const *);
  virtual string print(EnumType const *);
  virtual string print(TypeVariable const *);
  virtual string print(PseudoInstantiation const *);
  virtual string print(DependentQType const *);

  // **** [Compound]Type
  virtual string print(Type const *type);
  virtual string print(Type const *type, char const *name);
  virtual string printRight(Type const *type, bool innerParen = true);
  virtual string printLeft(Type const *type, bool innerParen = true);

  virtual string printLeft(CVAtomicType const *type, bool innerParen = true);
  virtual string printRight(CVAtomicType const *type, bool innerParen = true);
  virtual string printLeft(PointerType const *type, bool innerParen = true);
  virtual string printRight(PointerType const *type, bool innerParen = true);
  virtual string printLeft(ReferenceType const *type, bool innerParen = true);
  virtual string printRight(ReferenceType const *type, bool innerParen = true);
  virtual string printLeft(FunctionType const *type, bool innerParen = true);
  virtual string printRight(FunctionType const *type, bool innerParen = true);
  virtual string printRightUpToQualifiers(FunctionType const *type, bool innerParen);
  virtual string printRightQualifiers(FunctionType const *type, CVFlags cv);
  virtual string printRightAfterQualifiers(FunctionType const *type);
  virtual void   printExtraRightmostSyntax(FunctionType const *type, stringBuilder &);
  virtual string printLeft(ArrayType const *type, bool innerParen = true);
  virtual string printRight(ArrayType const *type, bool innerParen = true);
  virtual string printLeft(PointerToMemberType const *type, bool innerParen = true);
  virtual string printRight(PointerToMemberType const *type, bool innerParen = true);

  // **** Variable
  virtual string printAsParameter(Variable const *var);
};

// global context for a pretty-print
class PrintEnv {
  public:
  TypePrinterC &typePrinter;
  CodeOutStream *out;
  SourceLoc loc;
  
  public:
  PrintEnv(TypePrinterC &typePrinter0, CodeOutStream *out0)
    : typePrinter(typePrinter0)
    , out(out0)
    , loc(SL_UNKNOWN)
  {}
};

#define PRINT_AST(AST)                \
  do {                                \
    OutStream out0(cout);         \
    TypePrinter typePrinter0;         \
    PrintEnv penv0(typePrinter0);     \
    if (AST) AST->print(penv0, out0); \
    else out0 << "(PRINT_AST:null)";  \
    out0 << endl;                     \
  } while(0)

#endif // CC_PRINT_H
