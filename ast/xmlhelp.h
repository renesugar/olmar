// xmlhelp.h            see license.txt for copyright and terms of use
// included by generated ast code

// Generic serialization and de-serialization support.

#ifndef XMLHELP_H
#define XMLHELP_H

#include "str.h"         // string
#include "srcloc.h"      // SourceLoc

typedef unsigned int xmlUniqueId_t;

// manage identity canonicality; we now map addresses one to one to a
// sequence number; this means that the ids should be canonical now
// given isomorphic inputs
xmlUniqueId_t mapAddrToUniqueId(void const * const addr);

// manage identity of AST; FIX: I am not absolutely sure that we are
// not accidentally using this for user classes instead of just AST
// classes; to be absolutely sure, make a superclass of all of the AST
// classes and make the argument here take a pointer to that.
xmlUniqueId_t uniqueIdAST(void const * const obj);

// print a unique id with prefix, for example "FL12345678"; guaranteed
// to print (e.g.) "FL0" for NULL pointers; the "FL" part is the label
string xmlPrintPointer(char const *label, xmlUniqueId_t id);

// 2006-05-05 these used to take an rostring, but I changed them to const char
// *, because these functions are performance-critical and they were not
// strings until now, so don't allocate a string just to call these functions.

// I have manually mangled the name to include "_bool" or "_int" as
// otherwise what happens is that if a toXml() for some enum flag is
// missing then the C++ compiler will just use the toXml(bool)
// instead, which is a bug.

// string toXml_bool(bool b);
static inline const char * toXml_bool(bool b) { return b ? "true" : "false"; }
void fromXml_bool(bool &b, const char *str);

// string toXml_int(int i);
static inline int toXml_int(int i) { return i; }
void fromXml_int(int &i, const char *str);

// string toXml_long(long i);
static inline long toXml_long(long i) { return i; }
void fromXml_long(long &i, const char *str);

// string toXml_unsigned_int(unsigned int i);
static inline unsigned int toXml_unsigned_int(unsigned int i) { return i; }
void fromXml_unsigned_int(unsigned int &i, const char *str);

// string toXml_unsigned_long(unsigned long i);
static inline unsigned long toXml_unsigned_long(unsigned long i) { return i; }
void fromXml_unsigned_long(unsigned long &i, const char *str);

// string toXml_double(double x);
static inline double toXml_double(double i) { return i; }
void fromXml_double(double &x, const char *str);

string toXml_SourceLoc(SourceLoc loc);
void fromXml_SourceLoc(SourceLoc &loc, const char *str);

// output SRC with encoding and quotes around it.
ostream &outputXmlAttrQuoted(ostream &o, const char *src);
static inline ostream &outputXmlAttrQuoted(ostream &o, string const &src)
{ return outputXmlAttrQuoted(o, src.c_str()); }

// Output SRC with quotes, but no encoding.  Only use with objects that do not
// contain ["'<>&]
//
// Works for any type with ostream insertion operators, e.g. const char*,
// string, int, ...  Avoiding going to a string improves serialization
// performance a lot.
template <typename T>
inline
ostream &outputXmlAttrQuotedNoEscape(ostream &o, T src)
{
  return o << '\'' << src << '\'';
}

// for quoting and unquoting xml attribute strings
string xmlAttrQuote(const char *src);
inline string xmlAttrQuote(rostring src) { return xmlAttrQuote(src.c_str()); }
// string xmlAttrEncode(char const *src);
// string xmlAttrEncode(char const *p, int len);


// Use of xmlAttrDeQuote() is now almost certainly an error since the lexer
// returns dequoted/unescaped strings.

// string xmlAttrDeQuote(const char *text);
// // dsw: This function does not process all XML escapes.  I only
// // process the ones that I use in the partner encoding function
// // xmlAttrEncode().
// string xmlAttrDecode(char const *src, const char *end, char delim);

#endif // XMLHELP_H
