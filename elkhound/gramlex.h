// gramlex.h
// GrammarLexer: a c++ lexer class for use with Flex's generated c++ scanner

#ifndef __GRAMLEX_H
#define __GRAMLEX_H


// This included file is part of the Flex distribution.  It is
// installed in /usr/include on my Linux machine.  By including it, we
// get the declaration of the yyFlexLexer class.  Note that the file
// that flex generates, gramlex.yy.cc, also #includes this file.
// Perhaps also worth mentioning: I'm developing this with flex 2.5.4.
#include <FlexLexer.h>


// may as well pull in the token constant definitions (generated by
// Bison from grammar.y) to interpret return value of yylex()
#include "grampar.tab.h"

// a couple more tokens                            
#define TOK_EOF 0             // better name
#define TOK_INCLUDE 1         // not seen by parser


// other includes..
#include "str.h"      // string


// this class just holds the lexer state so it is properly encapsulated
// (and therefore, among other things, re-entrant)
class GrammarLexer : public yyFlexLexer {
public:      // types
  enum Constants {
    firstColumn = 1,           // how to number the columns
    firstLine = 1,             // and lines
  };

private:     // data
  int line, column;            // for reporting errors

public:      // data
  int commentStartLine;        // for reporting unterminated C comments
  int integerLiteral;          // to store number literal value
  string includeFileName;      // name in an #include directive

  // defined in the base class, FlexLexer:
  //   const char *YYText();           // start of matched text
  //   int YYLeng();                   // number of matched characters

private:     // funcs
  void newLine();              // called when a newline is encountered

public:      // funcs
  // create a new lexer that will read from stdin
  GrammarLexer();

  // clean up
  ~GrammarLexer();

  // get current token as a string
  string curToken() const;
  int curLen() const { return const_cast<GrammarLexer*>(this)->YYLeng(); }

  // read the next token and return its code; returns TOK_EOF for end of file;
  // this function is defined in flex's output
  virtual int yylex();

  // info about location of current token
  int curLine() const { return line; }
  int curCol() const;
  string curLoc() const;              // string containing line/col of current token

  // error reporting; called by the lexer code
  void err(char const *msg);          // msg should not include a newline
  void errorUnterminatedComment();
  void errorMalformedInclude();
  void errorIllegalCharacter(char ch);
};


#endif // __GRAMLEX_H
