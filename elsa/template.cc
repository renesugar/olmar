// template.cc
// template stuff implementation; code for template.h, and for
// the template section of cc_env.h at the end of Env

#include "template.h"      // this module
#include "cc_env.h"        // also kind of this module
#include "trace.h"         // tracingSys
#include "strtable.h"      // StringTable
#include "cc_lang.h"       // CCLang
#include "strutil.h"       // pluraln
#include "overload.h"      // selectBestCandidate_templCompoundType
#include "matchtype.h"     // MatchType, STemplateArgumentCMap
#include "typelistiter.h"  // TypeListIter
#include "cc_ast_aux.h"    // ASTTemplVisitor


// I'm doing this fairly frequently, and it is pretty safe (really should
// be returning a const ref...), so I'll consolidate a bit
template <class T>
inline SObjList<T>& objToSObjList(ObjList<T> &list)
{
  return reinterpret_cast<SObjList<T>&>(list);
}

// this one should be completely safe
template <class T>
inline SObjList<T> const & objToSObjListC(ObjList<T> const &list)
{
  return reinterpret_cast<SObjList<T> const &>(list);
}


void copyTemplateArgs(ObjList<STemplateArgument> &dest,
                      SObjList<STemplateArgument> const &src)
{
  xassert(dest.isEmpty());    // otherwise prepend/reverse won't work
  SFOREACH_OBJLIST(STemplateArgument, src, iter) {
    dest.prepend(new STemplateArgument(*(iter.data())));
  }
  dest.reverse();
}


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
  //
  // 8/09/04: sm: truncated down to just the name, since the extra
  // clutter was annoying and not that helpful
  return stringc //<< "/""*typevar"
//                   << "typedefVar->serialNumber:"
//                   << (typedefVar ? typedefVar->serialNumber : -1)
                 //<< "*/"
                 << name;
}

int TypeVariable::reprSize() const
{
  //xfailure("you can't ask a type variable for its size");

  // this happens when we're typechecking a template class, without
  // instantiating it, and we want to verify that some size expression
  // is constant.. so make up a number
  return 4;
}


// -------------------- PseudoInstantiation ------------------
PseudoInstantiation::PseudoInstantiation(CompoundType *p)
  : NamedAtomicType(p->name),
    primary(p),
    args()        // empty initially
{}

PseudoInstantiation::~PseudoInstantiation()
{}

string PseudoInstantiation::toCString() const
{
  return stringc << primary->name << sargsToString(args);
}

int PseudoInstantiation::reprSize() const
{
  // it shouldn't matter what we say here, since the query will only
  // be made in the context of checking (but not instantiating) a
  // template definition body
  return 4;
}


// ------------------ TemplateParams ---------------
TemplateParams::TemplateParams(TemplateParams const &obj)
  : params(obj.params)
{}

TemplateParams::~TemplateParams()
{}


string TemplateParams::paramsToCString() const
{
  return ::paramsToCString(params);
}

string paramsToCString(SObjList<Variable> const &params)
{
  stringBuilder sb;
  sb << "template <";
  int ct=0;
  SFOREACH_OBJLIST(Variable, params, iter) {
    Variable const *p = iter.data();
    if (ct++ > 0) {
      sb << ", ";
    }

    if (p->type->isTypeVariable()) {
      sb << "class " << p->name;
      if (p->type->asTypeVariable()->name != p->name) {
        // this should never happen, but if it does then I just want
        // it to be visible, not (directly) cause a crash
        sb << " /""* but type name is " << p->type->asTypeVariable()->name << "! */";
      }
    }
    else {
      // non-type parameter
      sb << p->toCStringAsParameter();
    }
  }
  sb << ">";
  return sb;
}

    
string TemplateParams::paramsLikeArgsToString() const
{
  stringBuilder sb;
  sb << "<";
  int ct=0;
  SFOREACH_OBJLIST(Variable, params, iter) {
    if (ct++) { sb << ", "; }
    sb << iter.data()->name;
  }
  sb << ">";
  return sb;
}


// defined in cc_type.cc
bool parameterListCtorSatisfies(TypePred &pred,
                                SObjList<Variable> const &params);

bool TemplateParams::anyParamCtorSatisfies(TypePred &pred) const
{
  return parameterListCtorSatisfies(pred, params);
}


// --------------- InheritedTemplateParams ---------------
InheritedTemplateParams::InheritedTemplateParams(InheritedTemplateParams const &obj)
  : TemplateParams(obj),
    enclosing(obj.enclosing)
{}

InheritedTemplateParams::~InheritedTemplateParams()
{}


// ------------------ TemplateInfo -------------
TemplateInfo::TemplateInfo(SourceLoc il, Variable *v)
  : TemplateParams(),
    var(NULL),
    instantiationOf(NULL),
    instantiations(),
    specializationOf(NULL),
    specializations(),
    arguments(),
    instLoc(il),
    partialInstantiationOf(NULL),
    partialInstantiations(),
    argumentsToPrimary(),
    defnScope(NULL),
    definitionTemplateInfo(NULL),
    instantiateBody(false)
{
  if (v) {
    // this sets 'this->var' too
    v->setTemplateInfo(this);
  }
}


// this is called by Env::makeUsingAliasFor ..
TemplateInfo::TemplateInfo(TemplateInfo const &obj)
  : TemplateParams(obj),
    var(NULL),                // caller must call Variable::setTemplateInfo
    instantiationOf(NULL),
    instantiations(obj.instantiations),      // suspicious... oh well
    specializationOf(NULL),
    specializations(obj.specializations),    // also suspicious
    arguments(),                             // copied below
    instLoc(obj.instLoc),
    partialInstantiationOf(NULL),
    partialInstantiations(),
    argumentsToPrimary(),                    // copied below
    defnScope(NULL),
    definitionTemplateInfo(NULL),
    instantiateBody(false)
{
  // inheritedParams
  FOREACH_OBJLIST(InheritedTemplateParams, obj.inheritedParams, iter2) {
    inheritedParams.prepend(new InheritedTemplateParams(*(iter2.data())));
  }
  inheritedParams.reverse();

  // arguments
  copyArguments(obj.arguments);

  // argumentsToPrimary
  copyTemplateArgs(argumentsToPrimary, objToSObjListC(obj.argumentsToPrimary));
}


TemplateInfo::~TemplateInfo()
{
  if (definitionTemplateInfo) {
    delete definitionTemplateInfo;
  }
}


TemplateThingKind TemplateInfo::getKind() const
{
  if (!instantiationOf && !specializationOf) {
    if (!isPartialInstantiation()) {
      xassert(arguments.isEmpty());
    }
    xassert(hasMainOrInheritedParameters());
    return TTK_PRIMARY;
  }

  // specialization or instantiation
  xassert(arguments.isNotEmpty());

  if (specializationOf) {
    return TTK_SPECIALIZATION;
  }
  else {     
    xassert(instantiationOf);
    return TTK_INSTANTIATION;
  }
}


bool TemplateInfo::isPartialSpec() const
{
  return isSpecialization() &&
         hasParameters();
}

bool TemplateInfo::isCompleteSpec() const
{
  return isSpecialization() &&
         !hasMainOrInheritedParameters();
}


bool TemplateInfo::isCompleteSpecOrInstantiation() const
{
  return isNotPrimary() &&
         !hasMainOrInheritedParameters();
}


bool TemplateInfo::isInstOfPartialSpec() const
{
  return isInstantiation() &&
         instantiationOf->templateInfo()->isPartialSpec();
}


// this is idempotent
TemplateInfo const *TemplateInfo::getPrimaryC() const
{
  if (instantiationOf) {
    return instantiationOf->templateInfo()->getPrimaryC();
  }
  else if (specializationOf) {
    return specializationOf->templateInfo();     // always only one level
  }
  else {
    return this;
  }
}


void TemplateInfo::addToList(Variable *elt, SObjList<Variable> &children,
                             Variable * const &parentPtr)
{
  // the key to this routine is casting away the constness of
  // 'parentPtr'; it's the one routine entitled to do so
  Variable * &parent = const_cast<Variable*&>(parentPtr);

  // add to list, ensuring (if in debug mode) it isn't already there
  xassertdb(!children.contains(elt));
  children.append(elt);             // could use 'prepend' for performance..

  // backpointer, ensuring we don't overwrite one
  xassert(!parent);
  xassert(this->var);
  parent = this->var;
}

void TemplateInfo::addInstantiation(Variable *inst)
{
  addToList(inst, instantiations,
            inst->templateInfo()->instantiationOf);
}

void TemplateInfo::addSpecialization(Variable *inst)
{
  addToList(inst, specializations,
            inst->templateInfo()->specializationOf);
}

void TemplateInfo::addPartialInstantiation(Variable *pinst)
{
  addToList(pinst, partialInstantiations,
            pinst->templateInfo()->partialInstantiationOf);
}


ObjList<STemplateArgument> &TemplateInfo::getArgumentsToPrimary()
{
  if (isInstOfPartialSpec()) {
    return argumentsToPrimary;
  }
  else {
    return arguments;
  }
}


bool equalArgumentLists(TypeFactory &tfac,
                        SObjList<STemplateArgument> const &list1,
                        SObjList<STemplateArgument> const &list2)
{
  SObjListIter<STemplateArgument> iter1(list1);
  SObjListIter<STemplateArgument> iter2(list2);

  while (!iter1.isDone() && !iter2.isDone()) {
    STemplateArgument const *sta1 = iter1.data();
    STemplateArgument const *sta2 = iter2.data();
    if (!sta1->isomorphic(tfac, sta2)) {
      return false;
    }

    iter1.adv();
    iter2.adv();
  }

  return iter1.isDone() && iter2.isDone();
}

bool TemplateInfo::equalArguments
  (TypeFactory &tfac, SObjList<STemplateArgument> const &list) const
{
  return equalArgumentLists(tfac, objToSObjListC(arguments), list);
}


bool TemplateInfo::argumentsContainTypeVariables() const
{
  FOREACH_OBJLIST(STemplateArgument, arguments, iter) {
    STemplateArgument const *sta = iter.data();
    if (sta->kind == STemplateArgument::STA_TYPE) {
      if (sta->value.t->containsTypeVariables()) return true;
    }
    // FIX: do other kinds
  }
  return false;
}


bool TemplateInfo::argumentsContainVariables() const
{
  FOREACH_OBJLIST(STemplateArgument, arguments, iter) {
    if (iter.data()->containsVariables()) return true;
  }
  return false;
}


bool TemplateInfo::hasParameters() const
{
  if (isPartialInstantiation()) {
    return true;
  }

  // check params attached directly to this object
  if (params.isNotEmpty()) {
    return true;
  }

  return false;
}

bool TemplateInfo::hasMainOrInheritedParameters() const
{
  // main
  if (hasParameters()) {
    return true;
  }

  // check for inherited parameters
  FOREACH_OBJLIST(InheritedTemplateParams, inheritedParams, iter) {
    if (iter.data()->params.isNotEmpty()) {
      return true;
    }
  }
               
  // no parameters at any level
  return false;                
}

bool TemplateInfo::hasParametersEx(bool considerInherited) const
{
  return considerInherited?
           hasMainOrInheritedParameters() :
           hasParameters();
}


Variable *TemplateInfo::getSpecialization(
  TypeFactory &tfac,
  SObjList<STemplateArgument> const &sargs)
{
  SFOREACH_OBJLIST_NC(Variable, specializations, iter) {
    if (iter.data()->templateInfo()->equalArguments(tfac, sargs)) {
      return iter.data();
    }
  }
  return NULL;     // not found
}


bool TemplateInfo::hasSpecificParameter(Variable const *v) const
{
  // 'params'?
  if (params.contains(v)) { return true; }
  
  // inherited?
  FOREACH_OBJLIST(InheritedTemplateParams, inheritedParams, iter) {
    if (iter.data()->params.contains(v)) { 
      return true; 
    }
  }

  return false;     // 'v' does not appear in any parameter list
}


void TemplateInfo::copyArguments(ObjList<STemplateArgument> const &sargs)
{
  copyTemplateArgs(arguments, objToSObjListC(sargs));
}

void TemplateInfo::copyArguments(SObjList<STemplateArgument> const &sargs)
{
  copyTemplateArgs(arguments, sargs);
}


string TemplateInfo::templateName() const
{
  if (isPrimary()) {
    return stringc << var->fullyQualifiedName()
                   << paramsLikeArgsToString();
  }

  if (isSpecialization()) {
    return stringc << var->fullyQualifiedName()
                   << sargsToString(arguments);
  }

  // instantiation; but of the primary or of a specialization?
  TemplateInfo *parentTI = instantiationOf->templateInfo();
  if (parentTI->isPrimary()) {
    return stringc << var->fullyQualifiedName()
                   << sargsToString(arguments);
  }
  else {
    // spec params + inst args, e.g. "A<T*><int>" to mean that
    // this is an instantiation of spec "A<T*>" with args "<int>",
    // i.e. original arguments "<int*>"
    return stringc << var->fullyQualifiedName()
                   << sargsToString(parentTI->arguments)
                   << sargsToString(arguments);
  }
}


void TemplateInfo::gdb()
{
  debugPrint(0);
}


void TemplateInfo::debugPrint(int depth, bool printPartialInsts)
{
  ind(cout, depth*2) << "TemplateInfo for "
                     << (var? var->name : "(null var)") << " {" << endl;

  depth++;

  if (isPartialInstantiation()) {
    // the template we are a partial instantiation of has all the
    // parameter info, so print it; but then *it* better not turn
    // around and print its partial instantiation list, otherwise we
    // get an infinite loop!  (discovered the hard way...)
    ind(cout, depth*2) << "partialInstantiatedFrom:\n";
    partialInstantiationOf->templateInfo()->
      debugPrint(depth+1, false /*printPartialInsts*/);
  }

  // inherited params
  FOREACH_OBJLIST(InheritedTemplateParams, inheritedParams, iter) {
    ind(cout, depth*2) << "inherited from " << iter.data()->enclosing->name
                       << ": " << iter.data()->paramsToCString() << endl;
  }

  // my params
  ind(cout, depth*2) << "params: " << paramsToCString() << endl;

  ind(cout, depth*2) << "arguments:" << endl;
  FOREACH_OBJLIST_NC(STemplateArgument, arguments, iter) {
    iter.data()->debugPrint(depth+1);
  }

  ind(cout, depth*2) << "instantiations:" << endl;
  depth++;
  SFOREACH_OBJLIST_NC(Variable, instantiations, iter) {
    Variable *var = iter.data();
    ind(cout, depth*2) << var->type->toString() << endl;
    var->templateInfo()->debugPrint(depth+1);
  }
  depth--;

  if (printPartialInsts) {
    ind(cout, depth*2) << "partial instantiations:" << endl;
    depth++;
    SFOREACH_OBJLIST_NC(Variable, partialInstantiations, iter) {
      Variable *var = iter.data();
      ind(cout, depth*2) << var->toString() << endl;
      var->templateInfo()->debugPrint(depth+1);
    }
    depth--;
  }

  depth--;

  ind(cout, depth*2) << "}" << endl;
}


// ------------------- STemplateArgument ---------------
STemplateArgument::STemplateArgument(STemplateArgument const &obj)
  : kind(obj.kind)
{
  if (kind == STA_TYPE) {
    value.t = obj.value.t;
  }
  else if (kind == STA_INT) {
    value.i = obj.value.i;
  }
  else {
    value.v = obj.value.v;
  }
}


STemplateArgument *STemplateArgument::shallowClone() const
{
  return new STemplateArgument(*this);
}


bool STemplateArgument::isObject() const
{
  switch (kind) {
  default:
    xfailure("illegal STemplateArgument kind"); break;

  case STA_TYPE:                // type argument
    return false; break;

  case STA_INT:                 // int or enum argument
  case STA_REFERENCE:           // reference to global object
  case STA_POINTER:             // pointer to global object
  case STA_MEMBER:              // pointer to class member
    return true; break;

  case STA_TEMPLATE:            // template argument (not implemented)
    return false; break;
  }
}
    

bool STemplateArgument::isDependent() const
{
  if (isType()) {
    // we don't (or shouldn't...) stack constructors on top of
    // ST_DEPENDENT, so just check at the top level
    //
    // 8/10/04: I had simply been calling Type::isDependent, but that
    // answers yes for type variables.  In the context in which I'm
    // calling this, I am only interested in '<dependent>'.  I realize
    // this is a bit of a non-orthogonality, but the fix isn't clear
    // at the moment.
    return getType()->isSimple(ST_DEPENDENT);
  }
  else {
    return false;      // what about non-type args?  template args?
  }
}


bool STemplateArgument::equals(STemplateArgument const *obj) const
{
  if (kind != obj->kind) {
    return false;
  }

  // At one point I tried making type arguments unequal if they had
  // different typedefs, but that does not work, because I need A<int>
  // to be the *same* type as A<my_int>, for example to do base class
  // constructor calls.

  switch (kind) {
    case STA_TYPE:     return value.t->equals(obj->value.t);
    case STA_INT:      return value.i == obj->value.i;
    default:           return value.v == obj->value.v;
  }
}


bool STemplateArgument::containsVariables() const
{
  if (kind == STemplateArgument::STA_TYPE) {
    if (value.t->containsVariables()) {
      return true;
    }
  } else if (kind == STemplateArgument::STA_REFERENCE) {
    // FIX: I am not at all sure that my interpretation of what
    // STemplateArgument::kind == STA_REFERENCE means; I think it
    // means it is a non-type non-template (object) variable in an
    // argument list
    return true;
  }
  // FIX: do other kinds
  return false;
}


bool STemplateArgument::isomorphic(TypeFactory &tfac, STemplateArgument const *obj) const
{
  if (kind != obj->kind) {
    return false;
  }

  switch (kind) {
    case STA_TYPE:
      // 10/09/04: (in/t0351.cc) Use 'equalOrIsomorphic' so we skip the
      // buggy matchtype module whenever possible (unfortunately, this
      // means the matchtype bugs get pushed even deeper...).
      return equalOrIsomorphic(tfac, value.t, obj->value.t);

    // TODO: these are wrong, because we don't have a proper way
    // to represent non-type template parameters in argument lists
    case STA_INT:      return value.i == obj->value.i;
    default:           return value.v == obj->value.v;
  }
}


string STemplateArgument::toString() const
{
  switch (kind) {
    default: xfailure("bad kind");
    case STA_NONE:      return string("STA_NONE");
    case STA_TYPE:      return value.t->toString();   // assume 'type' if no comment
    case STA_INT:       return stringc << "/*int*/ " << value.i;
    case STA_REFERENCE: return stringc << "/*ref*/ " << value.v->name;
    case STA_POINTER:   return stringc << "/*ptr*/ &" << value.v->name;
    case STA_MEMBER:    return stringc
      << "/*member*/ &" << value.v->scope->curCompound->name 
      << "::" << value.v->name;
    case STA_TEMPLATE:  return string("template (?)");
  }
}


void STemplateArgument::gdb()
{
  debugPrint(0);
}


void STemplateArgument::debugPrint(int depth)
{
  for (int i=0; i<depth; ++i) cout << "  ";
  cout << "STemplateArgument: " << toString() << endl;
}


SObjList<STemplateArgument> *cloneSArgs(SObjList<STemplateArgument> &sargs)
{
  SObjList<STemplateArgument> *ret = new SObjList<STemplateArgument>();
  SFOREACH_OBJLIST_NC(STemplateArgument, sargs, iter) {
    ret->append(iter.data());
  }
  return ret;
}


string sargsToString(SObjList<STemplateArgument> const &list)
{
  stringBuilder sb;
  sb << "<";

  int ct=0;
  SFOREACH_OBJLIST(STemplateArgument, list, iter) {
    if (ct++ > 0) {
      sb << ", ";
    }
    sb << iter.data()->toString();
  }

  sb << ">";
  return sb;
}


bool containsTypeVariables(SObjList<STemplateArgument> const &args)
{
  SFOREACH_OBJLIST(STemplateArgument, args, iter) {
    if (iter.data()->containsVariables()) {
      return true;
    }
  }
  return false;
}


bool hasDependentArgs(SObjList<STemplateArgument> const &args)
{
  SFOREACH_OBJLIST(STemplateArgument, args, iter) {
    if (iter.data()->isDependent()) {
      return true;
    }
  }
  return false;
}


// ---------------------- TemplCandidates ------------------------
STATICDEF
TemplCandidates::STemplateArgsCmp TemplCandidates::compareSTemplateArgs
  (TypeFactory &tfac, STemplateArgument const *larg, STemplateArgument const *rarg)
{
  xassert(larg->kind == rarg->kind);

  switch(larg->kind) {
  default:
    xfailure("illegal TemplateArgument kind");
    break;

  case STemplateArgument::STA_NONE: // not yet resolved into a valid template argument
    xfailure("STA_NONE TemplateArgument kind");
    break;

  case STemplateArgument::STA_TYPE: // type argument
    {
    // check if left is at least as specific as right
    bool leftAtLeastAsSpec;
    {
      MatchTypes match(tfac, MatchTypes::MM_WILD);
      if (match.match_Type(larg->value.t, rarg->value.t)) {
        leftAtLeastAsSpec = true;
      } else {
        leftAtLeastAsSpec = false;
      }
    }
    // check if right is at least as specific as left
    bool rightAtLeastAsSpec;
    {
      MatchTypes match(tfac, MatchTypes::MM_WILD);
      if (match.match_Type(rarg->value.t, larg->value.t)) {
        rightAtLeastAsSpec = true;
      } else {
        rightAtLeastAsSpec = false;
      }
    }

    // change of basis matrix
    if (leftAtLeastAsSpec) {
      if (rightAtLeastAsSpec) {
        return STAC_EQUAL;
      } else {
        return STAC_LEFT_MORE_SPEC;
      }
    } else {
      if (rightAtLeastAsSpec) {
        return STAC_RIGHT_MORE_SPEC;
      } else {
        return STAC_INCOMPARABLE;
      }
    }

    }
    break;

  case STemplateArgument::STA_INT: // int or enum argument
    if (larg->value.i == rarg->value.i) {
      return STAC_EQUAL;
    }
    return STAC_INCOMPARABLE;
    break;

  case STemplateArgument::STA_REFERENCE: // reference to global object
  case STemplateArgument::STA_POINTER: // pointer to global object
  case STemplateArgument::STA_MEMBER: // pointer to class member
    if (larg->value.v == rarg->value.v) {
      return STAC_EQUAL;
    }
    return STAC_INCOMPARABLE;
    break;

  case STemplateArgument::STA_TEMPLATE: // template argument (not implemented)
    xfailure("STA_TEMPLATE TemplateArgument kind; not implemented");
    break;
  }
}


STATICDEF int TemplCandidates::compareCandidatesStatic
  (TypeFactory &tfac, TemplateInfo const *lti, TemplateInfo const *rti)
{
  // I do not even put the primary into the set so it should never
  // show up.
  xassert(lti->isNotPrimary());
  xassert(rti->isNotPrimary());

  // they should have the same primary
  xassert(lti->getPrimaryC() == rti->getPrimaryC());

  // they should always have the same number of arguments; the number
  // of parameters is irrelevant
  xassert(lti->arguments.count() == rti->arguments.count());

  STemplateArgsCmp leaning = STAC_EQUAL;// which direction are we leaning?
  // for each argument pairwise
  ObjListIter<STemplateArgument> lIter(lti->arguments);
  ObjListIter<STemplateArgument> rIter(rti->arguments);
  for(;
      !lIter.isDone();
      lIter.adv(), rIter.adv()) {
    STemplateArgument const *larg = lIter.data();
    STemplateArgument const *rarg = rIter.data();
    STemplateArgsCmp cmp = compareSTemplateArgs(tfac, larg, rarg);
    switch(cmp) {
    default: xfailure("illegal STemplateArgsCmp"); break;
    case STAC_LEFT_MORE_SPEC:
      if (leaning == STAC_EQUAL) {
        leaning = STAC_LEFT_MORE_SPEC;
      } else if (leaning == STAC_RIGHT_MORE_SPEC) {
        leaning = STAC_INCOMPARABLE;
      }
      // left stays left and incomparable stays incomparable
      break;
    case STAC_RIGHT_MORE_SPEC:
      if (leaning == STAC_EQUAL) {
        leaning = STAC_RIGHT_MORE_SPEC;
      } else if (leaning == STAC_LEFT_MORE_SPEC) {
        leaning = STAC_INCOMPARABLE;
      }
      // right stays right and incomparable stays incomparable
      break;
    case STAC_EQUAL:
      // all stay same
      break;
    case STAC_INCOMPARABLE:
      leaning = STAC_INCOMPARABLE; // incomparable is an absorbing state
    }
  }
  xassert(rIter.isDone());      // we checked they had the same number of arguments earlier

  switch(leaning) {
  default: xfailure("illegal STemplateArgsCmp"); break;
  case STAC_LEFT_MORE_SPEC: return -1; break;
  case STAC_RIGHT_MORE_SPEC: return 1; break;
  case STAC_EQUAL:
    // FIX: perhaps this should be a user error?
    xfailure("Two template argument tuples are identical");
    break;
  case STAC_INCOMPARABLE: return 0; break;
  }
}


int TemplCandidates::compareCandidates(Variable const *left, Variable const *right)
{
  TemplateInfo *lti = const_cast<Variable*>(left)->templateInfo();
  xassert(lti);
  TemplateInfo *rti = const_cast<Variable*>(right)->templateInfo();
  xassert(rti);

  return compareCandidatesStatic(tfac, lti, rti);
}


// ----------------------- Env ----------------------------
// These are not all of Env's methods, just the ones declared in the
// section for templates.

Env::TemplTcheckMode Env::getTemplTcheckMode() const
{
  // one step towards removing this altogether...
  return tcheckMode;
  
  // ack!  the STA_REFERENCE hack depends on the mode...
  //return TTM_1NORMAL;
}


void Env::initArgumentsFromASTTemplArgs
  (TemplateInfo *tinfo,
   ASTList<TemplateArgument> const &templateArgs)
{
  xassert(tinfo);
  xassert(tinfo->arguments.count() == 0); // don't use this if there are already arguments
  FOREACH_ASTLIST(TemplateArgument, templateArgs, iter) {
    TemplateArgument const *targ = iter.data();
    if (targ->isTA_templateUsed()) continue;

    xassert(targ->sarg.hasValue());
    tinfo->arguments.append(new STemplateArgument(targ->sarg));
  }
}


bool Env::loadBindingsWithExplTemplArgs(Variable *var, ASTList<TemplateArgument> const &args,
                                        MatchTypes &match, InferArgFlags iflags)
{
  xassert(var->templateInfo());
  xassert(var->templateInfo()->isPrimary());
  xassert(match.getMode() == MatchTypes::MM_BIND);

  ASTListIter<TemplateArgument> argIter(args);
  if (!argIter.isDone() && argIter.data()->isTA_templateUsed()) {
    argIter.adv();      // skip TA_templateUsed
  }

  SObjListIterNC<Variable> paramIter(var->templateInfo()->params);
  for (; !paramIter.isDone() && !argIter.isDone();
       paramIter.adv(), argIter.adv()) {
    Variable *param = paramIter.data();
    TemplateArgument const *arg = argIter.data();

    // is the parameter already bound?  this happens e.g. during
    // explicit function instantiation, when the type of the function
    // can be used to infer some/all of the template parameters
    STemplateArgument const *existing = NULL;
    if (param->type->isTypeVariable()) {
      existing = match.bindings.getTypeVar(param->type->asTypeVariable());
    } else {
      // for, say, int template parameters
      existing = match.bindings.getObjVar(param);
    }

    // if so, it should agree with the explicitly provided argument
    if (existing) {
      if (!existing->equals(arg->sarg)) {
        if (iflags & IA_REPORT_ERRORS) {
          error(stringc << "for parameter `" << param->name
                        << "', inferred argument `" << existing->toString()
                        << "' does not match supplied argument `" << arg->sarg.toString()
                        << "'");
        }
        return false;
      }
      else {     
        // no need to get down into the (rather messy...) binding code
        // below
        continue;
      }
    }

    // FIX: when it is possible to make a TA_template, add
    // check for it here.
    //              xassert("Template template parameters are not implemented");

    if (param->hasFlag(DF_TYPEDEF) && arg->isTA_type()) {
      STemplateArgument *bound = new STemplateArgument;
      bound->setType(arg->asTA_typeC()->type->getType());
      match.bindings.putTypeVar(param->type->asTypeVariable(), bound);
    }
    else if (!param->hasFlag(DF_TYPEDEF) && arg->isTA_nontype()) {
      STemplateArgument *bound = new STemplateArgument;
      Expression *expr = arg->asTA_nontypeC()->expr;
      if (expr->getType()->isIntegerType()) {
        int staticTimeValue;
        bool constEvalable = expr->constEval(*this, staticTimeValue);
        if (!constEvalable) {
          // error already added to the environment
          return false;
        }
        bound->setInt(staticTimeValue);
      } else if (expr->getType()->isReference()) {
        // Subject: Re: why does STemplateArgument hold Variables?
        // From: Scott McPeak <smcpeak@eecs.berkeley.edu>
        // To: "Daniel S. Wilkerson" <dsw@eecs.berkeley.edu>
        // The Variable is there because STA_REFERENCE (etc.)
        // refer to (linker-visible) symbols.  You shouldn't
        // be making an STemplateArgument out of an expression
        // directly; if the template parameter is
        // STA_REFERENCE (etc.) then dig down to find the
        // Variable the programmer wants to use.
        //
        // haven't tried to exhaustively enumerate what kinds
        // of expressions this could be; handle other types as
        // they come up
        xassert(expr->isE_variable());
        bound->setReference(expr->asE_variable()->var);
      } else {
        unimp("unhandled kind of template argument");
        return false;
      }
      match.bindings.putObjVar(param, bound);
    }
    else {
      // mismatch between argument kind and parameter kind
      char const *paramKind = param->hasFlag(DF_TYPEDEF)? "type" : "non-type";
      char const *argKind = arg->isTA_type()? "type" : "non-type";
      // FIX: make a provision for template template parameters here

      // NOTE: condition this upon a boolean flag reportErrors if it
      // starts to fail while filtering functions for overload
      // resolution; see Env::inferTemplArgsFromFuncArgs() for an
      // example
      error(stringc
            << "`" << param->name << "' is a " << paramKind
            << " parameter, but `" << arg->argString() << "' is a "
            << argKind << " argument",
            EF_STRONG);
      return false;
    }
  }
  return true;
}


bool Env::inferTemplArgsFromFuncArgs
  (Variable *var,
   TypeListIter &argListIter,
   MatchTypes &match,
   InferArgFlags iflags)
{
  xassert(var->templateInfo());
  xassert(var->templateInfo()->isPrimary());
  xassert(match.hasEFlag(Type::EF_DEDUCTION));

  TRACE("template", "deducing template arguments from function arguments");

  // if the caller has passed in information about the receiver object
  // (so this function can work for nonstatic members), but the
  // function here is not a method, we need to skip the receiver
  FunctionType *funcType = var->type->asFunctionType();
  if ((iflags & IA_RECEIVER) &&          // receiver passed
      !funcType->isMethod()) {           // not a method
    argListIter.adv();                   // skip receiver
  }

  int i = 1;            // for error messages
  SFOREACH_OBJLIST_NC(Variable, funcType->params, paramIter) {
    Variable *param = paramIter.data();
    xassert(param);
    
    // 9/26/04: if the parameter does not contain any template params,
    // then strict matching is not required (I'm pretty sure I saw this
    // somewhere in cppstd, and gcc agrees (in/t0324.cc), but now I can't
    // find the proper reference...)
    //
    // TODO: Actually, this isn't quite right: if explicit template
    // arguments are supplied, and after the substitution is applied
    // this parameter's type *becomes* concrete, then we are supposed
    // to treat it as if it always was concrete.  But the way the code
    // is now, we will treat it as if it still was abstract and hence
    // needed to match almost exactly.
    if (!param->type->containsVariables()) {
      // skip it; no deduction can occur, and any convertibility errors
      // will be detected later
    }
    else {
      // we could run out of args and it would be ok as long as we
      // have default arguments for the rest of the parameters
      if (!argListIter.isDone()) {
        Type *curArgType = argListIter.data();
        bool argUnifies = match.match_Type(curArgType, param->type);

        if (!argUnifies) {
          // cppstd 14.8.2.1 para 3 bullet 3: if 'param->type' is a
          // template-id, then 'curArgType' can be derived from
          // 'param->type'; assume that 'containsVariables' is
          // sufficient evidence that 'param->type' is a template-id
          
          // for this comparison, push past any referenceness on
          // either argument or parameter (14.8.2.1 para 2 says so for
          // the parameter, less clear about the argument but I think
          // it falls under the general category of lvalue handling);
          // in any case if I get this wrong (too liberal) it should
          // be caught later once the deduced type is compared to the
          // argument types in the usual way
          curArgType = curArgType->asRval();
          Type *paramType = param->type->asRval();
          
          // push past one level of pointerness too (part of bullet 3)
          if (curArgType->isPointer() && paramType->isPointer()) {
            curArgType = curArgType->getAtType();
            paramType = paramType->getAtType();
          }

          if (curArgType->isCompoundType()) {
            // get all the base classes
            CompoundType *ct = curArgType->asCompoundType();
            SObjList<BaseClassSubobj const> bases;
            ct->getSubobjects(bases);
            SFOREACH_OBJLIST(BaseClassSubobj const, bases, iter) {
              BaseClassSubobj const *sub = iter.data();
              if (sub->ct == ct) { continue; }      // already tried 'ct'

              // attempt to match 'sub->ct' with 'param->type'
              //
              // TODO: There are two bugs here, due to allowing the
              // effect of one match attempt to contaminate the next.
              // First, if there are two base classes and the first
              // does not match but the second does, when the first
              // fails to match it may change the bindings in 'match'
              // in such a way as to cause the second match to
              // spuriously fail.  Second, cppstd says that we must
              // report an error if more than one base class matches,
              // but we will not be able to, since one successful
              // match will (in all likelihood) modify the bindings so
              // as to prevent the second match.  The solution is to
              // save the current bindings before attempting a match,
              // but MatchTypes does not currently support the needed
              // push and pop of bindings.  Therefore I will just note
              // the bugs and ignore them for now.
              Type *t = env.makeType(SL_UNKNOWN, sub->ct);    // leaked
              if (match.match_Type(t, paramType)) {
                argUnifies = true;
                break;
              }
            }
          }

          // did we find a match in the second attempt?
          if (!argUnifies) {
            if (iflags & IA_REPORT_ERRORS) {
              error(stringc << "during function template argument deduction: "
                    << "argument " << i << " `" << curArgType->toString() << "'"
                    << " is incompatible with parameter, `"
                    << param->type->toString() << "'");
            }
            return false;             // FIX: return a variable of type error?
          }
        }
      }
      else {
        // sm: 9/26/04: there used to be code here that reported an error
        // unless the parameter had a default argument, but that checking
        // is better done elsewhere (for uniformity)
      }
    }

    ++i;
    // advance the argIterCur if there is one
    if (!argListIter.isDone()) argListIter.adv();
  }
  
  // sm: 9/26/04: similarly, there used to be code that complained if there
  // were too many args; that is also checked elsewhere (t0026.cc ensures
  // that both conditions are caught)

  return true;
}


bool Env::getFuncTemplArgs
  (MatchTypes &match,
   ObjList<STemplateArgument> &sargs,
   PQName const *final,
   Variable *var,
   TypeListIter &argListIter,
   InferArgFlags iflags)
{
  TemplateInfo *varTI = var->templateInfo();
  xassert(varTI->isPrimary());

  // 'final' might be NULL in the case of doing overload resolution
  // for templatized ctors (that is, the ctor is templatized, but not
  // necessarily the class)
  if (final && final->isPQ_template()) {
    if (!loadBindingsWithExplTemplArgs(var, final->asPQ_templateC()->args, match,
                                       iflags)) {
      return false;
    }
  }

  if (!inferTemplArgsFromFuncArgs(var, argListIter, match, iflags)) {
    return false;
  }

  // put the bindings in a list in the right order
  bool haveAllArgs = true;

  // inherited params first
  FOREACH_OBJLIST(InheritedTemplateParams, varTI->inheritedParams, iter) {
    getFuncTemplArgs_oneParamList(match, sargs, iflags, haveAllArgs,
                                  /*piArgIter,*/ iter.data()->params);
  }

  // then main params
  getFuncTemplArgs_oneParamList(match, sargs, iflags, haveAllArgs,
                                /*piArgIter,*/ varTI->params);

  // certainly the partial instantiation should not have provided
  // more arguments than there are parameters!  it should not even
  // provide as many, but that's slightly harder to check
  //xassert(piArgIter.isDone());

  return haveAllArgs;
}

void Env::getFuncTemplArgs_oneParamList
  (MatchTypes &match,
   ObjList<STemplateArgument> &sargs,
   InferArgFlags iflags,
   bool &haveAllArgs,
   //ObjListIter<STemplateArgument> &piArgIter,
   SObjList<Variable> const &paramList)
{
  SFOREACH_OBJLIST(Variable, paramList, templPIter) {
    Variable const *param = templPIter.data();

    STemplateArgument const *sta = NULL;
    if (param->type->isTypeVariable()) {
      sta = match.bindings.getTypeVar(param->type->asTypeVariable());
    }
    else {
      sta = match.bindings.getObjVar(param);
    }

    if (!sta) {
      if (iflags & IA_REPORT_ERRORS) {
        error(stringc << "arguments do not bind template parameter `" 
                      << templPIter.data()->name << "'");
      }
      haveAllArgs = false;
    }
    else {
      // the 'sta' we have is owned by 'match' and will go away when
      // it does; make a copy that 'sargs' can own
      sargs.append(new STemplateArgument(*sta));
    }
  }
}


Variable *Env::lookupPQVariable_applyArgsTemplInst
  (Variable *primary, PQName const *name, FakeList<ArgExpression> *funcArgs)
{
  xassert(primary->isTemplateFunction());

  // if we are inside a function definition, just return the primary
  TemplTcheckMode ttm = getTemplTcheckMode();
  // FIX: this can happen when in the parameter list of a function
  // template definition a class template is instantiated (as a
  // Mutant)
  // UPDATE: other changes should prevent this
  xassert(ttm != TTM_2TEMPL_FUNC_DECL);
  if (ttm == TTM_2TEMPL_FUNC_DECL || ttm == TTM_3TEMPL_DEF) {
    xassert(primary->templateInfo()->isPrimary());
    return primary;
  }

  PQName const *final = name->getUnqualifiedNameC();

  // duck overloading
  if (primary->isOverloaded()) {
    xassert(primary->type->isFunctionType()); // only makes sense for function types
    // FIX: the correctness of this depends on someone doing
    // overload resolution later, which I don't think is being
    // done.
    return primary;
    // FIX: this isn't right; do overload resolution later;
    // otherwise you get a null signature being passed down
    // here during E_variable::itcheck_x()
    //              primary = oloadSet->findTemplPrimaryForSignature(signature);
    // // FIX: make this a user error, or delete it
    // xassert(primary);
  }
  xassert(primary->templateInfo()->isPrimary());

  // get the semantic template arguments
  ObjList<STemplateArgument> sargs;
  {
    TypeListIter_FakeList argListIter(funcArgs);
    MatchTypes match(tfac, MatchTypes::MM_BIND, Type::EF_DEDUCTION);
    if (!getFuncTemplArgs(match, sargs, final, primary, 
                          argListIter, IA_REPORT_ERRORS)) {
      return NULL;
    }
  }

  // apply the template arguments to yield a new type based on
  // the template; note that even if we had some explicit
  // template arguments, we have put them into the bindings now,
  // so we omit them here
  return instantiateFunctionTemplate(loc(), primary, sargs);
}


// insert bindings into SK_TEMPLATE_ARG scopes, from template
// parameters to concrete template arguments
//
// returns the deepest scope created
void Env::insertTemplateArgBindings
  (Variable *baseV, SObjList<STemplateArgument> &sargs)
{
  xassert(baseV);
  TemplateInfo *baseVTI = baseV->templateInfo();

  // begin iterating over arguments
  SObjListIterNC<STemplateArgument> argIter(sargs);

  // if 'baseV' is a partial instantiation, then it provides
  // a block of arguments at the beginning, and then we use 'sargs'
  SObjList<STemplateArgument> expandedSargs;
  if (baseVTI->isPartialInstantiation()) {
    // copy partial inst args first
    FOREACH_OBJLIST_NC(STemplateArgument, baseVTI->arguments, iter) {
      expandedSargs.append(iter.data());
    }

    // then 'sargs'
    SFOREACH_OBJLIST_NC(STemplateArgument, sargs, iter2) {
      expandedSargs.append(iter2.data());
    }
    
    // now, reset the iterator to walk the expanded list instead
    argIter.reset(expandedSargs);
    
    // finally, set 'baseVTI' to point at the original template,
    // since it has the parameter list for the definition
    baseVTI = baseVTI->partialInstantiationOf->templateInfo();
  }

  // does the definition parameter list differ from the declaration
  // parameter list?
  if (baseVTI->definitionTemplateInfo) {
    // use the params from the definition instead
    baseVTI = baseVTI->definitionTemplateInfo;
  }

  // first, apply them to the inherited parameters
  FOREACH_OBJLIST(InheritedTemplateParams, baseVTI->inheritedParams, iter) {
    InheritedTemplateParams const *inh = iter.data();

    // create a scope to hold the bindings
    Scope *s = enterScope(SK_TEMPLATE_ARGS, "inherited template argument bindings");

    // insert them
    insertTemplateArgBindings_oneParamList(s, baseV, argIter, inh->params);
  }

  // make a scope for the main template arguments; this one will be at
  // the very top, though it will then be covered by the scope of the
  // entity being instantiated (the caller does this)
  Scope *argScope = enterScope(SK_TEMPLATE_ARGS, "main template argument bindings");

  // then, bind "my" parameters
  insertTemplateArgBindings_oneParamList(argScope, baseV, argIter, baseVTI->params);

  if (!argIter.isDone()) {
    error(stringc
          << "too many template arguments to `" << baseV->name << "'", EF_STRONG);
  }
}

// returns false if an error is detected
bool Env::insertTemplateArgBindings_oneParamList
  (Scope *scope, Variable *baseV, SObjListIterNC<STemplateArgument> &argIter,
   SObjList<Variable> const &params)
{
  SObjListIter<Variable> paramIter(params);
  while (!paramIter.isDone()) {
    Variable const *param = paramIter.data();

    // if we have exhaused the explicit arguments, use a NULL 'sarg'
    // to indicate that we need to use the default arguments from
    // 'param' (if available)
    //
    // 8/10/04: Default arguments are now handled elsewhere
    // TODO: fully collapse this code to reflect that
    xassert(!argIter.isDone());       // should not get here with too few args
    STemplateArgument *sarg = /*argIter.isDone()? NULL :*/ argIter.data();

    if (sarg && sarg->isTemplate()) {
      xassert("Template template parameters are not implemented");
    }


    if (param->hasFlag(DF_TYPEDEF) &&
        (!sarg || sarg->isType())) {
      if (!sarg && !param->defaultParamType) {
        error(stringc
          << "too few template arguments to `" << baseV->name << "'");
        return false;
      }

      // bind the type parameter to the type argument
      Type *t = sarg? sarg->getType() : param->defaultParamType;
      Variable *binding = makeVariable(param->loc, param->name,
                                       t, DF_TYPEDEF | DF_BOUND_TARG);
      addVariableToScope(scope, binding);
    }
    else if (!param->hasFlag(DF_TYPEDEF) &&
             (!sarg || sarg->isObject())) {
      if (!sarg && !param->value) {
        error(stringc
          << "too few template arguments to `" << baseV->name << "'");
        return false;
      }

      // TODO: verify that the argument in fact matches the parameter type

      // bind the nontype parameter directly to the nontype expression;
      // this will suffice for checking the template body, because I
      // can const-eval the expression whenever it participates in
      // type determination; the type must be made 'const' so that
      // E_variable::constEval will believe it can evaluate it
      Type *bindType = tfac.applyCVToType(param->loc, CV_CONST,
                                          param->type, NULL /*syntax*/);
      Variable *binding = makeVariable(param->loc, param->name,
                                       bindType, DF_BOUND_TARG);

      // set 'bindings->value', in some cases creating AST
      if (!sarg) {
        binding->value = param->value;

        // sm: the statement above seems reasonable, but I'm not at
        // all convinced it's really right... has it been tcheck'd?
        // has it been normalized?  are these things necessary?  so
        // I'll wait for a testcase to remove this assertion... before
        // this assertion *is* removed, someone should read over the
        // applicable parts of cppstd
        xfailure("unimplemented: default non-type argument");
      }
      else if (sarg->kind == STemplateArgument::STA_INT) {
        E_intLit *value0 = buildIntegerLiteralExp(sarg->getInt());
        // FIX: I'm sure we can do better than SL_UNKNOWN here
        value0->type = tfac.getSimpleType(SL_UNKNOWN, ST_INT, CV_CONST);
        binding->value = value0;
      }
      else if (sarg->kind == STemplateArgument::STA_REFERENCE) {
        E_variable *value0 = new E_variable(NULL /*PQName*/);
        value0->var = sarg->getReference();
        binding->value = value0;
      }
      else {
        unimp(stringc << "STemplateArgument objects that are of kind other than "
              "STA_INT are not implemented: " << sarg->toString());
        return false;
      }
      xassert(binding->value);
      addVariableToScope(scope, binding);
    }
    else {
      // mismatch between argument kind and parameter kind
      char const *paramKind = param->hasFlag(DF_TYPEDEF)? "type" : "non-type";
      // FIX: make a provision for template template parameters here
      char const *argKind = sarg->isType()? "type" : "non-type";
      error(stringc
            << "`" << param->name << "' is a " << paramKind
            << " parameter, but `" << sarg->toString() << "' is a "
            << argKind << " argument", EF_STRONG);
    }

    paramIter.adv();
    if (!argIter.isDone()) {
      argIter.adv();
    }
  }

  // having added the bindings, turn off name acceptance
  scope->canAcceptNames = false;

  xassert(paramIter.isDone());
  return true;
}

void Env::insertTemplateArgBindings
  (Variable *baseV, ObjList<STemplateArgument> &sargs)
{
  insertTemplateArgBindings(baseV, objToSObjList(sargs));
}

                                                   
// reverse the effects of 'insertTemplateArgBindings'
void Env::deleteTemplateArgBindings()
{
  // just blow away template argument scopes on top
  while (scope()->scopeKind == SK_TEMPLATE_ARGS) {
    exitScope(scope());
  }
}


// The situation here is we have a partial specialization, for
// example
//
//   template <class T, class U>
//   class A<int, T*, U**> { ... }
//
// and we'd like to instantiate it with some concrete arguments.  What
// we have is arguments that apply to the *primary*, for example
//
//   <int, float*, char***>
//
// and we want to derive the proper arguments to the partial spec,
// namely
//
//   <float, char*>
//
// so we can pass these on to the instantiation routines.  
//
// It's a bit odd to be doing this matching again, since to even
// discover that the partial spec applied we would have already done
// it once.  For now I'll let that be...
void Env::mapPrimaryArgsToSpecArgs(
  Variable *baseV,         // partial specialization
  MatchTypes &match,       // carries 'bindings', which will own new arguments
  SObjList<STemplateArgument> &partialSpecArgs,      // dest. list
  SObjList<STemplateArgument> const &primaryArgs)    // source list
{
  // though the caller created this, it is really this function that
  // is responsible for deciding what arguments are used; so make sure
  // the caller did the right thing
  xassert(match.getMode() == MatchTypes::MM_BIND);

  // TODO: add 'const' to matchtypes
  SObjList<STemplateArgument> &hackPrimaryArgs =
    const_cast<SObjList<STemplateArgument>&>(primaryArgs);

  // execute the match to derive the bindings; we should not have
  // gotten here if they do not unify (Q: why the matchDepth==2?)
  TemplateInfo *baseVTI = baseV->templateInfo();
  bool matches = match.match_Lists(hackPrimaryArgs, baseVTI->arguments, 2 /*matchDepth*/);
  xassert(matches);

  // Now the arguments are bound in 'bindings', for example
  //
  //   T |-> float
  //   U |-> char
  //
  // We just need to run over the partial spec's parameters and
  // build an argument corresponding to each parameter.

  // first get args corresp. to inherited params
  FOREACH_OBJLIST(InheritedTemplateParams, baseVTI->inheritedParams, iter) {
    mapPrimaryArgsToSpecArgs_oneParamList(iter.data()->params, match, partialSpecArgs);
  }

  // then the main params
  mapPrimaryArgsToSpecArgs_oneParamList(baseVTI->params, match, partialSpecArgs);
}

void Env::mapPrimaryArgsToSpecArgs_oneParamList(
  SObjList<Variable> const &params,     // one arg per parameter
  MatchTypes &match,                    // carries bindingsto use
  SObjList<STemplateArgument> &partialSpecArgs)      // dest. list
{
  SFOREACH_OBJLIST(Variable, params, iter) {
    Variable const *param = iter.data();

    STemplateArgument const *arg = NULL;
    if (param->type->isTypeVariable()) {
      arg = match.bindings.getTypeVar(param->type->asTypeVariable());
    }
    else {
      arg = match.bindings.getObjVar(param);
    }
    if (!arg) {
      error(stringc
            << "during partial specialization parameter `" << param->name
            << "' not bound in inferred bindings", EF_STRONG);
      return;
    }

    // Cast away constness... the reason it's const to begin with is
    // that 'bindings' doesn't want me to change it, and as a reminder
    // that 'bindings' owns it so it will go away when 'bindings'
    // does.  Since passing 'arg' to 'insertTemplateArgBindings' will
    // not change it, and since I understand the lifetime
    // relationships, this should be safe.
    partialSpecArgs.append(const_cast<STemplateArgument*>(arg));
  }
}


// go over the list of arguments, and extract a list of semantic
// arguments
bool Env::templArgsASTtoSTA
  (ASTList<TemplateArgument> const &arguments,
   SObjList<STemplateArgument> &sargs)
{
  FOREACH_ASTLIST(TemplateArgument, arguments, iter) {
    // the caller wants me not to modify the list, and I am not going
    // to, but I need to put non-const pointers into the 'sargs' list
    // since that's what the interface to 'equalArguments' expects..
    // this is a problem with const-polymorphism again
    TemplateArgument *ta = const_cast<TemplateArgument*>(iter.data());
    
    if (ta->isTA_templateUsed()) {
      // skip this, it is not a real argument
      continue;
    }

    if (!ta->sarg.hasValue()) {
      // sm: 7/24/04: This used to be a user error, but I think it should
      // be an assertion because it is referring to internal implementation
      // details, not anything the user knows about.
      //xfailure(stringc << "TemplateArgument has no value " << ta->argString());
      //
      // sm: 8/10/04: push reporting/handling obligation back to
      // caller; this can happen when a template argument has an error
      // (e.g., refers to an undeclared identifier) (t0245.cc, error 1)
      return false;
    }
    sargs.prepend(&(ta->sarg));     // O(1)
  }
  sargs.reverse();                  // O(n)
  
  return true;
}


// TODO: can this be removed?  what goes wrong if we use MM_BIND
// always?
MatchTypes::MatchMode Env::mapTcheckModeToTypeMatchMode(TemplTcheckMode tcheckMode)
{
  // map the typechecking mode to the type matching mode
  MatchTypes::MatchMode matchMode = MatchTypes::MM_NONE;
  switch(tcheckMode) {
  default: xfailure("bad mode"); break;
  case TTM_1NORMAL:
    matchMode = MatchTypes::MM_BIND;
    break;
  case TTM_2TEMPL_FUNC_DECL:
    matchMode = MatchTypes::MM_ISO;
    break;
  case TTM_3TEMPL_DEF:
    // 8/09/04: we used to not instantiate anything in mode 3 (template
    // definition), but in the new design I see no harm in allowing it;
    // I think we should use bind mode
    matchMode = MatchTypes::MM_BIND;
    break;
  }
  return matchMode;
}


// find most specific specialization that matches the given arguments
Variable *Env::findMostSpecific
  (Variable *baseV, SObjList<STemplateArgument> const &sargs)
{
  // baseV should be a template primary
  TemplateInfo *baseVTI = baseV->templateInfo();
  xassert(baseVTI->isPrimary());

  // ?
  MatchTypes::MatchMode matchMode =
    mapTcheckModeToTypeMatchMode(getTemplTcheckMode());

  // iterate through all of the specializations and build up a set of
  // candidates
  TemplCandidates templCandidates(tfac);
  SFOREACH_OBJLIST_NC(Variable, baseVTI->specializations, iter) {
    Variable *spec = iter.data();
    TemplateInfo *specTI = spec->templateInfo();
    xassert(specTI);        // should have templateness

    // TODO: add 'const' to matchtype
    SObjList<STemplateArgument> &hackSargs =
      const_cast<SObjList<STemplateArgument>&>(sargs);

    // see if this candidate matches
    MatchTypes match(tfac, matchMode);
    if (match.match_Lists(hackSargs, specTI->arguments, 2 /*matchDepth (?)*/)) {
      templCandidates.add(spec);
    }
  }

  // there are no candidates so we just use the primary
  if (templCandidates.candidates.isEmpty()) {
    return baseV;
  }

  // there are candidates to try; select the best
  Variable *bestV = selectBestCandidate_templCompoundType(templCandidates);

  // if there is not best candidate, then the call is ambiguous and
  // we should deal with that error
  if (!bestV) {
    // TODO: expand this error message
    error(stringc << "ambiguous attempt to instantiate template", EF_STRONG);
    return baseV;      // recovery: use the primary
  }

  // otherwise, use the best one
  return bestV;
}


// remove scopes from the environment until the innermost
// scope on the scope stack is the same one that the template
// definition appeared in; template definitions can see names
// visible from their defining scope only [cppstd 14.6 para 1]
//
// update: (e.g. t0188.cc) pop scopes until we reach one that
// *contains* (or equals) the defining scope
//
// 4/20/04: Even more (e.g. t0204.cc), we need to push scopes
// to dig back down to the defining scope.  So the picture now
// looks like this:
//
//       global                   /---- this is "foundScope"
//         |                     /
//         namespace1           /   }
//         | |                 /    }- 2. push these: "pushedScopes"
//         | namespace11  <---/     }
//         |   |
//         |   template definition
//         |
//         namespace2               }
//           |                      }- 1. pop these: "poppedScopes"
//           namespace21            }
//             |
//             point of instantiation
//
// actually, it's *still* not right, because
//   - this allows the instantiation to see names declared in
//     'namespace11' that are below the template definition, and
//   - it's entirely wrong for dependent names, a concept I
//     largely ignore at this time
// however, I await more examples before continuing to refine
// my approximation to the extremely complex lookup rules
//
// makeEatScope: first step towards removing SK_EAT_TEMPL_SCOPE
// altogether
void Env::prepArgScopeForTemlCloneTcheck
  (ObjList<SavedScopePair> &poppedScopes, SObjList<Scope> &pushedScopes, 
   Scope *foundScope)
{
  xassert(foundScope);

  // pop scope scopes
  while (!scopes.first()->enclosesOrEq(foundScope)) {
    Scope *s = scopes.removeFirst();
    TRACE("scope", "prepArgScope: removing " << s->desc());

    // do I need to save a delegation pointer?
    SavedScopePair *ssp = new SavedScopePair(s);
    if (s->hasDelegationPointer()) {
      ssp->parameterizingScope = s->getAndNullifyDelegationPointer();
      TRACE("scope", "prepArgScope: ... and saved delegation ptr " << 
                     ssp->parameterizingScope->desc());
    }

    poppedScopes.prepend(ssp);
    if (scopes.isEmpty()) {
      xfailure("emptied scope stack searching for defining scope");
    }
  }

  // make a list of the scopes to push; these form a path from our
  // current scope to the 'foundScope'
  Scope *s = foundScope;
  while (s != scopes.first()) {
    pushedScopes.prepend(s);
    s = s->parentScope;
    if (!s) {
      if (scopes.first()->isGlobalScope()) {
        // ok, hit the global scope in the traversal
        break;
      }
      else {
        xfailure("missed the current scope while searching up "
                 "from the defining scope");
      }
    }
  }

  // now push them in list order, which means that 'foundScope'
  // will be the last one to be pushed, and hence the innermost
  // (I waited until now b/c this is the opposite order from the
  // loop above that fills in 'pushedScopes')
  SFOREACH_OBJLIST_NC(Scope, pushedScopes, iter) {
    // Scope 'iter.data()' is now on both lists, but neither owns
    // it; 'scopes' does not own Scopes that are named, as explained
    // in the comments near its declaration (cc_env.h)
    TRACE("scope", "prepArgScope: adding " << iter.data()->desc());
    scopes.prepend(iter.data());
  }
}


void Env::unPrepArgScopeForTemlCloneTcheck
  (ObjList<SavedScopePair> &poppedScopes, SObjList<Scope> &pushedScopes)
{
  // restore the original scope structure
  pushedScopes.reverse();
  while (pushedScopes.isNotEmpty()) {
    // make sure the ones we're removing are the ones we added
    xassert(scopes.first() == pushedScopes.first());
    TRACE("scope", "unPrepArgScope: removing " << scopes.first()->desc());
    scopes.removeFirst();
    pushedScopes.removeFirst();
  }

  // re-add the inner scopes removed above
  while (poppedScopes.isNotEmpty()) {
    SavedScopePair *ssp = poppedScopes.removeFirst();

    // take out the scope and nullify it, effectively transferring ownership
    Scope *s = ssp->scope;
    ssp->scope = NULL;
    TRACE("scope", "unPrepArgScope: adding " << s->desc());

    // replace the parameterizingScope if needed
    if (ssp->parameterizingScope) {
      s->setDelegationPointer(ssp->parameterizingScope);
      TRACE("scope", "... and restored delegation ptr " <<
                     ssp->parameterizingScope->desc());
    }

    scopes.prepend(s);
    delete ssp;
  }
}


// --------------- function template instantiation ------------
// Get or create an instantiation Variable for a function template.
// Note that this does *not* instantiate the function body; instead,
// instantiateFunctionBody() has that responsibility.
Variable *Env::instantiateFunctionTemplate
  (SourceLoc loc,                              // location of instantiation request
   Variable *primary,                          // template primary to instantiate
   SObjList<STemplateArgument> const &sargs)   // arguments to apply to 'primary'
{
  TemplateInfo *primaryTI = primary->templateInfo();
  xassert(primaryTI->isPrimary());

  // look for a (complete) specialization that matches
  Variable *spec = findCompleteSpecialization(primaryTI, sargs);
  if (spec) {
    return spec;      // use it
  }

  // look for an existing instantiation that has the right arguments
  Variable *inst = findInstantiation(primaryTI, sargs);
  if (inst) {
    return inst;      // found it; that's all we need
  }

  // since we didn't find an existing instantiation, we have to make
  // one from scratch
  TRACE("template", "instantiating func decl: " <<
                    primary->fullyQualifiedName() << sargsToString(sargs));

  // I don't need this, right?
  // isolate context
  //InstantiationContextIsolator isolator(*this, loc);

  // bind the parameters in an STemplateArgumentMap
  STemplateArgumentCMap map;
  bindParametersInMap(map, primaryTI, sargs);

  // compute the type of the instantiation by applying 'map' to
  // the templatized type
  Type *instType = applyArgumentMapToType(map, primary->type);
  
  // pacify Oink...
  instType = tfac.cloneType(instType);

  // create the representative Variable
  inst = makeInstantiationVariable(primary, instType);

  // TODO: fold the following three activities into
  // 'makeInstantiationVariable'

  // annotate it with information about its templateness
  TemplateInfo *instTI = new TemplateInfo(loc, inst);
  instTI->copyArguments(sargs);

  // insert into the instantiation list of the primary
  primaryTI->addInstantiation(inst);

  // this is an instantiation
  xassert(instTI->isInstantiation());

  return inst;
}

Variable *Env::instantiateFunctionTemplate
  (SourceLoc loc,
   Variable *primary,
   ObjList<STemplateArgument> const &sargs)
{
  return instantiateFunctionTemplate(loc, primary,
    objToSObjListC(sargs));
}


void Env::ensureFuncBodyTChecked(Variable *instV)
{
  if (!instV) {
    return;      // error recovery
  }
  if (!instV->type->isFunctionType()) {
    // I'm not sure what circumstances can cause this, but it used
    // to be that all call sites to this function were guarded by
    // this 'isFunctionType' check, so I pushed it inside
    return;
  }

  TemplateInfo *instTI = instV->templateInfo();
  if (!instTI) {
    // not a template instantiation
    return;
  }
  if (!instTI->isCompleteSpecOrInstantiation()) {
    // not an instantiation; this might be because we're in
    // the middle of tchecking a template definition, so we
    // just used a function template primary sort of like
    // a PseudoInstantiation; skip checking it
    return;
  }
  if (instTI->instantiateBody) {
    // we've already seen this request, so either the function has
    // already been instantiated, or else we haven't seen the
    // definition yet so there's nothing we can do
    return;
  }

  // acknowledge the request
  instTI->instantiateBody = true;

  // what template am I an instance of?
  Variable *baseV = instTI->instantiationOf;
  if (!baseV) {
    // This happens for explicit complete specializations.  It's
    // not clear whether such things should have templateInfos
    // at all, but there seems little harm, so I'll just bail in
    // that case
    return;
  }

  // have we seen a definition of it?
  if (!baseV->funcDefn) {
    // nope, nothing we can do yet
    TRACE("template", "want to instantiate func body: " << 
                      instV->toQualifiedString() << 
                      ", but cannot because have not seen defn");
    return;
  }

  // ok, at this point we're committed to going ahead with
  // instantiating the function body
  instantiateFunctionBody(instV);
}

void Env::instantiateFunctionBody(Variable *instV)
{ 
  if (!doFunctionTemplateBodyInstantiation) {
    TRACE("template", "NOT instantiating func body: " << 
                      instV->toQualifiedString() <<
                      " because body instantiation is disabled");
    return;
  }

  TRACE("template", "instantiating func body: " << instV->toQualifiedString());
  
  // reconstruct a few variables from above
  TemplateInfo *instTI = instV->templateInfo();
  Variable *baseV = instTI->instantiationOf;

  // someone should have requested this
  xassert(instTI->instantiateBody);

  // isolate context
  InstantiationContextIsolator isolator(*this, loc());

  // defnScope: the scope where the function definition appeared.
  Scope *defnScope;

  // do we have a function definition already?
  if (instV->funcDefn) {
    // inline definition
    defnScope = instTI->defnScope;
  }
  else {
    // out-of-line definition; must clone the primary's definition
    instV->funcDefn = baseV->funcDefn->clone();
    defnScope = baseV->templateInfo()->defnScope;
  }

  // set up the scopes in a way similar to how it was when the
  // template definition was first seen
  ObjList<SavedScopePair> poppedScopes;
  SObjList<Scope> pushedScopes;
  prepArgScopeForTemlCloneTcheck(poppedScopes, pushedScopes, defnScope);

  // bind the template arguments in scopes so that when we tcheck the
  // body, lookup will find them
  insertTemplateArgBindings(baseV, instTI->arguments);

  // push the declaration scopes for inline definitions, since
  // we don't get those from the declarator (that is in fact a
  // mistake of the current implementation; eventually, we should
  // 'pushDeclarationScopes' regardless of DF_INLINE_DEFN)
  if (instV->funcDefn->dflags & DF_INLINE_DEFN) {
    pushDeclarationScopes(instV, defnScope);
  }

  // check the body, forcing it to use 'instV'
  instV->funcDefn->tcheck(*this, instV);

  // remove the template argument scopes
  deleteTemplateArgBindings();

  if (instV->funcDefn->dflags & DF_INLINE_DEFN) {
    popDeclarationScopes(instV, defnScope);
  }

  unPrepArgScopeForTemlCloneTcheck(poppedScopes, pushedScopes);
  xassert(poppedScopes.isEmpty() && pushedScopes.isEmpty());
}


void Env::instantiateForwardFunctions(Variable *primary)
{
  if (!primary->templateInfo()) {
    return;      // error recovery (t0275.cc)
  }

  SFOREACH_OBJLIST_NC(Variable, primary->templateInfo()->instantiations, iter) {
    Variable *inst = iter.data();
    
    if (inst->templateInfo()->instantiateBody) {
      instantiateFunctionBody(inst);
    }
  }
}


// ----------------- class template instantiation -------------
// Get or create an instantiation Variable for a class template.
// Note that this does *not* instantiate the class body; instead,
// instantiateClassBody() has that responsibility.
//
// Return NULL if there is a problem with the arguments.
Variable *Env::instantiateClassTemplate
  (SourceLoc loc,                             // location of instantiation request
   Variable *primary,                         // template primary to instantiate
   SObjList<STemplateArgument> const &origPrimaryArgs)  // arguments to apply to 'primary'
{
  // I really don't know what's the best place to do this, but I
  // need it here so this is a start...
  primary = primary->skipAlias();

  TemplateInfo *primaryTI = primary->templateInfo();
  xassert(primaryTI->isPrimary());

  // Augment the supplied arguments with defaults from the primary
  // (note that defaults on a specialization are not used, and are
  // consequently illegal [14.5.4 para 10]).
  //
  // This also checks whether the arguments match the parameters,
  // and returns false if they do not.
  ObjList<STemplateArgument> owningPrimaryArgs;
  if (!supplyDefaultTemplateArguments(primaryTI, owningPrimaryArgs,
                                      origPrimaryArgs)) {
    return NULL;
  }

  // The code below wants to use SObjLists, and since they are happy
  // accepting const versions, this is safe.  An alternative fix would
  // be to push owningness down into those interfaces, but I'm getting
  // tired of doing that ...
  SObjList<STemplateArgument> const &primaryArgs =     // non-owning
    objToSObjListC(owningPrimaryArgs);

  // find the specialization that should be used (might turn
  // out to be the primary; that's fine)
  Variable *spec = findMostSpecific(primary, primaryArgs);
  TemplateInfo *specTI = spec->templateInfo();
  if (specTI->isCompleteSpec()) {
    return spec;      // complete spec; good to go
  }

  // if this is a partial specialization, we need the arguments
  // to be relative to the partial spec before we can look for
  // the instantiation
  MatchTypes match(tfac, MatchTypes::MM_BIND);
  SObjList<STemplateArgument> partialSpecArgs;
  if (spec != primary) {
    xassertdb(specTI->isPartialSpec());
    mapPrimaryArgsToSpecArgs(spec, match, partialSpecArgs, primaryArgs);
  }

  // look for an existing instantiation that has the right arguments
  Variable *inst = spec==primary?
    findInstantiation(specTI, primaryArgs) :
    findInstantiation(specTI, partialSpecArgs);
  if (inst) {
    return inst;      // found it; that's all we need
  }

  // since we didn't find an existing instantiation, we have to make
  // one from scratch
  if (spec==primary) {
    TRACE("template", "instantiating class decl: " <<
                      primary->fullyQualifiedName() << sargsToString(primaryArgs));
  }
  else {
    TRACE("template", "instantiating partial spec decl: " <<
                      primary->fullyQualifiedName() <<
                      sargsToString(specTI->arguments) <<
                      sargsToString(partialSpecArgs));
  }

  // create the CompoundType
  CompoundType const *specCT = spec->type->asCompoundType();
  CompoundType *instCT = tfac.makeCompoundType(specCT->keyword, specCT->name);
  instCT->forward = true;
  instCT->instName = str(stringc << specCT->name << sargsToString(primaryArgs));
  instCT->parentScope = specCT->parentScope;

  // wrap the compound in a regular type
  Type *instType = makeType(loc, instCT);

  // create the representative Variable
  inst = makeInstantiationVariable(spec, instType);

  // this also functions as the implicit typedef for the class,
  // though it is not entered into any scope
  instCT->typedefVar = inst;

  // also make the self-name, which *does* go into the scope
  // (testcase: t0167.cc)
  if (lang.compoundSelfName) {
    Variable *self = makeVariable(loc, instCT->name, instType,
                                  DF_TYPEDEF | DF_SELFNAME);
    instCT->addUniqueVariable(self);
    addedNewVariable(instCT, self);
  }

  // make a TemplateInfo for this instantiation
  TemplateInfo *instTI = new TemplateInfo(loc, inst);

  // fill in its arguments
  instTI->copyArguments(spec==primary? primaryArgs : partialSpecArgs);
  
  // if it is an instance of a partial spec, keep the primaryArgs too ...
  if (spec!=primary) {
    copyTemplateArgs(instTI->argumentsToPrimary, primaryArgs);
  }

  // attach it as an instance
  specTI->addInstantiation(inst);

  // this is an instantiation
  xassert(instTI->isInstantiation());

  return inst;
}

Variable *Env::instantiateClassTemplate
  (SourceLoc loc,
   Variable *primary,
   ObjList<STemplateArgument> const &sargs)
{
  return instantiateClassTemplate(loc, primary, objToSObjListC(sargs));
}


void Env::instantiateClassBody(Variable *inst)
{
  TemplateInfo *instTI = inst->templateInfo();
  CompoundType *instCT = inst->type->asCompoundType();

  Variable *spec = instTI->instantiationOf;
  TemplateInfo *specTI = spec->templateInfo();
  CompoundType *specCT = spec->type->asCompoundType();

  TRACE("template", "instantiating " <<
                    (specTI->isPrimary()? "class" : "partial spec") <<
                    " body: " << instTI->templateName());

  if (specCT->forward) {
    error(stringc << "attempt to instantiate `" << instTI->templateName()
                  << "', but no definition has been provided for `"
                  << specTI->templateName() << "'");
    return;
  }

  // isolate context
  InstantiationContextIsolator isolator(*this, loc());

  // defnScope: the scope where the class definition appeared
  Scope *defnScope;

  // do we have a function definition already?
  if (instCT->syntax) {
    // inline definition
    defnScope = instTI->defnScope;
  }
  else {
    // out-of-line definition; must clone the spec's definition
    instCT->syntax = specCT->syntax->clone();
    defnScope = specTI->defnScope;
  }
  xassert(instCT->syntax);
  xassert(defnScope);

  // set up the scopes in a way similar to how it was when the
  // template definition was first seen
  ObjList<SavedScopePair> poppedScopes;
  SObjList<Scope> pushedScopes;
  prepArgScopeForTemlCloneTcheck(poppedScopes, pushedScopes, defnScope);

  // bind the template arguments in scopes so that when we tcheck the
  // body, lookup will find them
  insertTemplateArgBindings(spec, instTI->arguments);

  // check the class body, forcing it to use 'instCT'
  instCT->syntax->name->tcheck(*this);
  instCT->syntax->ctype = instCT;

  // don't check method bodies
  {
    Restorer<bool> r(checkFunctionBodies, false);
    instCT->syntax->tcheckIntoCompound(*this, DF_NONE, instCT);
  }

  // Now, we've just tchecked the clone in an environment that
  // makes all the type variables map to concrete types, so we
  // now have a nice, ordinary, non-template class with a bunch
  // of members.  But there is information stored in the
  // original AST that needs to be transferred over to the
  // clone, namely information about out-of-line definitions.
  // We need both the Function pointers and the list of template
  // params used at the definition site (since we have arguments
  // but don't know what names to bind them to).  So, walk over
  // both member lists, transferring information as necessary.
  transferTemplateMemberInfo(loc(), specCT->syntax, instCT->syntax,
                             instTI->arguments);

  // the instantiation is now complete
  instCT->forward = false;

  // restore the scopes
  deleteTemplateArgBindings();
  unPrepArgScopeForTemlCloneTcheck(poppedScopes, pushedScopes);
  xassert(poppedScopes.isEmpty() && pushedScopes.isEmpty());
}


// this is for 14.7.1 para 4, among other things
void Env::ensureClassBodyInstantiated(CompoundType *ct)
{
  if (!ct->isComplete() && ct->isInstantiation()) {
    instantiateClassBody(ct->typedefVar);
  }
}

// given a function type whose parameters are about to be considered
// for various conversions, make sure that all relevant template
// classes are instantiated
void Env::instantiateTemplatesInParams(FunctionType *ft)
{
  SFOREACH_OBJLIST(Variable, ft->params, iter) {
    Type *paramType = iter.data()->type;
    if (paramType->asRval()->isCompoundType()) {
      ensureClassBodyInstantiated(paramType->asRval()->asCompoundType());
    }
  }
}


void Env::instantiateForwardClasses(Variable *baseV)
{
  // temporarily supress TTM_3TEMPL_DEF and return to TTM_1NORMAL for
  // purposes of instantiating the forward classes
  Restorer<TemplTcheckMode> restoreMode(tcheckMode, TTM_1NORMAL);

  SFOREACH_OBJLIST_NC(Variable, baseV->templateInfo()->instantiations, iter) {
    instantiateClassBody(iter.data());
  }
}


// return false on error
bool Env::supplyDefaultTemplateArguments
  (TemplateInfo *primaryTI,
   ObjList<STemplateArgument> &dest,          // arguments to use for instantiation
   SObjList<STemplateArgument> const &src)    // arguments supplied by user
{
  // since default arguments can refer to earlier parameters,
  // maintain a map of the arguments known so far
  STemplateArgumentCMap map;

  // simultanously iterate over arguments and parameters, building
  // 'dest' as we go
  SObjListIter<Variable> paramIter(primaryTI->params);
  SObjListIter<STemplateArgument> argIter(src);
  while (!paramIter.isDone()) {
    Variable const *param = paramIter.data();

    STemplateArgument *arg = NULL;     // (owner)

    // take argument from explicit list?
    if (!argIter.isDone()) {
      arg = argIter.data()->shallowClone();
      argIter.adv();

      // TODO: check that this argument matches the template parameter
    }

    // default?
    else {
      arg = makeDefaultTemplateArgument(paramIter.data(), map);
      if (arg) {
        TRACE("template", "supplied default argument `" <<
                          arg->toString() << "' for param `" <<
                          param->name << "'");
      }
    }

    if (!arg) {
      error(stringc << "no argument supplied for template parameter `"
                    << param->name << "'");
      return false;
    }

    // save this argument
    dest.append(arg);
    map.add(param->name, arg);

    paramIter.adv();
  }

  if (!argIter.isDone()) {
    error(stringc << "too many arguments supplied to template `"
                  << primaryTI->templateName() << "'");
    return false;
  }

  return true;
}


STemplateArgument *Env::makeDefaultTemplateArgument
  (Variable const *param, STemplateArgumentCMap &map)
{
  // type parameter?
  if (param->hasFlag(DF_TYPEDEF) &&
      param->defaultParamType) {
    // use 'param->defaultParamType', but push it through the map
    // so it can refer to previous arguments 
    Type *t = applyArgumentMapToType(map, param->defaultParamType);
    return new STemplateArgument(t);
  }
  
  // non-type parameter?
  else if (!param->hasFlag(DF_TYPEDEF) &&
           param->value) {
    // This was unimplemented in the old code, so I'm going to
    // keep that behavior.  Back then I said, in reference to
    // simply using 'param->value' directly:
    //
    // sm: the statement above seems reasonable, but I'm not at
    // all convinced it's really right... has it been tcheck'd?
    // has it been normalized?  are these things necessary?  so
    // I'll wait for a testcase to remove this assertion... before
    // this assertion *is* removed, someone should read over the
    // applicable parts of cppstd
    //xfailure("unimplemented: default non-type argument");
    
    // 8/21/04: attempting to implement w/o reading cppstd... :}
    int val;
    STemplateArgument *ret = new STemplateArgument;
    if (param->value->constEval(*this, val)) {
      // I am just hoping it's an int ....
      ret->setInt(val);
    }
    else {
      // error already reported, but proceed anyway
      ret->setInt(0);
    }
    return ret;
  }
  
  return NULL;
}


// transfer template info from members of 'source' to corresp.
// members of 'dest'; 'dest' is a clone of 'source'
void Env::transferTemplateMemberInfo
  (SourceLoc instLoc, TS_classSpec *source,
   TS_classSpec *dest, ObjList<STemplateArgument> const &sargs)
{
  // simultanous iteration
  ASTListIterNC<Member> srcIter(source->members->list);
  ASTListIterNC<Member> destIter(dest->members->list);

  for (; !srcIter.isDone() && !destIter.isDone();
         srcIter.adv(), destIter.adv()) {
    if (srcIter.data()->isMR_decl()) {
      Declaration *srcDecl = srcIter.data()->asMR_decl()->d;
      Declaration *destDecl = destIter.data()->asMR_decl()->d;

      if (srcDecl->dflags & DF_FRIEND) {
        continue;     // skip whole declaration for friends (t0262.cc)
      }

      // associate the type specifiers
      transferTemplateMemberInfo_typeSpec(instLoc, srcDecl->spec, source->ctype,
                                          destDecl->spec, sargs);

      // simultanously iterate over the declarators
      FakeList<Declarator> *srcDeclarators = srcDecl->decllist;
      FakeList<Declarator> *destDeclarators = destDecl->decllist;

      for (; srcDeclarators->isNotEmpty() && destDeclarators->isNotEmpty();
             srcDeclarators = srcDeclarators->butFirst(),
             destDeclarators = destDeclarators->butFirst()) {
        Variable *srcVar = srcDeclarators->first()->var;
        Variable *destVar = destDeclarators->first()->var;

        if (srcVar->type->isFunctionType()) {
          // srcVar -> destVar
          transferTemplateMemberInfo_one(instLoc, srcVar, destVar, sargs);
        }

        // TODO: what about nested classes?
        else if (srcIter.data()->asMR_decl()->d->spec->isTS_classSpec()) {
          unimp("nested class of a template class");
        }
      }
      xassert(srcDeclarators->isEmpty() && destDeclarators->isEmpty());
    }

    else if (srcIter.data()->isMR_func()) {
      Variable *srcVar = srcIter.data()->asMR_func()->f->nameAndParams->var;
      Variable *destVar = destIter.data()->asMR_func()->f->nameAndParams->var;

      transferTemplateMemberInfo_one(instLoc, srcVar, destVar, sargs);

      // the instance 'destVar' needs to have a 'defnScope'; it didn't
      // get set earlier b/c at the time the declaration was tchecked,
      // the Function didn't know it was an instantiation (but why is
      // that?)
      TemplateInfo *destTI = destVar->templateInfo();
      if (!destTI->defnScope) {
        destTI->defnScope = destVar->scope;
        xassert(destTI->defnScope);

        // arg.. I keep pushing this around.. maybe new strategy:
        // set defnScope and funcDefn at same time?
        destVar->funcDefn = destIter.data()->asMR_func()->f;
      }
      else {
        // this happens when 'destVar' is actually a partial instantiation,
        // so the scope was set by transferTemplateMemberInfo_membert
        // when ..._one delegated to it
        xassert(destTI->isPartialInstantiation());
      }
    }

    else if (srcIter.data()->isMR_template()) {
      TemplateDeclaration *srcTDecl = srcIter.data()->asMR_template()->d;
      TemplateDeclaration *destTDecl = destIter.data()->asMR_template()->d;

      // I've decided that member templates should just be treated as
      // primaries in their own right, right no relation to the
      // "original" definition, hence no action is needed!
      //
      // ah, but there is still the need to xfer the funcDefn, and to
      // find the instantiations later, for out-of-line defns, plus they
      // need to remember the template arguments.  so, I'm introducing
      // the notion of "partial instantiation"

      if (srcTDecl->isTD_proto()) {
        Variable *srcVar = srcTDecl->asTD_proto()->d->decllist->first()->var;
        Variable *destVar = destTDecl->asTD_proto()->d->decllist->first()->var;

        transferTemplateMemberInfo_membert(instLoc, srcVar, destVar, sargs);
      }

      else if (srcTDecl->isTD_func()) {
        Variable *srcVar = srcTDecl->asTD_func()->f->nameAndParams->var;
        Variable *destVar = destTDecl->asTD_func()->f->nameAndParams->var;

        transferTemplateMemberInfo_membert(instLoc, srcVar, destVar, sargs);
      }

      else if (srcTDecl->isTD_class()) {
        transferTemplateMemberInfo_typeSpec(instLoc,
          srcTDecl->asTD_class()->spec, source->ctype,
          destTDecl->asTD_class()->spec, sargs);
      }

      else if (srcTDecl->isTD_tmember()) {
        // not sure if this would even parse... if it does I don't
        // know what it might mean
        error("more than one template <...> declaration inside a class body?");
      }

      else {
        xfailure("unknown TemplateDeclaration kind");
      }
    }

    else {
      // other kinds of member decls: don't need to do anything
    }
  }

  // one is clone of the other, so same length lists
  xassert(srcIter.isDone() && destIter.isDone());
}


// transfer specifier info, particularly for nested class or
// member template classes
void Env::transferTemplateMemberInfo_typeSpec
  (SourceLoc instLoc, TypeSpecifier *srcTS, CompoundType *sourceCT,
   TypeSpecifier *destTS, ObjList<STemplateArgument> const &sargs)
{
  if (srcTS->isTS_elaborated()) {
    Variable *srcVar = srcTS->asTS_elaborated()->atype->typedefVar;
    Variable *destVar = destTS->asTS_elaborated()->atype->typedefVar;

    if (srcVar->scope == sourceCT) {
      // just a forward decl, do the one element
      transferTemplateMemberInfo_one(instLoc, srcVar, destVar, sargs);
    }
    else {
      // this isn't a declaration of a type that is a member of the
      // relevant template, it is just a reference to some other type;
      // ignore it
    }
  }

  else if (srcTS->isTS_classSpec()) {
    TS_classSpec *srcCS = srcTS->asTS_classSpec();
    TS_classSpec *destCS = destTS->asTS_classSpec();

    // connect the classes themselves
    transferTemplateMemberInfo_one(instLoc,
      srcCS->ctype->typedefVar,
      destCS->ctype->typedefVar, sargs);

    // connect their members
    transferTemplateMemberInfo(instLoc, srcCS, destCS, sargs);
  }

  else {
    // other kinds of type specifiers: don't need to do anything
  }
}


// transfer template information from primary 'srcVar' to
// instantiation 'destVar'
void Env::transferTemplateMemberInfo_one
  (SourceLoc instLoc, Variable *srcVar, Variable *destVar,
   ObjList<STemplateArgument> const &sargs)
{
  xassert(srcVar != destVar);

  // bit of a hack: if 'destVar' already has templateInfo, then it's
  // because it is a member template (or a member of a member
  // template), and we got here by recursively calling
  // 'transferTemplateMemberInfo'; call the member template handler
  // instead
  if (destVar->templateInfo()) {
    transferTemplateMemberInfo_membert(instLoc, srcVar, destVar, sargs);
    return;
  }

  TRACE("templateXfer", "associated primary " << srcVar->toQualifiedString()
                     << " with inst " << destVar->toQualifiedString());

  // make the TemplateInfo for this member instantiation
  TemplateInfo *destTI = new TemplateInfo(instLoc);

  // copy arguments into 'destTI'
  destTI->copyArguments(sargs);

  // attach 'destTI' to 'destVar'
  destVar->setTemplateInfo(destTI);

  // 'destVar' is an instantiation of 'srcVar' with 'sargs'
  TemplateInfo *srcTI = srcVar->templateInfo();
  xassert(srcTI);
  srcTI->addInstantiation(destVar);
}


// this is for member templates ("membert")
void Env::transferTemplateMemberInfo_membert
  (SourceLoc instLoc, Variable *srcVar, Variable *destVar,
   ObjList<STemplateArgument> const &sargs)
{
  // what follows is a modified version of 'transferTemplateMemberInfo_one'

  // 'destVar' is a partial instantiation of 'srcVar' with 'args'
  TemplateInfo *srcTI = srcVar->templateInfo();
  xassert(srcTI);
  TemplateInfo *destTI = destVar->templInfo;
  xassert(destTI);

  if (destTI->isInstantiation() || destTI->isPartialInstantiation()) {
    // The relevant info has already been transferred.  This happens
    // for example when an inner class is declared and then defined,
    // when we see it for the second time.
    return;
  }

  destTI->copyArguments(sargs);

  srcTI->addPartialInstantiation(destVar);

  // should not have already checked this member's body even if
  // it has an inline definition
  xassert(!destVar->funcDefn);

  // does the source have a definition?
  if (srcVar->funcDefn) {
    // give the definition to the dest too
    destVar->funcDefn = srcVar->funcDefn;

    // is it inline?
    if (srcVar->scope == srcTI->defnScope) {
      // then the dest's defnScope should be similarly arranged
      destTI->defnScope = destVar->scope;
    }
    else {
      // for out of line, the defn scopes of src and dest are the same
      destTI->defnScope = srcTI->defnScope;
    }
  }

  // do this last so I have full info to print for 'destVar'
  TRACE("templateXfer", "associated primary " << srcVar->toQualifiedString()
                     << " with partial inst " << destVar->toQualifiedString());
}


// given a name that was found without qualifiers or template arguments,
// see if we're currently inside the scope of a template definition
// with that name
CompoundType *Env::findEnclosingTemplateCalled(StringRef name)
{
  FOREACH_OBJLIST(Scope, scopes, iter) {
    Scope const *s = iter.data();

    if (s->curCompound &&
        s->curCompound->templateInfo() &&
        s->curCompound->name == name) {
      return s->curCompound;
    }
  }
  return NULL;     // not found
}


// --------------------- from cc_tcheck.cc ----------------------
// go over all of the function bodies and make sure they have been
// typechecked
class EnsureFuncBodiesTcheckedVisitor : public ASTTemplVisitor {
  Env &env;
public:
  EnsureFuncBodiesTcheckedVisitor(Env &env0) : env(env0) {}
  bool visitFunction(Function *f);
  bool visitDeclarator(Declarator *d);
};

bool EnsureFuncBodiesTcheckedVisitor::visitFunction(Function *f) {
  xassert(f->nameAndParams->var);
  env.ensureFuncBodyTChecked(f->nameAndParams->var);
  return true;
}

bool EnsureFuncBodiesTcheckedVisitor::visitDeclarator(Declarator *d) {
//    printf("EnsureFuncBodiesTcheckedVisitor::visitDeclarator d:%p\n", d);
  // a declarator can lack a var if it is just the declarator for the
  // ASTTypeId of a type, such as in "operator int()".
  if (!d->var) {
    // I just want to check something that will make sure that this
    // didn't happen because the enclosing function body never got
    // typechecked
    xassert(d->context == DC_UNKNOWN);
//      xassert(d->decl);
//      xassert(d->decl->isD_name());
//      xassert(!d->decl->asD_name()->name);
  } else {
    env.ensureFuncBodyTChecked(d->var);
  }
  return true;
}


void instantiateRemainingMethods(Env &env, TranslationUnit *tunit)
{
  // Given the current architecture, it is impossible to ensure that
  // all called functions have had their body tchecked.  This is
  // because if implicit calls to existing functions.  That is, one
  // user-written (not implicitly defined) function f() that is
  // tchecked implicitly calls another function g() that is
  // user-written, but the call is elaborated into existence.  This
  // means that we don't see the call to g() until the elaboration
  // stage, but by then typechecking is over.  However, since the
  // definition of g() is user-written, not implicitly defined, it
  // does indeed need to be tchecked.  Therefore, at the end of a
  // translation unit, I simply typecheck all of the function bodies
  // that have not yet been typechecked.  If this doesn't work then we
  // really need to change something non-trivial.
  //
  // It seems that we are ok for now because it is only function
  // members of templatized classes that we are delaying the
  // typechecking of.  Also, I expect that the definitions will all
  // have been seen.  Therefore, I can just typecheck all of the
  // function bodies at the end of typechecking the translation unit.
  //
  // sm: Only do this if tchecking doesn't have any errors yet,
  // b/c the assertions in the traversal will be potentially invalid
  // if errors were found
  //
  // 8/03/04: Um, why are we doing this at all?!  It defeats the
  // delayed instantiation mechanism altogether!  I'm disabling it.
  #if 0    // no!
  if (env.numErrors() == 0) {
    EnsureFuncBodiesTcheckedVisitor visitor(env);
    tunit->traverse(visitor);
  }
  #endif // 0
}


// we (may) have just encountered some syntax which declares
// some template parameters, but found that the declaration
// matches a prior declaration with (possibly) some other template
// parameters; verify that they match (or complain), and then
// discard the ones stored in the environment (if any)
//
// return false if there is some problem, true if it's all ok
// (however, this value is ignored at the moment)
bool Env::verifyCompatibleTemplateParameters(CompoundType *prior)
{
  Scope *scope = this->scope();
  if (!scope->hasTemplateParams() && !prior->isTemplate()) {
    // neither talks about templates, forget the whole thing
    return true;
  }

  // before going further, associate the scope's parameters
  // so that happens regardless of the decision here
  if (scope->hasTemplateParams()) {
    scope->setParameterizedEntity(prior->typedefVar);
  }

  if (!scope->hasTemplateParams() && prior->isTemplate()) {
    error(stringc
      << "prior declaration of " << prior->keywordAndName()
      << " at " << prior->typedefVar->loc
      << " was templatized with parameters "
      << prior->templateInfo()->paramsToCString()
      << " but the this one is not templatized",
      EF_DISAMBIGUATES);
    return false;
  }

  if (scope->hasTemplateParams() &&
      scope->templateParams.isNotEmpty() &&      // t0252.cc
      !prior->isTemplate()) {
    error(stringc
      << "prior declaration of " << prior->keywordAndName()
      << " at " << prior->typedefVar->loc
      << " was not templatized, but this one is, with parameters "
      << paramsToCString(scope->templateParams),
      EF_DISAMBIGUATES);
    return false;
  }

  // now we know both declarations have template parameters;
  // check them for naming equivalent types
  //
  // furthermore, fix the names in 'prior' in case they differ
  // with those of 'scope->curTemplateParams'
  //
  // even more, merge their default arguments
  bool ret = mergeParameterLists(
    prior->typedefVar,
    prior->templateInfo()->params,     // dest
    scope->templateParams);            // src

  return ret;
}


// context: I have previously seen a (forward) template
// declaration, such as
//   template <class S> class C;             // dest
//                   ^
// and I want to modify it to use the same names as another
// declaration later on, e.g.
//   template <class T> class C { ... };     // src
//                   ^
// since in fact I am about to discard the parameters that
// come from 'src' and simply keep using the ones from
// 'dest' for future operations, including processing the
// template definition associated with 'src'
bool Env::mergeParameterLists(Variable *prior,
                              SObjList<Variable> &destParams,
                              SObjList<Variable> const &srcParams)
{
  TRACE("templateParams", "mergeParameterLists: prior=" << prior->name
    << ", dest=" << paramsToCString(destParams)
    << ", src=" << paramsToCString(srcParams));

  // keep track of whether I've made any naming changes
  // (alpha conversions)
  bool anyNameChanges = false;

  SObjListMutator<Variable> destIter(destParams);
  SObjListIter<Variable> srcIter(srcParams);
  for (; !destIter.isDone() && !srcIter.isDone();
       destIter.adv(), srcIter.adv()) {
    Variable *dest = destIter.data();
    Variable const *src = srcIter.data();

    // are the types equivalent?
    if (!isomorphicTypes(dest->type, src->type)) {
      error(stringc
        << "prior declaration of " << prior->toString()
        << " at " << prior->loc
        << " was templatized with parameter `"
        << dest->name << "' of type `" << dest->type->toString()
        << "' but this one has parameter `"
        << src->name << "' of type `" << src->type->toString()
        << "', and these are not equivalent",
        EF_DISAMBIGUATES);
      return false;
    }

    // what's up with their default arguments?
    if (dest->value && src->value) {
      // this message could be expanded...
      error("cannot specify default value of template parameter more than once");
      return false;
    }

    // there is a subtle problem if the prior declaration has a
    // default value which refers to an earlier template parameter,
    // but the name of that parameter has been changed
    if (anyNameChanges &&              // earlier param changed
        dest->value) {                 // prior has a value
      // leave it as a to-do for now; a proper implementation should
      // remember the name substitutions done so far, and apply them
      // inside the expression for 'dest->value'
      xfailure("unimplemented: alpha conversion inside default values"
               " (workaround: use consistent names in template parameter lists)");
    }

    // merge their default values
    if (src->value && !dest->value) {
      dest->value = src->value;
    }

    // do they use the same name?
    if (dest->name != src->name) {
      // make the names the same
      TRACE("templateParams", "changing parameter " << dest->name
        << " to " << src->name);
      anyNameChanges = true;

      // Make a new Variable to hold the modified parameter.  I'm not
      // sure this is the right thing to do, b/c I'm concerned about
      // the fact that the original decl will be pointing at the old
      // Variable, but if I modify it in place I've got the problem
      // that the template params for a class are shared by all its
      // members, so I'd be changing all the members' params too.
      // Perhaps it's ok to make a few copies of template parameter
      // Variables, as they are somehow less concrete than the other
      // named entities...
      Variable *v = makeVariable(dest->loc, src->name, src->type, dest->flags);
      
      // copy a few other fields, including default value
      v->value = dest->value;
      v->defaultParamType = dest->defaultParamType;
      v->scope = dest->scope;
      v->scopeKind = dest->scopeKind;

      // replace the old with the new
      destIter.dataRef() = v;
    }
  }

  if (srcIter.isDone() && destIter.isDone()) {
    return true;   // ok
  }
  else {
    error(stringc
      << "prior declaration of " << prior->toString()
      << " at " << prior->loc
      << " was templatized with "
      << pluraln(destParams.count(), "parameter")
      << ", but this one has "
      << pluraln(srcParams.count(), "parameter"),
      EF_DISAMBIGUATES);
    return false;
  }
}


bool Env::mergeTemplateInfos(Variable *prior, TemplateInfo *dest,
                             TemplateInfo const *src)
{
  bool ok = mergeParameterLists(prior, dest->params, src->params);

  // sync up the inherited parameters too
  ObjListIterNC<InheritedTemplateParams> destIter(dest->inheritedParams);
  ObjListIter<InheritedTemplateParams> srcIter(src->inheritedParams);

  for (; !destIter.isDone() && !srcIter.isDone();
         destIter.adv(), srcIter.adv()) {
    if (!mergeParameterLists(prior, destIter.data()->params, srcIter.data()->params)) {
      ok = false;
    }
  }

  if (!destIter.isDone() || !srcIter.isDone()) {
    // TODO: expand this error message
    error("differing number of template parameter lists");
    ok = false;
  }
  
  return ok;
}


bool Env::isomorphicTypes(Type *a, Type *b)
{
  // 10/09/04: Why does this exist and also 'equalOrIsomorphic' exist?
  // I will try making this one call that one.
  return equalOrIsomorphic(tfac, a, b);

  #if 0
  MatchTypes match(tfac, MatchTypes::MM_ISO);
  return match.match_Type(a, b);
  #endif // 0
}


Type *Env::applyArgumentMapToType(STemplateArgumentCMap &map, Type *origSrc)
{
  // my intent is to not modify 'origSrc', so I will use 'src', except
  // when I decide to return what I already have, in which case I will
  // use 'origSrc'
  Type const *src = origSrc;

  switch (src->getTag()) {
    default: xfailure("bad tag");

    case Type::T_ATOMIC: {
      CVAtomicType const *scat = src->asCVAtomicTypeC();
      Type *ret = applyArgumentMapToAtomicType(map, scat->atomic, scat->cv);
      if (!ret) {
        return origSrc;      // use original
      }
      else {
        return ret;
      }
    }

    case Type::T_POINTER: {
      PointerType const *spt = src->asPointerTypeC();
      return tfac.makePointerType(SL_UNKNOWN, spt->cv,
        applyArgumentMapToType(map, spt->atType));
    }

    case Type::T_REFERENCE: {
      ReferenceType const *srt = src->asReferenceTypeC();
      return tfac.makeReferenceType(SL_UNKNOWN,
        applyArgumentMapToType(map, srt->atType));
    }

    case Type::T_FUNCTION: {
      FunctionType const *sft = src->asFunctionTypeC();
      FunctionType *rft = tfac.makeFunctionType(SL_UNKNOWN,
        applyArgumentMapToType(map, sft->retType));

      // copy parameters
      SFOREACH_OBJLIST(Variable, sft->params, iter) {
        Variable const *sp = iter.data();
        Variable *rp = makeVariable(sp->loc, sp->name,
          applyArgumentMapToType(map, sp->type), sp->flags);
          
        if (sp->value) {
          // TODO: I should be substituting the template parameters
          // in the default argument too... but for now I will just
          // use it without modification
          rp->value = sp->value;
        }

        rft->addParam(rp);
      }
      doneParams(rft);

      rft->flags = sft->flags;

      if (rft->exnSpec) {
        // TODO: it's not all that hard, I just want to skip it for now
        xfailure("unimplemented: exception spec on template function where "
                 "defn has differing parameters than declaration");
      }

      return rft;
    }

    case Type::T_ARRAY: {
      ArrayType const *sat = src->asArrayTypeC();
      return tfac.makeArrayType(SL_UNKNOWN,
        applyArgumentMapToType(map, sat->eltType), sat->size);
    }

    case Type::T_POINTERTOMEMBER: {
      PointerToMemberType const *spmt = src->asPointerToMemberTypeC();
      
      // slightly tricky mapping the 'inClassNAT' since we need to make
      // sure the mapped version is still a NamedAtomicType
      Type *retInClassNAT =
        applyArgumentMapToAtomicType(map, spmt->inClassNAT, CV_NONE);
      if (!retInClassNAT) {
        // use original 'spmt->inClassNAT'
        return tfac.makePointerToMemberType(SL_UNKNOWN,
          spmt->inClassNAT,
          spmt->cv,
          applyArgumentMapToType(map, spmt->atType));
      }
      else if (!retInClassNAT->isNamedAtomicType()) {
        return error(stringc << "during template instantiation: type `" <<
                                retInClassNAT->toString() <<
                                "' is not suitable as the class in a pointer-to-member");
      }
      else {
        return tfac.makePointerToMemberType(SL_UNKNOWN,
          retInClassNAT->asNamedAtomicType(),
          spmt->cv,
          applyArgumentMapToType(map, spmt->atType));
      }
    }
  }
}

Type *Env::applyArgumentMapToAtomicType
  (STemplateArgumentCMap &map, AtomicType *origSrc, CVFlags srcCV)
{
  AtomicType const *src = origSrc;

  if (src->isTypeVariable()) {
    TypeVariable const *stv = src->asTypeVariableC();

    STemplateArgument const *replacement = map.get(stv->name);
    if (!replacement) {
      // TODO: I think these should be user errors ...
      xfailure(stringc << "applyArgumentMapToAtomicType: the type name `"
                       << stv->name << "' is not bound");
    }
    else if (!replacement->isType()) {
      xfailure(stringc << "applyArgumentMapToAtomicType: the type name `"
                       << stv->name << "' is bound to a non-type argument");
    }

    // take what we got and apply the cv-flags that were associated
    // with the type variable, e.g. "T const" -> "int const"
    Type *ret = tfac.applyCVToType(SL_UNKNOWN, srcCV,
                                   replacement->getType(), NULL /*syntax*/);
    if (!ret) {
      return error(stringc << "during template instantiation: type `" <<
                              replacement->getType() << "' cannot be cv-qualified");
    }
    else {
      return ret;     // good to go
    }
  }

  else if (src->isPseudoInstantiation()) {
    PseudoInstantiation const *spi = src->asPseudoInstantiationC();

    // build a concrete argument list, so we can make a real instantiation
    ObjList<STemplateArgument> args;
    FOREACH_OBJLIST(STemplateArgument, spi->args, iter) {
      STemplateArgument const *sta = iter.data();
      if (sta->isType()) {
        STemplateArgument *rta = new STemplateArgument;
        rta->setType(applyArgumentMapToType(map, sta->getType()));
        args.append(rta);
      }
      else {
        // handle the one case that sort of works; STA_REFERENCE is (for
        // the moment; this is a bug) used as a catch-call
        if (sta->kind == STemplateArgument::STA_REFERENCE) {
          StringRef varName = sta->value.v->name;
          STemplateArgument const *replacement = map.get(varName);
          if (!replacement) {
            // TODO: I think these should be user errors ...
            //
            // TODO: it is possible this really is a concrete
            // reference argument, instead of being an abstract
            // parameter, but we have no way of telling the difference
            // in the current (broken) design
            xfailure(stringc << "applyArgumentMapToAtomicType: the non-type name `"
                             << varName << "' is not bound");
          }
          else if (replacement->isType()) {
            xfailure(stringc << "applyArgumentMapToAtomicType: the non-type name `"
                             << varName << "' is bound to a type argument");
          }

          args.append(replacement->shallowClone());
        }
        else {
          // the argument is already concrete
          args.append(sta->shallowClone());
        }
      }
    }

    // instantiate the class with our newly-created arguments
    Variable *instClass = 
      instantiateClassTemplate(loc(), spi->primary->typedefVar, args);
    if (!instClass) {
      instClass = errorVar;    // error already reported; this is recovery
    }

    // apply the cv-flags and return it
    return tfac.applyCVToType(SL_UNKNOWN, srcCV,
                              instClass->type, NULL /*syntax*/);
  }

  else {
    // all others do not refer to type variables; returning NULL
    // here means to use the original unchanged
    return NULL;
  }
}


Variable *Env::findCompleteSpecialization(TemplateInfo *tinfo,
                                          SObjList<STemplateArgument> const &sargs)
{
  SFOREACH_OBJLIST_NC(Variable, tinfo->specializations, iter) {
    TemplateInfo *instTI = iter.data()->templateInfo();
    if (instTI->equalArguments(tfac /*why needed?*/, sargs)) {
      return iter.data();      // found it
    }
  }
  return NULL;                 // not found
}


Variable *Env::findInstantiation(TemplateInfo *tinfo,
                                 SObjList<STemplateArgument> const &sargs)
{
  if (tinfo->isCompleteSpec()) {
    xassertdb(tinfo->equalArguments(tfac /*?*/, sargs));
    return tinfo->var;
  }

  SFOREACH_OBJLIST_NC(Variable, tinfo->instantiations, iter) {
    TemplateInfo *instTI = iter.data()->templateInfo();
    if (instTI->equalArguments(tfac /*why needed?*/, sargs)) {
      return iter.data();      // found it
    }
  }
  return NULL;                 // not found
}


// make a Variable with type 'type' that will be an instantiation
// of 'templ'
Variable *Env::makeInstantiationVariable(Variable *templ, Type *instType)
{
  Variable *inst = makeVariable(templ->loc, templ->name, instType, templ->flags);
  inst->access = templ->access;
  inst->scope = templ->scope;
  inst->scopeKind = templ->scopeKind;
  return inst;
}


void Env::bindParametersInMap(STemplateArgumentCMap &map, TemplateInfo *tinfo,
                              SObjList<STemplateArgument> const &sargs)
{
  SObjListIter<STemplateArgument> argIter(sargs);

  // inherited parameters
  FOREACH_OBJLIST(InheritedTemplateParams, tinfo->inheritedParams, iter) {
    bindParametersInMap(map, iter.data()->params, argIter);
  }

  // main parameters
  bindParametersInMap(map, tinfo->params, argIter);
  
  if (!argIter.isDone()) {
    error(stringc << "too many template arguments supplied for "
                  << tinfo->var->name);
  }
}

void Env::bindParametersInMap(STemplateArgumentCMap &map,
                              SObjList<Variable> const &params,
                              SObjListIter<STemplateArgument> &argIter)
{
  SFOREACH_OBJLIST(Variable, params, iter) {
    Variable const *param = iter.data();

    if (map.get(param->name)) {
      error(stringc << "template parameter `" << param->name <<
                       "' occurs more than once");
    }
    else if (argIter.isDone()) {
      error(stringc << "no template argument supplied for parameter `" <<
                       param->name << "'");
    }
    else {
      map.add(param->name, argIter.data());
    }

    if (!argIter.isDone()) {
      argIter.adv();
    }
  }
}


// given a CompoundType that is a template (primary or partial spec),
// yield a PseudoInstantiation of that template with its own params
Type *Env::pseudoSelfInstantiation(CompoundType *ct, CVFlags cv)
{
  TemplateInfo *tinfo = ct->typedefVar->templateInfo();
  xassert(tinfo);     // otherwise why are we here?

  PseudoInstantiation *pi = new PseudoInstantiation(
    tinfo->getPrimary()->var->type->asCompoundType());   // awkward...

  if (tinfo->isPrimary()) {
    // 14.6.1 para 1

    // I'm guessing we just use the main params, and not any
    // inherited params?
    SFOREACH_OBJLIST_NC(Variable, tinfo->params, iter) {
      Variable *param = iter.data();

      // build a template argument that just refers to the template
      // parameter
      STemplateArgument *sta = new STemplateArgument;
      if (param->type->isTypeVariable()) {
        sta->setType(param->type);
      }
      else {
        // TODO: this is wrong, like it is basically everywhere else
        // we use STA_REFERENCE ...
        sta->setReference(param);
      }

      pi->args.append(sta);
    }
  }

  else /*partial spec*/ {
    // 14.6.1 para 2
    xassert(tinfo->isPartialSpec());

    // use the specialization arguments
    copyTemplateArgs(pi->args, objToSObjListC(tinfo->arguments));
  }

  return makeCVAtomicType(ct->typedefVar->loc, pi, cv);
}


Variable *Env::makeExplicitFunctionSpecialization
  (SourceLoc loc, DeclFlags dflags, PQName *name, FunctionType *ft)
{
  // find the overload set
  Variable *ovlVar = lookupPQVariable(name, LF_TEMPL_PRIMARY);
  if (!ovlVar) {
    error(stringc << "cannot find primary `" << name->toString() 
                  << "' to specialize");
    return NULL;
  }

  // find last component of 'name' so we can see if template arguments
  // are explicitly supplied
  PQ_template *pqtemplate = NULL;
  SObjList<STemplateArgument> nameArgs;
  if (name->getUnqualifiedName()->isPQ_template()) {
    pqtemplate = name->getUnqualifiedName()->asPQ_template();
    if (!templArgsASTtoSTA(pqtemplate->args, nameArgs)) {
      return NULL;       // error already reported
    }
  }

  // get set of overloaded names (might be singleton)
  SObjList<Variable> set;
  ovlVar->getOverloadList(set);

  // look for a template member of the overload set that can
  // specialize to the type 'ft' and whose resulting parameter
  // bindings are 'sargs' (if supplied)
  Variable *best = NULL;     
  Variable *ret = NULL;
  SFOREACH_OBJLIST_NC(Variable, set, iter) {
    Variable *primary = iter.data();
    if (!primary->isTemplate()) {
      continue_outer_loop:     // poor man's labeled continue ...
      continue;
    }

    // can this element specialize to 'ft'?
    MatchTypes match(tfac, MatchTypes::MM_BIND);
    if (match.match_Type(ft, primary->type)) {
      // yes; construct the argument list that specializes 'primary'
      TemplateInfo *primaryTI = primary->templateInfo();
      SObjList<STemplateArgument> specArgs;

      if (primaryTI->inheritedParams.isNotEmpty()) {
        // two difficulties:
        //   - both the primary and specialization types might refer
        //     to inherited type varibles, but MM_BIND doesn't want
        //     type variables occurring on both sides
        //   - I want to compute right now the argument list that
        //     specializes primary, but then later I will want the
        //     full argument list for making the TemplateInfo
        xfailure("unimplemented: specializing a member template");
      }

      // simultanously iterate over the user's provided arguments,
      // if any, checking for correspondence with inferred arguments
      SObjListIter<STemplateArgument> nameArgsIter(nameArgs);

      // just use the main (not inherited) parameters...
      SFOREACH_OBJLIST_NC(Variable, primaryTI->params, paramIter) {
        Variable *param = paramIter.data();

        // get the binding
        STemplateArgument const *binding = match.bindings.getVar(param->name);
        if (!binding) {
          // inference didn't pin this down; did the user give me
          // arguments to use instead?
          if (!nameArgsIter.isDone()) {
            // yes, use the user's argument instead
            binding = nameArgsIter.data();
          }
          else {
            // no, so this candidate can't match
            goto continue_outer_loop;
          }
        }
        else {
          // does the inferred argument match what the user has?
          if (pqtemplate &&                               // user gave me arguments
              (nameArgsIter.isDone() ||                           // but not enough
               !binding->isomorphic(tfac, nameArgsIter.data()))) {   // or no match
            // no match, this candidate can't match
            goto continue_outer_loop;
          }
        }

        // remember the argument (I promise not to modify it...)
        specArgs.append(const_cast<STemplateArgument*>(binding));

        if (!nameArgsIter.isDone()) {
          nameArgsIter.adv();
        }
      }

      // does the inferred argument list match 'nameArgs'?  we already
      // checked individual elements above, now just confirm the count
      // is correct (in fact, only need to check there aren't too many)
      if (pqtemplate &&                  // user gave me arguments
          !nameArgsIter.isDone()) {        // but too many
        // no match, go on to the next primary
        continue;
      }

      // ok, found a suitable candidate
      if (best) {
        error(stringc << "ambiguous specialization, could specialize `"
                      << best->type->toString() << "' or `"
                      << primary->type->toString()
                      << "'; use explicit template arguments to disambiguate",
                      EF_STRONG);
        // error recovery: use 'best' anyway
        break;
      }
      best = primary;

      // do we already have a specialization like this?
      ret = primary->templateInfo()->getSpecialization(tfac, specArgs);
      if (ret) {
        TRACE("template", "re-declaration of function specialization of " <<
                          primary->type->toCString(primary->fullyQualifiedName()) <<
                          ": " << ret->name << sargsToString(specArgs));
      }
      else {
        // build a Variable to represent the specialization
        ret = makeSpecializationVariable(loc, dflags, primary, ft, specArgs);
        TRACE("template", "complete function specialization of " <<
                          primary->type->toCString(primary->fullyQualifiedName()) <<
                          ": " << ret->name << sargsToString(specArgs));
      }
    } // initial candidate match check
  } // candidate loop

  if (!ret) {
    error("specialization does not match any function template", EF_STRONG);
    return NULL;
  }

  return ret;
}


Variable *Env::makeSpecializationVariable
  (SourceLoc loc, DeclFlags dflags, Variable *templ, Type *type,
   SObjList<STemplateArgument> const &args)
{
  // make the Variable
  Variable *spec = makeVariable(loc, templ->name, type, dflags);
  spec->access = templ->access;
  spec->scope = templ->scope;
  spec->scopeKind = templ->scopeKind;

  // make & attach the TemplateInfo
  TemplateInfo *ti = new TemplateInfo(loc, spec);
  ti->copyArguments(args);

  // attach to the template
  templ->templateInfo()->addSpecialization(spec);

  // this is a specialization
  xassert(ti->isSpecialization());

  return spec;
}


void Env::explicitlyInstantiate(Variable *var)
{
  TemplateInfo *tinfo = var->templateInfo();
  xassert(tinfo);

  // 8/12/04: This code has not been tested very much ...

  // function instantiation?
  if (var->type->isFunctionType()) {
    // It's ok if we haven't seen the definition yet, however, the
    // presence of this explicit instantation request means that the
    // definition must be somewhere in the translation unit (14.7.2
    // para 4).  However, I do not enforce this.
    ensureFuncBodyTChecked(var);
    return;
  }

  // class instantiation
  xassert(var->type->isCompoundType());
  CompoundType *ct = var->type->asCompoundType();
  if (!ensureCompleteType("instantiate", var->type)) {
    return;    // recovery
  }

  // 14.7.2 para 7: instantiate all members, too

  // base classes
  FOREACH_OBJLIST(BaseClass, ct->bases, baseIter) {
    Variable *b = baseIter.data()->ct->typedefVar;
    if (b->isInstantiation()) {     // t0273.cc
      explicitlyInstantiate(b);
    }
  }

  // member variables, functions
  for (PtrMap<const char, Variable>::Iter membIter(ct->getVariableIter());
       !membIter.isDone(); membIter.adv()) {
    Variable *memb = membIter.value();

    if (memb->templateInfo()) {
      explicitlyInstantiate(memb);
    }
  }

  // inner classes
  for (PtrMap<const char, CompoundType>::Iter innerIter(ct->getCompoundIter());
       !innerIter.isDone(); innerIter.adv()) {
    CompoundType *inner = innerIter.value();

    explicitlyInstantiate(inner->typedefVar);
  }
}


// This is called in response to syntax like (t0256.cc)
//
//   template
//   int foo(int t);
//
// for which 'name' would be "foo" and 'type' would be "int ()(int)".
// This function then finds a function template called "foo" that
// matches the given type, and instantiates its body.  Finally, that
// instantiation is returned.
//
// On error, an error message is emitted and NULL is returned.
Variable *Env::explicitFunctionInstantiation(PQName *name, Type *type)
{
  if (!type->isFunctionType()) {
    error("explicit instantiation of non-function type");
    return NULL;
  }

  Variable *ovlHeader = lookupPQVariable(name, LF_TEMPL_PRIMARY);
  if (!ovlHeader || !ovlHeader->type->isFunctionType()) {
    error(stringc << "no such function `" << *name << "'");
    return NULL;
  }

  // did the user attach arguments to the name?
  ASTList<TemplateArgument> *nameArgs = NULL;
  if (name->getUnqualifiedName()->isPQ_template()) {
    nameArgs = &( name->getUnqualifiedName()->asPQ_template()->args );
  }

  // instantiation to eventually return
  Variable *ret = NULL;

  // examine all overloaded versions of the function
  SObjList<Variable> set;
  ovlHeader->getOverloadList(set);
  SFOREACH_OBJLIST_NC(Variable, set, iter) {
    Variable *primary = iter.data();
    if (!primary->isTemplate()) continue;
    TemplateInfo *primaryTI = primary->templateInfo();

    // does the type we have match the type of this template?
    MatchTypes match(tfac, MatchTypes::MM_BIND);
    if (!match.match_Type(type, primary->type)) {
      continue;   // no match
    }

    // use user's arguments (if any) to fill in missing bindings
    if (nameArgs) {
      if (!loadBindingsWithExplTemplArgs(primary, *nameArgs, match, 
                                         IA_NO_ERRORS)) {
        continue;      // no match
      }
    }

    // convert the bindings into a sequential argument list
    // (I am ignoring the inherited params b/c I'm not sure if it
    // is correct to use them here...)
    ObjList<STemplateArgument> sargs;
    bool haveAllArgs = true;
    getFuncTemplArgs_oneParamList(match, sargs, IA_NO_ERRORS,
                                  haveAllArgs, primaryTI->params);
    if (!haveAllArgs) {
      continue;   // no match
    }

    // at this point, the match is a success
    if (ret) {
      error("ambiguous function template instantiation");
      return ret;        // stop looking
    }

    // apply the arguments to the primary
    ret = instantiateFunctionTemplate(name->loc, primary, sargs);

    // instantiate the body
    explicitlyInstantiate(ret);
  }

  if (!ret) {
    error(stringc << "type `" << type->toString() 
                  << "' does not match any template function `" << *name << "'");
  }

  return ret;
}
  
     
// ------------------- InstantiationContextIsolator -----------------------
InstantiationContextIsolator::InstantiationContextIsolator(Env &e, SourceLoc loc)
  : env(e),
    origNestingLevel(e.disambiguationNestingLevel),
    origSecondPass(e.secondPassTcheck),
    origErrors()
{
  env.instantiationLocStack.push(loc);
  env.disambiguationNestingLevel = 0;
  env.secondPassTcheck = false;
  origErrors.takeMessages(env.errors);
}

InstantiationContextIsolator::~InstantiationContextIsolator()
{
  env.instantiationLocStack.pop();
  env.disambiguationNestingLevel = origNestingLevel;
  env.secondPassTcheck = origSecondPass;

  // where do the newly-added errors, i.e. those from instantiation,
  // which are sitting in 'env.errors', go?
  if (env.hiddenErrors) {
    // shuttle them around to the hidden message list
    env.hiddenErrors->takeMessages(env.errors);
  }
  else {
    // put them at the end of the original errors, as if we'd never
    // squirreled away any messages
    origErrors.takeMessages(env.errors);
  }
  xassert(env.errors.isEmpty());

  // now put originals back into env.errors
  env.errors.takeMessages(origErrors);
}


// EOF