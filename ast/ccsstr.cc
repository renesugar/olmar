// ccsstr.cc            see license.txt for copyright and terms of use
// code for ccsstr.h

#include "ccsstr.h"      // this module
#include "xassert.h"     // xassert
#include "exc.h"         // xformat
#include "strutil.h"     // string, replace

#include <iostream.h>    // cout
#include <ctype.h>       // isspace


CCSubstrate::CCSubstrate(ReportError &err)
  : EmbeddedLang(err)
{
  reset();
}

void CCSubstrate::reset(int initNest)
{
  state = ST_NORMAL;
  nesting = initNest;
  backslash = false;
  star = false;
  text.setlength(0);
}


CCSubstrate::~CCSubstrate()
{}


void CCSubstrate::handle(char const *str, int len, char finalDelim)
{
  text.append(str, len);

  for (; len>0; len--,str++) {
    switch (state) {
      case ST_NORMAL:
        switch (*str) {
          case '{':
          case '(':
          case '[':
            nesting++;
            break;

          case '}':
          case ')':
          case ']':
            if (nesting == 0) {
              err.reportError(stringc
                << "unexpected closing delimiter `" << *str
                << "' -- probably due to missing `" << finalDelim << "'");
            }
            else {
              nesting--;
            }
            break;

          case '\"':
            state = ST_STRING;
            break;

          case '\'':
            state = ST_CHAR;
            break;

          case '/':
            state = ST_SLASH;
            break;
        }
        break;

      case ST_STRING:
      case ST_CHAR:
        if (!backslash) {
          if ((state == ST_STRING && *str == '\"') ||
              (state == ST_CHAR && *str == '\'')) {
            state = ST_NORMAL;
          }
          else if (*str == '\\') {
            backslash = true;
          }
          else if (*str == '\n') {
            err.reportError("unterminated string or char literal");
          }
        }
        else {
          backslash = false;
        }
        break;

      case ST_SLASH:
        if (*str == '*') {
          state = ST_C_COMMENT;
        }
        else if (*str == '/') {
          state = ST_CC_COMMENT;
        }
        else {
          state = ST_NORMAL;
        }
        break;

      case ST_C_COMMENT:
        if (!star) {
          if (*str == '*') {
            star = true;
          }
        }
        else {
          star = false;
          if (*str == '/') {
            state = ST_NORMAL;
          }
        }
        break;

      case ST_CC_COMMENT:
        // I don't like the possibility of escaped newlines
        // in C++ comments, so I don't support it (so there!)
        if (*str == '\n') {
          state = ST_NORMAL;
        }
        break;

      default:
        xfailure("unknown state");
    }
  }
}


bool CCSubstrate::zeroNesting() const
{
  return (state == ST_NORMAL || state == ST_SLASH) &&
         nesting == 0;
}


string CCSubstrate::getFuncBody() const
{
  if (isDeclaration) {
    // I used to be appending ';' here, but now I need the flexibility to
    // add additional test before it, so I will rely on the caller to add
    // semicolons where necessary
    return text;
  }
  else if (exprOnly) {
    return stringc << "return " << text << ";";
  }
  else {
    return text;
  }
}


string CCSubstrate::getDeclName() const
{
  // go with the rather inelegant heuristic that the word
  // just before the first '(' is the function's name
  char const *start = text.pcharc();
  char const *p = start;
  
  // find first '('
  while (*p && *p!='(') { p++; }
  if (!*p) {
    xformat("missing '('");
  }             
  if (p == start) {
    xformat("missing name");
  }

  // skip backward past any whitespace before the '('
  p--;
  while (p>=start && isspace(*p)) { p--; }
  if (p<start) {
    xformat("missing name");
  }
  char const *nameEnd = p+1;    // char just past last
  
  // move backward through the name
  while (p>=start && 
         (isalnum(*p) || *p=='_'))
    { p--; }
  p++;    // move back to most recent legal char
  
  // done
  return string(p, nameEnd-p);
}


// ------------------ test code -------------------
#ifdef TEST_CCSSTR

#define CC CCSubstrate

void feed(CC &cc, char const *src)
{
  cout << "trying: " << src << endl;
  while (*src) {
    // feed it in 10 char increments, to test split processing too
    int len = min(strlen(src), 10);
    cc.handle(src, len);
    src += len;
  }
}


void test(char const *src, CC::State state, int nesting, bool flag)
{
  CC cc(simpleReportError);
  feed(cc, src);

  if (!( cc.state == state &&
         cc.nesting == nesting &&
         state==CC::ST_C_COMMENT? cc.star==flag :
                                  cc.backslash==flag )) {
    xfailure(stringc << "failed on src: " << src);
  }
}


void normal(char const *src, int nesting)
{
  test(src, CC::ST_NORMAL, nesting, false);
}

void str(char const *src, int nesting, bool bs)
{
  test(src, CC::ST_STRING, nesting, bs);

  // repeat the test with single-tick
  string another = replace(src, "\"", "\'");
  test(src, CC::ST_CHAR, nesting, bs);
}


void yes(char const *src)
{
  CC cc(simpleReportError);
  feed(cc, src);

  xassert(cc.zeroNesting());
}

void no(char const *src)
{
  CC cc(simpleReportError);
  feed(cc, src);

  xassert(!cc.zeroNesting());
}

void name(char const *body, char const *n)
{
  CC cc(simpleReportError);
  feed(cc, body);
  xassert(cc.getDeclName().equals(n));
}

void badname(char const *body)
{
  CC cc(simpleReportError);
  feed(cc, body);
  try {
    cc.getDeclName();
    xfailure("got a name when it shoudn't have!");
  }
  catch (...)
    {}
}


int main()
{
  normal("int main()", 0);
  normal("int main() { hi", 1);
  normal("int main() { hi {", 2);
  normal("int main() { hi { foo[5", 3);
  normal("int main() { hi { foo[5] and ", 2);
  normal("int main() { hi { foo[5] and } bar ", 1);
  normal("int main() { hi { foo[5] and } bar } baz ", 0);

  normal("main() { printf(\"hello \\ world\"); ret", 1);

  normal("()[]{}([{}])", 0);
  normal("{ ()[]{}([{}]) } ", 0);
  normal("( ()[]{}([{}]) )", 0);
  normal("[ ()[]{}([{}]) ]", 0);
  normal("\"foo\" ()[]{}([{}])", 0);

  str("main() { printf(\"hello", 2, false);
  str("main() { printf(\"hello \\", 2, true);
  str("main() { printf(\"hello \\ world", 2, false);
  str("main() { printf(\"hello \\ world\", \"hi", 2, false);

  test("\"a\" 'b' (", CC::ST_NORMAL, 0, false);

  yes("main() {}");
  yes("main() { printf(\"foo\", 3, 4 /*yep{*/); }");
  yes("some // junk {\n more");
  yes("'\\''");
  yes("\"\\\"\"");
  yes("[][][][][]");
  yes("\"[[[\"");
  yes("*");
  yes("/* [ /* [ */");

  no("\"");
  no("(");
  no(" ( /* ) */ ");

  name("int main()", "main");
  name("int eval(Environment &env)", "eval");
  name("man()", "man");
  badname("(");
  badname("  (");
  badname("  ");
  badname("");
  badname(")");
  badname("main");

  cout << "\nccsstr: all tests PASSED\n";

  return 0;
}

#endif // TEST_CCSSTR
