// cc_lang.cc            see license.txt for copyright and terms of use
// code for cc_lang.h

#include "cc_lang.h"     // this module

#include <string.h>      // memset


static void enableWarning(Bool3 &b)
{
  if (b == B3_TRUE) {
    b = B3_WARN;
  }
}

void CCLang::enableAllWarnings()
{
  enableWarning(allowImplicitFunctionDecls);
}


// ---------------------- ANSI and K&R C ----------------------
void CCLang::ANSI_C89()
{
  // just in case I forget to initialize something....
  memset(this, 0, sizeof(*this));

  isCplusplus = false;
  declareGNUBuiltins = false;

  tagsAreTypes = false;
  recognizeCppKeywords = false;
  implicitFuncVariable = false;
  gccFuncBehavior = GFB_none;
  noInnerClasses = true;
  uninitializedGlobalDataIsCommon = true;
  emptyParamsMeansNoInfo = true;
  complainUponBadDeref = true;
  strictArraySizeRequirements = false;
  assumeNoSizeArrayHasSizeOne = false;
  allowOverloading = false;
  compoundSelfName = false;
  allowImplicitFunctionDecls = B3_TRUE;        // C89 does not require prototypes
  allowImplicitInt = true;
  allowDynamicallySizedArrays = false;
  allowIncompleteEnums = false;
  allowMemberWithClassName = true;
  nonstandardAssignmentOperator = false;
  allowExternCThrowMismatch = true;
  allowImplicitIntForMain = false;
  predefined_Bool = false;

  treatExternInlineAsPrototype = false;
  allowNewlinesInStringLits = false;
  stringLitCharsAreConst = false; // Didn't check C89; C99 says they are non-const

  // C99 spec: Section 6.5.4, footnote 85: "A cast does not yield an lvalue".
  lvalueFlowsThroughCast = false;
  
  allowImplicitIntForOperators = false;
}

void CCLang::KandR_C()
{
  ANSI_C89();

  allowImplicitInt = true;
}

void CCLang::ANSI_C99_extensions()
{
  implicitFuncVariable = true;
  predefined_Bool = true;
}

void CCLang::ANSI_C99()
{
  ANSI_C89();

  // new features
  ANSI_C99_extensions();

  // removed C89 features
  allowImplicitInt = false;
  allowImplicitFunctionDecls = B3_FALSE;
}


// ------------------------ GNU C ------------------------
void CCLang::GNU_C_extensions()
{
  gccFuncBehavior = GFB_string;
  allowDynamicallySizedArrays = true;
  assumeNoSizeArrayHasSizeOne = true;
  treatExternInlineAsPrototype = true;
  allowNewlinesInStringLits = true;
  declareGNUBuiltins = true;

  // http://gcc.gnu.org/onlinedocs/gcc-3.1/gcc/Lvalues.html
  lvalueFlowsThroughCast = true;

  // http://gcc.gnu.org/onlinedocs/gcc-3.1/gcc/Incomplete-Enums.html
  allowIncompleteEnums = true;
}

void CCLang::GNU_C()
{
  ANSI_C89();

  ANSI_C99_extensions();
  GNU_C_extensions();
}

void CCLang::GNU_KandR_C()
{
  KandR_C();

  GNU_C_extensions();

  // this seems wrong, but Oink's tests want it this way...
  ANSI_C99_extensions();
}

void CCLang::GNU2_KandR_C()
{
  GNU_KandR_C();

  // dsw: seems to not be true for gcc 2.96 at least
  predefined_Bool = false;
}


// ---------------------------- C++ ----------------------------
void CCLang::ANSI_Cplusplus()
{
  // just in case...
  memset(this, 0, sizeof(*this));

  isCplusplus = true;
  declareGNUBuiltins = false;

  tagsAreTypes = true;
  recognizeCppKeywords = true;
  implicitFuncVariable = false;
  gccFuncBehavior = GFB_none;
  noInnerClasses = false;
  uninitializedGlobalDataIsCommon = false;
  emptyParamsMeansNoInfo = false;

  // these aren't exactly ANSI C++; they might be "pragmatic C++"
  // for the current state of the parser
  complainUponBadDeref = false;
  strictArraySizeRequirements = false;
  assumeNoSizeArrayHasSizeOne = false;

  allowOverloading = true;
  compoundSelfName = true;

  allowImplicitFunctionDecls = B3_FALSE;
  allowImplicitInt = false;
  allowDynamicallySizedArrays = false;
  allowIncompleteEnums = false;
  allowMemberWithClassName = false;

  // indeed this is nonstandard but everyone seems to do it this way ...
  nonstandardAssignmentOperator = true;

  allowExternCThrowMismatch = false;
  allowImplicitIntForMain = false;

  predefined_Bool = false;
  treatExternInlineAsPrototype = false;
  allowNewlinesInStringLits = false;
  stringLitCharsAreConst = true; // Cppstd says they are const.
  lvalueFlowsThroughCast = false;
  allowImplicitIntForOperators = false;
}

void CCLang::GNU_Cplusplus()
{
  ANSI_Cplusplus();

  gccFuncBehavior = GFB_variable;

  // is this really right?  Oink tests it like it is ...
  allowDynamicallySizedArrays = true;

  allowMemberWithClassName = true;
  allowExternCThrowMismatch = true;
  allowImplicitIntForMain = true;

  declareGNUBuiltins = true;
}


// -------------------------- MSVC ---------------------
void CCLang::MSVC_bug_compatibility()
{
  allowImplicitIntForOperators = true;
}


// EOF