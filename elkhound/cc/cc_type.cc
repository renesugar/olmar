// cc_type.cc            see license.txt for copyright and terms of use
// code for cc_type.h

#include "cc_type.h"    // this module
#include "trace.h"      // tracingSys
#include "variable.h"   // Variable
#include <assert.h>     // assert
#include <string.h>


#if 0
MLValue mlStorage(DeclFlags df)
{
  // storage = NoStorage | Static | Register | Extern

  // not quite a perfect map .. but until it matters
  // somewhere I'm leaving it as-is

  if (df & DF_STATIC) {
    return mlTuple0(storage_Static);
  }
  if (df & DF_REGISTER) {
    return mlTuple0(storage_Register);
  }
  if (df & DF_EXTERN) {
    return mlTuple0(storage_Extern);
  }
  return mlTuple0(storage_NoStorage);
}
#endif // 0


string makeIdComment(int id)
{
  if (tracingSys("type-ids")) {
    return stringc << "/""*" << id << "*/";
                     // ^^ this is to work around an Emacs highlighting bug
  }
  else {
    return "";
  }
}


// ------------------ AtomicType -----------------
ALLOC_STATS_DEFINE(AtomicType)

AtomicType::AtomicType()
  //: id(NULL_ATOMICTYPEID)
{
  ALLOC_STATS_IN_CTOR
}


AtomicType::~AtomicType()
{
  ALLOC_STATS_IN_DTOR
}


CAST_MEMBER_IMPL(AtomicType, SimpleType)
CAST_MEMBER_IMPL(AtomicType, CompoundType)
CAST_MEMBER_IMPL(AtomicType, EnumType)
CAST_MEMBER_IMPL(AtomicType, TypeVariable)


bool AtomicType::equals(AtomicType const *obj) const
{
  // one exception to the below: when checking for
  // equality, TypeVariables are all equivalent
  if (this->isTypeVariable() &&
      obj->isTypeVariable()) {
    return true;
  }

  // all of the AtomicTypes are unique-representation,
  // so pointer equality suffices
  //
  // it is *important* that we don't do structural equality
  // here, because then we'd be confused by types with the
  // same name that appear in different scopes!
  return this == obj;
}


template <class T>
string recurseCilString(T const *type, int depth)
{
  depth--;
  xassert(depth >= 0);     // otherwise we started < 1
  if (depth == 0) {
    // just print the id
    return stringc << "id " << type->getId();
  }
  else {
    // print id in a comment but go on to print the
    // type one level deeper
    return stringc << makeIdComment(type->getId()) << " "
                   << type->toCilString(depth);
  }
}


string AtomicType::toString(int depth) const
{
  return stringc << recurseCilString(this, depth+1)
                 << " /""* " << toCString() << " */";
}


// ------------------ SimpleType -----------------
SimpleType const SimpleType::fixed[NUM_SIMPLE_TYPES] = {
  SimpleType(ST_CHAR),
  SimpleType(ST_UNSIGNED_CHAR),
  SimpleType(ST_SIGNED_CHAR),
  SimpleType(ST_BOOL),
  SimpleType(ST_INT),
  SimpleType(ST_UNSIGNED_INT),
  SimpleType(ST_LONG_INT),
  SimpleType(ST_UNSIGNED_LONG_INT),
  SimpleType(ST_LONG_LONG),
  SimpleType(ST_UNSIGNED_LONG_LONG),
  SimpleType(ST_SHORT_INT),
  SimpleType(ST_UNSIGNED_SHORT_INT),
  SimpleType(ST_WCHAR_T),
  SimpleType(ST_FLOAT),
  SimpleType(ST_DOUBLE),
  SimpleType(ST_LONG_DOUBLE),
  SimpleType(ST_VOID),
  SimpleType(ST_ELLIPSIS),
  SimpleType(ST_CDTOR),
  SimpleType(ST_ERROR),
  SimpleType(ST_DEPENDENT),
};

string SimpleType::toCString() const
{
  return simpleTypeName(type);
}


string SimpleType::toCilString(int) const
{
  return toCString();
}


int SimpleType::reprSize() const
{
  return simpleTypeReprSize(type);
}


#if 0
#define MKTAG(n,t) MAKE_ML_TAG(typ, n, t)
MKTAG(0, TVoid)
MKTAG(1, TInt)
MKTAG(2, TBitfield)
MKTAG(3, TFloat)
MKTAG(4, Typedef)
MKTAG(5, TPtr)
MKTAG(6, TArray)
MKTAG(7, TStruct)
MKTAG(8, TUnion)
MKTAG(9, TEnum)
MKTAG(10, TFunc)
#undef MKTAG

#define MKTAG(n,t) MAKE_ML_TAG(ikind, n, t)
MKTAG(0, IChar)
MKTAG(1, ISChar)
MKTAG(2, IUChar)
MKTAG(3, IInt)
MKTAG(4, IUInt)
MKTAG(5, IShort)
MKTAG(6, IUShort)
MKTAG(7, ILong)
MKTAG(8, IULong)
MKTAG(9, ILongLong)
MKTAG(10, IULongLong)
#undef MKTAG

#define MKTAG(n,t) MAKE_ML_TAG(fkind, n, t)
MKTAG(0, FFloat)
MKTAG(1, FDouble)
MKTAG(2, FLongDouble)
#undef MKTAG


MLValue SimpleType::toMLValue(int, CVFlags cv) const
{
  // TVoid * attribute list
  // TInt of ikind * attribute list

  MLValue attrs = cvToMLAttrs(cv);

  #define TUP(t,i) return mlTuple2(typ_##t, mlTuple0(ikind_##i), attrs)
  switch (type) {
    default: xfailure("bad tag");
    case ST_CHAR:               TUP(TInt, IChar);
    case ST_UNSIGNED_CHAR:      TUP(TInt, IUChar);
    case ST_SIGNED_CHAR:        TUP(TInt, ISChar);
    case ST_BOOL:               TUP(TInt, IInt);        // ...
    case ST_INT:                TUP(TInt, IInt);
    case ST_UNSIGNED_INT:       TUP(TInt, IUInt);
    case ST_LONG_INT:           TUP(TInt, ILong);
    case ST_UNSIGNED_LONG_INT:  TUP(TInt, IULong);
    case ST_LONG_LONG:          TUP(TInt, ILongLong);
    case ST_UNSIGNED_LONG_LONG: TUP(TInt, IULongLong);
    case ST_SHORT_INT:          TUP(TInt, IShort);
    case ST_UNSIGNED_SHORT_INT: TUP(TInt, IUShort);
    case ST_WCHAR_T:            TUP(TInt, IShort);      // ...
  #undef TUP
  #define TUP(t,i) return mlTuple2(typ_##t, mlTuple0(fkind_##i), attrs)
    case ST_FLOAT:              TUP(TFloat, FFloat);
    case ST_DOUBLE:             TUP(TFloat, FDouble);
    case ST_LONG_DOUBLE:        TUP(TFloat, FLongDouble);
    case ST_VOID:               return mlTuple1(typ_TVoid, attrs);
  }
  #undef TUP
}
#endif // 0


string SimpleType::uniqueName() const
{
  return simpleTypeName(type);
}


// ------------------ NamedAtomicType --------------------
NamedAtomicType::NamedAtomicType(StringRef n)
  : name(n),
    typedefVar(NULL),
    access(AK_PUBLIC)
{}

NamedAtomicType::~NamedAtomicType()
{
  if (typedefVar) {
    delete typedefVar;
  }
}


string NamedAtomicType::uniqueName() const
{
  // 'a' for atomic
  return stringc << "a" << (int)this /*id*/ << "_" << name;
}


#if 0
MLValue NamedAtomicType::toMLValue(int depth, CVFlags cv) const
{
  // we break the circularity at the entry to named atomics;
  // we'll emit typedefs for all of them beforehand
  xassert(depth >= 1);
  depth--;

  if (depth == 0) {
    // Typedef of string * int * typ ref * attribute list

    return mlTuple4(typ_Typedef,
                    mlString(uniqueName()),
                    mlInt(id),
                    mlRef(mlNil()),      // to be set in a post-process
                    cvToMLAttrs(cv));
  }
  else {
    // full info
    return toMLContentsValue(depth, cv);
  }
}    
#endif // 0


// ------------------ CompoundType -----------------
CompoundType::CompoundType(Keyword k, StringRef n)
  : NamedAtomicType(n),
    Scope(0 /*changeCount*/, SourceLocation() /*dummy loc*/),
    forward(true),
    keyword(k),
    bases(),
    templateInfo(NULL),
    syntax(NULL)
{
  curCompound = this;
  curAccess = (k==K_CLASS? AK_PRIVATE : AK_PUBLIC);
}

CompoundType::~CompoundType()
{
  bases.deleteAll();
  if (templateInfo) {
    delete templateInfo;
  }
}


#if 0
void CompoundType::makeComplete(Env *parentEnv, TypeEnv *te,
                                VariableEnv *ve)
{
  xassert(!env);     // shouldn't have already made one yet
  env = new Env(parentEnv, te, ve);
}    
#endif // 0


STATICDEF char const *CompoundType::keywordName(Keyword k)
{
  switch (k) {
    default:          xfailure("bad keyword");
    case K_STRUCT:    return "struct";
    case K_CLASS:     return "class";
    case K_UNION:     return "union";
  }
}


string CompoundType::toCString() const
{
  stringBuilder sb;
  
  if (templateInfo) {
    sb << templateInfo->toString() << " ";
  }

  sb << keywordName(keyword) << " "
     << (name? name : "/*anonymous*/");
     
  if (templateInfo && templateInfo->specialArguments) {
    sb << "<" << templateInfo->specialArgumentsRepr << ">";
  }
   
  return sb;
}


#if 0
// watch out for circular pointers (recursive types) here!
// (already bitten once ..)
string CompoundType::toStringWithFields() const
{
  stringBuilder sb;
  sb << toCString();
  if (isComplete()) {
    sb << " { ";
    FOREACH_OBJLIST(Field, fields, iter) {
      Field const *f = iter.data();
      sb << f->type->toCString(f->name) << "; ";
    }
    sb << "};";
  }
  else {
    sb << ";";
  }
  return sb;
}
#endif // 0


string CompoundType::toCilString(int depth) const
{
  return toCString();

  #if 0
  if (!isComplete()) {
    // this is a problem for my current type-printing
    // strategy, since I'm likely to print this even
    // when later I will get complete type info ..
    return "incomplete";
  }

  stringBuilder sb;
  sb << keywordName(keyword) << " " << name << " {\n";

  // iterate over fields
  // TODO2: this is not in the declared order ..
  StringSObjDict<Variable> &vars = env->getVariables();
  StringSObjDict<Variable>::Iter iter(vars);
  for (; !iter.isDone(); iter.next()) {
    Variable const *var = iter.value();
    sb << "  " << var->name << ": "
       << var->type->toString(depth-1) << ";\n";
  }

  sb << "}";
  return sb;
  #endif // 0
}


#if 0
MLValue CompoundType::toMLContentsValue(int depth, CVFlags cv) const
{
  // TStruct of string * fieldinfo list * int * attribute list
  // TUnion of string * fieldinfo list * int * attribute list

  // build up field list
  MLValue fields = mlNil();
  if (isComplete()) {
    StringSObjDict<Variable>::Iter iter(env->getVariables());
    for (; !iter.isDone(); iter.next()) {
      fields = mlCons(mlRecord4(
                        "fstruct", mlString(name),
                        "fname", mlString(iter.value()->name),
                        "typ", iter.value()->type->toMLValue(depth),
                        "fattr", mlRef(mlNil())
                      ), fields);
    }
  }

  // attributes
  MLValue att = cvToMLAttrs(cv);
  if (!isComplete()) {
    att = mlCons(mlString("INCOMPLETE"), att);
  }

  // assemble into a single tuple
  return mlTuple4(keyword==K_STRUCT? typ_TStruct : typ_TUnion,
                  mlString(name),
                  fields,
                  mlInt(id),
                  att);
}    
#endif // 0


int CompoundType::reprSize() const
{
  int total = 0;
  for (StringSObjDict<Variable>::IterC iter(getVariableIter());
       !iter.isDone(); iter.next()) {
    Variable *v = iter.value();
    // count nonstatic data members
    if (!v->type->isFunctionType() &&
        !v->hasFlag(DF_TYPEDEF) &&
        !v->hasFlag(DF_STATIC)) {
      int membSize = v->type->reprSize();
      if (keyword == K_UNION) {
        // representation size is max over field sizes
        total = max(total, membSize);
      }
      else {
        // representation size is sum over field sizes
        total += membSize;
      }
    }
  }
  return total;
}


int CompoundType::numFields() const
{                                                
  int ct = 0;
  for (StringSObjDict<Variable>::IterC iter(getVariableIter());
       !iter.isDone(); iter.next()) {
    Variable *v = iter.value();
           
    // count nonstatic data members
    if (!v->type->isFunctionType() &&
        !v->hasFlag(DF_TYPEDEF) &&
        !v->hasFlag(DF_STATIC)) {
      ct++;                        
    }
  }

  return ct;
}


void CompoundType::addField(Variable *v)
{
  addVariable(v);
}


string toString(CompoundType::Keyword k)
{
  xassert((unsigned)k < (unsigned)CompoundType::NUM_KEYWORDS);
  return string(typeIntrNames[k]);    // see cc_type.h
}


// ---------------- EnumType ------------------
EnumType::~EnumType()
{}


string EnumType::toCString() const
{
  return stringc << "enum " << (name? name : "/*anonymous*/");
}


string EnumType::toCilString(int depth) const
{
  // TODO2: get fields
  return toCString();
}


#if 0
MLValue EnumType::toMLContentsValue(int, CVFlags cv) const
{
  // TEnum of string * (string * int) list * int * attribute list

  return mlTuple4(typ_TEnum,
                  mlString(name),
                  mlNil(),        // TODO2: get enum elements
                  mlInt(id),
                  cvToMLAttrs(cv));
}
#endif // 0


int EnumType::reprSize() const
{
  // this is the usual choice
  return simpleTypeReprSize(ST_INT);
}


EnumType::Value *EnumType::addValue(StringRef name, int value, Variable *decl)
{
  xassert(!valueIndex.isMapped(name));

  Value *v = new Value(name, this, value, decl);
  values.append(v);
  valueIndex.add(name, v);
  
  return v;
}


EnumType::Value const *EnumType::getValue(StringRef name) const
{
  Value *v;
  if (valueIndex.query(name, v)) { 
    return v;
  }
  else {
    return NULL;
  }
}


// ---------------- EnumType::Value --------------
EnumType::Value::Value(StringRef n, EnumType const *t, int v, Variable *d)
  : name(n), type(t), value(v), decl(d)
{}

EnumType::Value::~Value()
{}


// ------------------ TypeVariable ----------------
TypeVariable::~TypeVariable()
{}

string TypeVariable::toCString() const
{
  // use the "typename" syntax instead of "class", to distinguish
  // this from an ordinary class, and because it's syntax which
  // more properly suggests the ability to take on *any* type,
  // not just those of classes
  //
  // but, the 'typename' syntax can only be used in some specialized
  // circumstances.. so I'll suppress it in the general case and add
  // it explicitly when printing the few constructs that allow it
  //return stringc << "/*typename*/ " << name;
  return string(name);
}

string TypeVariable::toCilString(int /*depth*/) const
{
  return toCString();
}

int TypeVariable::reprSize() const
{
  //xfailure("you can't ask a type variable for its size");

  // this happens when we're typechecking a template class, without
  // instantiating it, and we want to verify that some size expression
  // is constant.. so make up a number
  return 4;
}


// -------------------- Type ----------------------
ALLOC_STATS_DEFINE(Type)

Type::Type()
  //: id(NULL_TYPEID)
  : refType(NULL)
{
  ALLOC_STATS_IN_CTOR
}

// FIX dsw: garbage?: should we destruct the refType?
Type::~Type()
{
  ALLOC_STATS_IN_DTOR
}

Type *Type::getRefTypeTo() {
  if (!refType) refType = const_cast<Type*>(makePtrOperType(PO_REFERENCE, CV_NONE, this));
  return refType;
}

CAST_MEMBER_IMPL(Type, CVAtomicType)
CAST_MEMBER_IMPL(Type, PointerType)
CAST_MEMBER_IMPL(Type, FunctionType)
CAST_MEMBER_IMPL(Type, ArrayType)


bool Type::equals(Type const *obj) const
{
  if (getTag() != obj->getTag()) {
    return false;
  }

  switch (getTag()) {
    default: xfailure("bad tag");
    #define C(tag,type) \
      case tag: return ((type const*)this)->innerEquals((type const*)obj);
    C(T_ATOMIC, CVAtomicType)
    C(T_POINTER, PointerType)
    C(T_FUNCTION, FunctionType)
    C(T_ARRAY, ArrayType)
    #undef C
  }
}


string Type::idComment() const
{
  return makeIdComment(getId());
}


string cvToString(CVFlags cv)
{
  if (cv != CV_NONE) {
    return stringc << " " << toString(cv);
  }
  else {
    return string("");
  }
}


string Type::toCString() const
{
  if (isCVAtomicType()) {
    // special case a single atomic type, so as to avoid
    // printing an extra space
    CVAtomicType const &atomic = asCVAtomicTypeC();
    return stringc
      << atomic.atomic->toCString()
      << ::toString(atomic.q)
      << cvToString(atomic.cv);
  }
  else {
    return stringc << idComment() << leftString() << rightString();
  }
}

string Type::toCString(char const *name) const
{
  // print the inner parentheses if the name is omitted
  bool innerParen = (name && name[0])? false : true;

  #if 0    // wrong
  // except, if this type is a pointer, then omit the parens anyway;
  // we only need parens when the type is a function or array and
  // the name is missing
  if (isPointerType()) {
    innerParen = false;
  }                
  #endif // 0

  stringBuilder s;
  s << idComment();
  s << leftString(innerParen);
  s << (name? name : "/*anon*/");
  s << rightString(innerParen);
  return s;
}

string Type::rightString(bool /*innerParen*/) const
{
  return "";
}


string Type::toString(int depth) const
{
  return toCString();  
  //return stringc << recurseCilString(this, depth+1)
  //               << " /""* " << toCString() << " */";
}


bool Type::isSimpleType() const
{
  if (isCVAtomicType()) {
    AtomicType const *at = asCVAtomicTypeC().atomic;
    return at->isSimpleType();
  }
  else {
    return false;
  }
}

SimpleType const &Type::asSimpleTypeC() const
{
  return asCVAtomicTypeC().atomic->asSimpleTypeC();
}

bool Type::isSimple(SimpleTypeId id) const
{
  return isSimpleType() &&
         asSimpleTypeC().type == id;
}

bool Type::isIntegerType() const
{
  return isSimpleType() &&
         simpleTypeInfo(asSimpleTypeC().type).isInteger;
}


bool Type::isCompoundTypeOf(CompoundType::Keyword keyword) const
{
  CompoundType const *ct = ifCompoundType();
  return ct && ct->keyword == keyword;
}

CompoundType const *Type::ifCompoundType() const
{
  if (isCVAtomicType()) {
    AtomicType const *at = asCVAtomicTypeC().atomic;
    if (at->isCompoundType()) {
      return &( at->asCompoundTypeC() );
    }
  }
  return NULL;
}

bool Type::isOwnerPtr() const
{
  return isPointer() && ((asPointerTypeC().cv & CV_OWNER) != 0);
}

bool Type::isPointer() const
{
  return isPointerType() && asPointerTypeC().op == PO_POINTER;
}

bool Type::isReference() const
{
  return isPointerType() && asPointerTypeC().op == PO_REFERENCE;
}

Type const *Type::asRval() const
{
  if (isReference()) {
    // note that due to the restriction about stacking reference
    // types, unrolling more than once is never necessary
    return asPointerTypeC().atType;
  }
  else {
    return this;
  }
}


bool Type::isCVAtomicType(AtomicType::Tag tag) const
{
  return isCVAtomicType() &&
         asCVAtomicTypeC().atomic->getTag() == tag;
}

bool Type::isTemplateFunction() const
{
  return isFunctionType() &&
         asFunctionTypeC().isTemplate();
}

bool Type::isTemplateClass() const
{
  return isCompoundType() &&
         ifCompoundType()->isTemplate();
}

bool Type::isCDtorFunction() const
{
  return isFunctionType() &&
         asFunctionTypeC().retType->isSimple(ST_CDTOR);
}


bool typeIsError(Type const *t)
{
  return t->isError();
}

bool Type::containsErrors() const
{
  return anyCtorSatisfies(typeIsError);
}


bool typeHasTypeVariable(Type const *t)
{
  return t->isTypeVariable() ||
         (t->ifCompoundType() &&
          t->ifCompoundType()->isTemplate());
}

bool Type::containsTypeVariables() const
{
  return anyCtorSatisfies(typeHasTypeVariable);
}


// ----------------- CVAtomicType ----------------
CVAtomicType const CVAtomicType::fixed[NUM_SIMPLE_TYPES] = {
  CVAtomicType(&SimpleType::fixed[ST_CHAR],               CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_UNSIGNED_CHAR],      CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_SIGNED_CHAR],        CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_BOOL],               CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_INT],                CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_UNSIGNED_INT],       CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_LONG_INT],           CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_UNSIGNED_LONG_INT],  CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_LONG_LONG],          CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_UNSIGNED_LONG_LONG], CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_SHORT_INT],          CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_UNSIGNED_SHORT_INT], CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_WCHAR_T],            CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_FLOAT],              CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_DOUBLE],             CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_LONG_DOUBLE],        CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_VOID],               CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_ELLIPSIS],           CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_CDTOR],              CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_ERROR],              CV_NONE),
  CVAtomicType(&SimpleType::fixed[ST_DEPENDENT],          CV_NONE),
};


CVAtomicType *CVAtomicType::deepClone() const {
  // NOTE: We don't clone the underlying AtomicType.
  CVAtomicType *tmp = new CVAtomicType(atomic, cv);
  tmp->q = q ? ::deepCloneLiterals(q) : NULL;
  return tmp;
}


bool CVAtomicType::innerEquals(CVAtomicType const *obj) const
{
  return atomic->equals(obj->atomic) &&
         cv == obj->cv;
}


string CVAtomicType::atomicIdComment() const
{
  return makeIdComment(atomic->getId());
}


string CVAtomicType::leftString(bool /*innerParen*/) const
{
  stringBuilder s;
  s << atomicIdComment();
  s << atomic->toCString();
  s << ::toString(q);
  s << cvToString(cv);
  
  // this is the only mandatory space in the entire syntax
  // for declarations; it separates the type specifier from
  // the declarator(s)
  s << " ";

  return s;
}


string CVAtomicType::toCilString(int depth) const
{
  return stringc << ::toString(q)
                 << cvToString(cv)
                 << " atomic "
                 << recurseCilString(atomic, depth);
}


#if 0
MLValue CVAtomicType::toMLValue(int depth) const
{
  return atomic->toMLValue(depth, cv);
}    
#endif // 0


int CVAtomicType::reprSize() const
{
  return atomic->reprSize();
}


bool CVAtomicType::anyCtorSatisfies(TypePred pred) const
{
  return pred(this);
}


// ------------------- PointerType ---------------
PointerType::PointerType(PtrOper o, CVFlags c, Type const *a)
  : op(o), q(NULL), cv(c), atType(a) 
{
  // since references are always immutable, it makes no sense to
  // apply const or volatile to them
  xassert(op==PO_REFERENCE? cv==CV_NONE : true);

  // it also makes no sense to stack reference operators underneath
  // other indirections (i.e. no ptr-to-ref, nor ref-to-ref)
  xassert(!a->isReference());
}


PointerType *PointerType::deepClone() const {
  PointerType *tmp = new PointerType(op, cv, atType->deepClone());
  tmp->q = q ? ::deepCloneLiterals(q) : NULL;
  return tmp;
}


bool PointerType::innerEquals(PointerType const *obj) const
{
  return op == obj->op &&
         cv == obj->cv &&
         atType->equals(obj->atType);
}


string PointerType::leftString(bool /*innerParen*/) const
{
  stringBuilder s;
  s << atType->leftString(false /*innerParen*/);
  if (atType->isFunctionType() ||
      atType->isArrayType()) {
    s << "(";
  }
  s << (op==PO_POINTER? "*" : "&");
  s << ::toString(q);
  s << cvToString(cv);
  return s;
}

string PointerType::rightString(bool /*innerParen*/) const
{
  stringBuilder s;
  if (atType->isFunctionType() ||
      atType->isArrayType()) {
    s << ")";
  }
  s << atType->rightString(false /*innerParen*/);
  return s;
}


string PointerType::toCilString(int depth) const
{
  return stringc << ::toString(q)
                 << cvToString(cv)
                 << (op==PO_POINTER? "ptrto " : "refto ")
                 << recurseCilString(atType, depth);
}


#if 0
MLValue PointerType::toMLValue(int depth) const
{
  // TPtr of typ * attribute list

  return mlTuple2(typ_TPtr,
                  atType->toMLValue(depth),
                  cvToMLAttrs(cv));
}
#endif // 0


int PointerType::reprSize() const
{
  // a typical value .. (architecture-dependent)
  return 4;
}


bool PointerType::anyCtorSatisfies(TypePred pred) const
{
  return pred(this) ||
         atType->anyCtorSatisfies(pred);
}


// -------------------- FunctionType::ExnSpec --------------
FunctionType *FunctionType::deepClone() const {
  FunctionType *tmp = new FunctionType(retType->deepClone(), cv);
  for (ObjListIter<Parameter> param_iter(params); !param_iter.isDone(); param_iter.adv()) {
    tmp->addParam(param_iter.data()->deepClone());
//      xassert(param_iter.data()->type ==
//              param_iter.data()->decl->type // the variable
//              );
  }
  tmp->acceptsVarargs = acceptsVarargs;
  // FIX: omit these for now.
//    tmp->exnSpec = exnSpec->deepClone();
//    tmp->templateParams = templateParams->deepClone();

  tmp->q = q ? ::deepCloneLiterals(q) : NULL;
  return tmp;
}


FunctionType::ExnSpec::~ExnSpec()
{
  types.removeAll();
}


bool FunctionType::ExnSpec::anyCtorSatisfies(Type::TypePred pred) const
{
  SFOREACH_OBJLIST(Type const, types, iter) {
    if (iter.data()->anyCtorSatisfies(pred)) {
      return true;
    }
  }
  return false;
}


// -------------------- FunctionType -----------------
FunctionType::FunctionType(Type const *r, CVFlags c)
  : retType(r),
    q(NULL),
    cv(c),
    params(),
    acceptsVarargs(false),
    exnSpec(NULL),
    templateParams(NULL)
{}


FunctionType::~FunctionType()
{        
  if (exnSpec) {
    delete exnSpec;
  }
  if (templateParams) {
    delete templateParams;
  }
}


bool FunctionType::innerEquals(FunctionType const *obj) const
{
  if (retType->equals(obj->retType) &&
      //cv == obj->cv &&     // this is part of the parameter list
      acceptsVarargs == obj->acceptsVarargs) {
    // so far so good, try the parameters
    return equalParameterLists(obj) &&
           equalExceptionSpecs(obj);
  }
  else {
    return false;
  }
}

bool FunctionType::equalParameterLists(FunctionType const *obj) const
{
  // dsw: FIX: The comment below makes no sense to me.  If it *is*
  // legal to overload with different cv qualifiers, then why does it
  // cause us to return false?  Anyway, omitting checking for
  // QualifierLiterals here.
  if (cv != obj->cv) {
    // the 'cv' qualifier is really attached to the 'this' parameter;
    // it's important to check this here, since disequality of
    // parameter lists is part of the criteria for allowing
    // overloading, and it *is* legal to overload with different 'cv'
    // flags on a class method
    return false;
  }

  ObjListIter<Parameter> iter1(params);
  ObjListIter<Parameter> iter2(obj->params);
  for (; !iter1.isDone() && !iter2.isDone();
       iter1.adv(), iter2.adv()) {
    // parameter names do not have to match, but
    // the types do
    if (iter1.data()->type->equals(iter2.data()->type)) {
      // ok
    }
    else {
      return false;
    }
  }

  return iter1.isDone() == iter2.isDone();
}


// almost identical code to the above.. list comparison is
// always a pain..
bool FunctionType::equalExceptionSpecs(FunctionType const *obj) const
{
  if (exnSpec==NULL && obj->exnSpec==NULL)  return true;
  if (exnSpec==NULL || obj->exnSpec==NULL)  return false;

  // hmm.. this is going to end up requiring that exception specs
  // be listed in the same order, whereas I think the semantics
  // imply they're more like a set.. oh well
  SObjListIter<Type const> iter1(exnSpec->types);
  SObjListIter<Type const> iter2(obj->exnSpec->types);
  for (; !iter1.isDone() && !iter2.isDone();
       iter1.adv(), iter2.adv()) {
    if (iter1.data()->equals(iter2.data())) {
      // ok
    }
    else {
      return false;
    }
  }

  return iter1.isDone() == iter2.isDone();
}


void FunctionType::addParam(Parameter *param)
{
  params.append(param);
}


string FunctionType::leftString(bool innerParen) const
{
  stringBuilder sb;

  // template parameters
  if (templateParams) {
    sb << templateParams->toString() << " ";
  }

  // return type and start of enclosing type's description

  // NOTE: we do *not* propagate 'innerParen'!  
  sb << retType->leftString();
  if (innerParen) {
    sb << "(";
  }

  return sb;
}

string FunctionType::rightString(bool innerParen) const
{
  // finish enclosing type
  stringBuilder sb;
  if (innerParen) {
    sb << ")";
  }

  // arguments
  sb << "(";
  int ct=0;
  FOREACH_OBJLIST(Parameter, params, iter) {
    if (ct++ > 0) {
      sb << ", ";
    }
    sb << iter.data()->toString();
  }

  if (acceptsVarargs) {
    if (ct++ > 0) {
      sb << ", ";
    }
    sb << "...";
  }

  sb << ")";

  // qualifiers
  sb << ::toString(q);
  if (cv) {
    sb << " " << ::toString(cv);
  }

  // exception specs
  if (exnSpec) {
    sb << " throw(";                
    int ct=0;
    SFOREACH_OBJLIST(Type const, exnSpec->types, iter) {
      if (ct++ > 0) {
        sb << ", ";
      }
      sb << iter.data()->toString();
    }
    sb << ")";
  }

  // finish up the return type
  sb << retType->rightString();

  return sb;
}


string FunctionType::toCilString(int depth) const
{
  stringBuilder sb;
  sb << "func " /*<< cvToString(cv) << " "*/;
  if (acceptsVarargs) {
    sb << "varargs ";
  }
  sb << "(";

  int ct=0;
  FOREACH_OBJLIST(Parameter, params, iter) {
    if (++ct > 1) {
      sb << ", ";
    }
    sb << iter.data()->name << ": "
       << recurseCilString(iter.data()->type, depth);
  }

  sb << ") -> " << recurseCilString(retType, depth);

  return sb;
}


#if 0
MLValue FunctionType::toMLValue(int depth) const
{
  // TFunc of typ * typ list * bool * attribute list

  // build up argument type list
  MLValue args = mlNil();
  FOREACH_OBJLIST(Parameter, params, iter) {
    args = mlCons(iter.data()->type->toMLValue(depth),
                  args);
  }

  return mlTuple4(typ_TFunc,
                  retType->toMLValue(depth),
                  args,
                  mlBool(acceptsVarargs),
                  cvToMLAttrs(cv));
}
#endif // 0


int FunctionType::reprSize() const
{
  // thinking here about how this works when we're summing
  // the fields of a class with member functions ..
  return 0;
}


bool parameterListCtorSatisfies(Type::TypePred pred, 
                                ObjList<Parameter> const &params)
{
  FOREACH_OBJLIST(Parameter, params, iter) {
    if (iter.data()->type->anyCtorSatisfies(pred)) {
      return true;
    }
  }
  return false;
}

bool FunctionType::anyCtorSatisfies(TypePred pred) const
{
  return pred(this) ||
         retType->anyCtorSatisfies(pred) ||
         parameterListCtorSatisfies(pred, params) ||
         (exnSpec && exnSpec->anyCtorSatisfies(pred)) ||
         (templateParams && templateParams->anyCtorSatisfies(pred));
}



// -------------------- Parameter -----------------
Parameter::~Parameter()
{
  if (defaultArgument) {
    delete defaultArgument;
  }
}

Parameter *Parameter::deepClone() const {
  Parameter *tmp =
    new Parameter(
                  // dsw: FIX: don't see a reason to clone the name
//                    (name ? strdup(name) : (StringRef)NULL)
                  name
                  ,NULL // NOT: type->deepClone()
                  // clone the variable and then copy the type below.
                  ,decl->deepClone()
                  );
  // maintain the invariant that param->type == param->decl->type
  tmp->type = tmp->decl->type;
  // dsw: Cloning this involves cloning the whole expression language.
  // FIX: does this matter?
//    tmp->defaultArgument = defaultArgument->deepClone();
  tmp->defaultArgument = NULL;
  return tmp;
}

string Parameter::toString() const
{
  stringBuilder sb;
  if (type->isTypeVariable()) {
    sb << "class " << name;
  }
  else {
    sb << type->toCString(name);
  }

  if (defaultArgument) {
    sb << " = " << defaultArgument->exprText;
  }
  return sb;
}


// ---------------- DefaultArgument ------------
DefaultArgument::~DefaultArgument()
{
  // deletes 'exprText'
}


// ----------------- TemplateParams --------------
TemplateParams::~TemplateParams()
{
  params.deleteAll();
}


string TemplateParams::toString() const
{
  stringBuilder sb;
  sb << "template <";
  int ct=0;
  FOREACH_OBJLIST(Parameter, params, iter) {
    if (ct++ > 0) {
      sb << ", ";
    }
    sb << iter.data()->toString();
  }
  sb << ">";
  return sb;
}


bool TemplateParams::equalTypes(TemplateParams const *obj) const
{
  ObjListIter<Parameter> iter1(params), iter2(obj->params);
  for (; !iter1.isDone() && !iter2.isDone();
       iter1.adv(), iter2.adv()) {
    if (iter1.data()->type->equals(iter2.data()->type)) {
      // ok
    }
    else {
      return false;
    }
  }

  return iter1.isDone() == iter2.isDone();
}


bool TemplateParams::anyCtorSatisfies(Type::TypePred pred) const
{
  return parameterListCtorSatisfies(pred, params);
}


// ------------------ ClassTemplateInfo -------------
ClassTemplateInfo::ClassTemplateInfo()
  : instantiated(),           // empty map
    specializations(),        // empty list
    specialArguments(NULL)    // empty list
{}

ClassTemplateInfo::~ClassTemplateInfo()
{}


// -------------------- ArrayType ------------------
ArrayType *ArrayType::deepClone() const {
  ArrayType *tmp = new ArrayType(eltType->deepClone(), size);
  tmp->hasSize = hasSize;
  tmp->q = q ? ::deepCloneLiterals(q) : NULL;
  return tmp;
}


void ArrayType::checkWellFormedness() const
{
  // you can't have an array of references
  xassert(!eltType->isReference());
}


bool ArrayType::innerEquals(ArrayType const *obj) const
{
  if (!( eltType->equals(obj->eltType) &&
         hasSize == obj->hasSize )) {
    return false;
  }

  if (hasSize) {
    return size == obj->size;
  }
  else {
    return true;
  }
}


string ArrayType::leftString(bool /*innerParen*/) const
{
  return eltType->leftString();
}

string ArrayType::rightString(bool /*innerParen*/) const
{
  stringBuilder sb;

  if (hasSize) {
    sb << "[" << size << "]";
  }
  else {
    sb << "[]";
  }

  sb << eltType->rightString();

  return sb;
}


string ArrayType::toCilString(int depth) const
{
  stringBuilder sb;
  sb << "array [";
  if (hasSize) {
    sb << size;
  }
  sb << "] of " << recurseCilString(eltType, depth);
  return sb;
}


#if 0
MLValue ArrayType::toMLValue(int depth) const
{
  // TArray of typ * exp option * attribute list

  // size
  MLValue mlSize;
  if (hasSize) {
    // since the array type is currently an arbitrary
    // expression, but I just store an int (because I
    // evaluate sizeof at parse time), construct an
    // expression now
    Owner<CilExpr> e; e = newIntLit(NULL /*extra*/, size);
    mlSize = mlSome(e->toMLValue());
  }
  else {
    mlSize = mlNone();
  }

  return mlTuple3(typ_TArray,
                  eltType->toMLValue(depth),
                  mlSize,
                  mlNil());    // no attrs for arrays
}
#endif // 0


int ArrayType::reprSize() const
{
  if (hasSize) {
    return eltType->reprSize() * size;
  }
  else {
    // or should I throw an exception ..?
    cout << "warning: reprSize of a sizeless array\n";
    return 0;
  }
}


bool ArrayType::anyCtorSatisfies(TypePred pred) const
{
  return pred(this) ||
         eltType->anyCtorSatisfies(pred);
}


// -------------------- type construction ----------------------
CVAtomicType *makeType(AtomicType const *atomic)
{
  return makeCVType(atomic, CV_NONE);
}


CVAtomicType *makeCVType(AtomicType const *atomic, CVFlags cv)
{
  return new CVAtomicType(atomic, cv);
}


Type const *applyCVToType(CVFlags cv, Type const *baseType)
{
  if (baseType->isError()) {
    return baseType;
  }

  if (cv == CV_NONE) {
    // keep what we've got
    return baseType;
  }

  // the idea is we're trying to apply 'cv' to 'baseType'; for
  // example, we could have gotten baseType like
  //   typedef unsigned char byte;     // baseType == unsigned char
  // and want to apply const:
  //   byte const b;                   // cv = CV_CONST
  // yielding final type
  //   unsigned char const             // return value from this fn

  // first, check for special cases
  switch (baseType->getTag()) {
    case Type::T_ATOMIC: {
      CVAtomicType const &atomic = baseType->asCVAtomicTypeC();
      if ((atomic.cv | cv) == atomic.cv) {
        // the given type already contains 'cv' as a subset,
        // so no modification is necessary
        return baseType;
      }
      else {
        // we have to add another CV, so that means creating
        // a new CVAtomicType with the same AtomicType as 'baseType'
        CVAtomicType *ret = new CVAtomicType(atomic);

        // but with the new flags added
        ret->cv = (CVFlags)(ret->cv | cv);

        return ret;
      }
      break;
    }

    case Type::T_POINTER: {
      // logic here is nearly identical to the T_ATOMIC case
      PointerType const &ptr = baseType->asPointerTypeC();
      if (ptr.op == PO_REFERENCE) {
        return NULL;     // can't apply CV to references
      }
      if ((ptr.cv | cv) == ptr.cv) {
        return baseType;
      }
      else {
        PointerType *ret = new PointerType(ptr);
        ret->cv = (CVFlags)(ret->cv | cv);
        return ret;
      }
      break;
    }

    default:    // silence warning
    case Type::T_FUNCTION:
    case Type::T_ARRAY:
      // can't apply CV to either of these (function has CV, but
      // can't get it after the fact)
      return NULL;
  }
}


ArrayType const *setArraySize(ArrayType const *type, int size)
{
  ArrayType *ret = new ArrayType(type->eltType, size);
  return ret;
}


Type const *makePtrOperType(PtrOper op, CVFlags cv, Type const *type)
{
  if (type->isError()) {
    return type;
  }

  xassert(!type->isReference()); // don't make a ref to a ref

  return new PointerType(op, cv, type);
}


#ifdef DISTINCT_CVATOMIC_TYPES
CVAtomicType const *getSimpleType(SimpleTypeId st, CVFlags cv = CV_NONE)
{
  xassert((unsigned)st < (unsigned)NUM_SIMPLE_TYPES);
  return new CVAtomicType(&SimpleType::fixed[st], cv);
}

#else
CVAtomicType const *getSimpleType(SimpleTypeId st)
{
  xassert((unsigned)st < (unsigned)NUM_SIMPLE_TYPES);
  return &(CVAtomicType::fixed[st]);
}
#endif


// ------------------ test -----------------
void cc_type_checker()
{
  #ifndef NDEBUG
  // verify the fixed[] arrays
  // it turns out this is probably redundant, since the compiler will
  // complain if I leave out an entry, now that the classes do not have
  // default constructors! yes!
  for (int i=0; i<NUM_SIMPLE_TYPES; i++) {
    assert(SimpleType::fixed[i].type == i);

    SimpleType const *st = (SimpleType const*)(CVAtomicType::fixed[i].atomic);
    assert(st && st->type == i);
  }
  #endif // NDEBUG
}


// --------------- debugging ---------------
static char *staticBuffer(char const *s)
{
  static char buf[200];

  int len = strlen(s);
  if (len > 79) len=79;
  memcpy(buf, s, len);
  buf[len] = 0;
  
  return buf;
}


char *type_toString(Type const *t)
{
  return staticBuffer(t->toString());
}


char *type_toCilString(Type const *t)
{
  return staticBuffer(t->toCilString(20 /*depth*/));
}
