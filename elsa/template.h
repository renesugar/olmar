// template.h
// data structures for representing templates, etc.
//
// Conceptually, everything here is an extension of cc_type.
//
// Note that there are many more template-related functions, declared
// in cc_env.h in a block at the end of the declaration of the Env
// class.  There isn't a nice way to separate those declarations out,
// since I want them to be members of Env.

#ifndef TEMPLATE_H
#define TEMPLATE_H

#include "cc_type.h"         // non-template parts of type system


// used for (abstract) template parameter types
class TypeVariable : public NamedAtomicType {
public:
  TypeVariable(StringRef name) : NamedAtomicType(name) {}
  ~TypeVariable();

  // AtomicType interface
  virtual Tag getTag() const { return T_TYPEVAR; }
  virtual string toCString() const;
  virtual string toMLString() const;
  virtual int reprSize() const;
};


// denote a class template with arguments supplied, so it's like an
// instantiation, but some of those arguments contain type variables,
// so this does not denote a single concrete type; this appears in
// template definitions only
//
// actually it might not contain type variables but instead only
// contain non-type argument variables; the point is we don't have
// enough information to do a concrete instantiation
class PseudoInstantiation : public NamedAtomicType {
public:      // data
  // class template primary to which we are adding arguments
  CompoundType *primary;

  // the arguments, some of which contain type variables
  ObjList<STemplateArgument> args;

public:      // funcs
  PseudoInstantiation(CompoundType *p);
  ~PseudoInstantiation();

  // AtomicType interface
  virtual Tag getTag() const { return T_PSEUDOINSTANTIATION; }
  virtual string toCString() const;
  virtual string toMLString() const;
  virtual int reprSize() const;
};


// just some template parameters (this class exists, in part, so
// that Scope doesn't have to instantiate a full TemplateInfo)
class TemplateParams {
public:    // data
  // template parameters: the dimensions along which the associated
  // entity may be specialized at compile time
  SObjList<Variable> params;

public:    // funcs
  TemplateParams() {}
  TemplateParams(TemplateParams const &obj);
  ~TemplateParams();
  
  // queries on parameters
  string paramsToCString() const;
  string paramsToMLString() const;
  bool anyParamCtorSatisfies(TypePred &pred) const;
  
  // print the parameters like arguments, e.g. "<S, T>"
  // instead of "template <class S, class T>"
  string paramsLikeArgsToString() const;
};

// make this available outside the class too
string paramsToCString(SObjList<Variable> const &params);


// template parameters on an enclosing object; for example, if "this"
// object is a member function enclosed in a template class, then
// this object will store (a copy of) the parameters for the class
// (this is needed, among other reasons, because when the member
// function is defined, the user is free to rename the parameters
// that applied to the class)
class InheritedTemplateParams : public TemplateParams {
public:      // data
  // We can only inherit params from a class; this is it.  If these
  // inherited parameters are attached to an instantiation, then
  // 'enclosing' is the instantiated enclosing class.
  CompoundType *enclosing;          // (serf)

public:      // funcs
  InheritedTemplateParams(CompoundType *e) : enclosing(e) {}
  InheritedTemplateParams(InheritedTemplateParams const &obj);
  ~InheritedTemplateParams();
};


// kind of template things (Variables with TemplateInfo)
enum TemplateThingKind {
  // template primary, the main template from which instantiations
  // are generated
  TTK_PRIMARY,
  
  // explicit specialization, a user-provided definition for a
  // specific set of arguments (or argument patterns)
  TTK_SPECIALIZATION,
  
  // instantiation, an object created by the compiler by taking
  // a primary or a partial specialization and filling in the
  // template parameters with concrete arguments
  TTK_INSTANTIATION,
  
  NUM_TTKINDS
};


// for a template function or class, including instantiations thereof,
// this is the information regarding its template-ness
class TemplateInfo : public TemplateParams {
public:    // data
  // This class maintains a number of bidirectional relationships.
  // To help ensure that both ends of the relation are maintained,
  // I mark some pointer fields 'const', to force updates to go
  // through dedicated routines.

  // every TemplateInfo is associated 1-to-1 with some Variable,
  // and this is the associated Variable; this value is initially
  // NULL, and set by Variable::setTemplateInfo
  //
  // TODO: make TemplateInfo inherit from Variable instead of
  // using two connected objects
  Variable * const var;                    // (serf)

  // inherited parameters, in order from outermost to innermost; this
  // TemplateThing is parameterized by the inherited parameters *and*
  // the main 'params' list
  ObjList<InheritedTemplateParams> inheritedParams;

  // the specialization / primary that we were instantiated from, if
  // we are an instantiation; NULL if we are not
  Variable * const instantiationOf;        // (serf)

  // inverse of 'instantiatedFrom'
  SObjList<Variable> instantiations;

  // the primary that this is a specialization of
  Variable * const specializationOf;       // (serf)

  // inverse of 'specializationOf'
  SObjList<Variable> specializations;

  // arguments to apply to my parent's parameters (inherited, then
  // main) to arrive at this object
  ObjList<STemplateArgument> arguments;

  // one of three conditions holds:
  //
  // TTKind              instantiatedFrom  specializationOf  arguments
  // -----------------------------------------------------------------
  // TTK_PRIMARY         NULL              NULL              empty
  // TTK_SPECIALIZATION  NULL              non-NULL          non-empty
  // TTK_INSTANTIATION   non-NULL          NULL              non-empty
  //
  // exception: partial instantiations are TTK_PRIMARY with non-empty arguments

  // if this is an instantiation, this records the (most proximal)
  // source location that gave rise to the need to instantiate it;
  // if not, it's just the location of the declaration of the
  // template itself
  SourceLoc instLoc;

  // Bidirectional "partial instantiation" relation: a partial
  // instantiation is a template function that is in most respects a
  // template primary.  However, it does not have its own independent
  // definition, rather the definition is provided by
  // 'partialInstantiatedFrom'.  Moreover, a partial instantiation
  // carries 'arguments' that are to be applied to the template it it
  // is a partial instance of, with remaining arguments supplied by
  // the partial instance's own parameters.
  Variable * const partialInstantiationOf;      // (serf)
  SObjList<Variable> partialInstantiations;

  // bit of a hack: during type matching, I need the arguments that
  // apply to the primary (not to a partial spec); I can compute them,
  // but that requires an Env b/c it means constructing new types;
  // better would be to extend matchtypes so it would allow a binding
  // map for the LHS type, but until that is implemented I'll just
  // keep a copy of the args to the primary too (this is empty unless
  // this isInstOfPartialSpec())
  ObjList<STemplateArgument> argumentsToPrimary;

  // scope in which the definition appears, if this is a primary
  // or a specialization (NULL otherwise)
  Scope *defnScope;

  // template parameters for use when instantiating a definition,
  // if different from the declaration
  TemplateInfo *definitionTemplateInfo;         // (nullable owner)

  // true if we have seen syntax that demands an instantiation
  // of the body, not just the declaration
  bool instantiateBody;

private:     // funcs
  // can modify the 'const' fields, for updates
  void addToList(Variable *elt, SObjList<Variable> &children, 
                 Variable * const &parentPtr);

public:      // funcs
  // Q: can I make the 'var' argument mandatory?
  TemplateInfo(SourceLoc instLoc, Variable *var = NULL);
  TemplateInfo(TemplateInfo const &obj);
  ~TemplateInfo();

  // name of the template class or function
  //StringRef getBaseName() const;    // who calls this?

  // what kind of template thing is this?
  TemplateThingKind getKind() const;
  bool isPrimary() const { return getKind() == TTK_PRIMARY; }
  bool isSpecialization() const { return getKind() == TTK_SPECIALIZATION; }
  bool isInstantiation() const { return getKind() == TTK_INSTANTIATION; }

  // some more queries along these lines
  bool isNotPrimary() const { return !isPrimary(); }
  bool isPartialSpec() const;
  bool isCompleteSpec() const;
  bool isCompleteSpecOrInstantiation() const;
  bool isPartialInstantiation() const { return !!partialInstantiationOf; }
  bool isInstOfPartialSpec() const;

  // return the primary for this template thing
  TemplateInfo const *getPrimaryC() const;
  TemplateInfo *getPrimary() { return const_cast<TemplateInfo*>(getPrimaryC()); }

  // modify one of the bidirectional relations; this is always
  // done by asking the parent to add a child
  void addInstantiation(Variable *inst);
  void addSpecialization(Variable *spec);
  void addPartialInstantiation(Variable *pinst);

  // return the arguments that get to this instantiation from the
  // primary; this is different from 'args' if this is an
  // instantiation of a partial specialization
  ObjList<STemplateArgument> &getArgumentsToPrimary();


  // true if 'list' contains equivalent semantic arguments
  bool equalArguments(TypeFactory &tfac, SObjList<STemplateArgument> const &list) const;

  // true if the arguments contain type variables
  //
  // TODO: remove this, and only have 'argumentsContainVariables'
  bool argumentsContainTypeVariables() const;

  // dsw: check the arguments contain type or object (say, int)
  // variables; FIX: I'm not sure this is implemented right; see
  // comments in implementation
  bool argumentsContainVariables() const;

  // true if there are parameters (at this level; not inherited)
  bool hasParameters() const;
                                            
  // inherited or main
  bool hasMainOrInheritedParameters() const;

  // true if the given Variable is among the parameters (at any level)
  //
  // TODO: what is this used for?
  bool hasSpecificParameter(Variable const *v) const;

  // copy 'sargs' into 'arguments'
  void copyArguments(ObjList<STemplateArgument> const &sargs);
  void copyArguments(SObjList<STemplateArgument> const &sargs);

  // debugging/error messages: print the fully qualified name,
  // plus arguments/parameters, to identify this template thing
  string templateName() const;

  // debugging
  void gdb();
  void debugPrint(int depth = 0, bool printPartialInsts = true);
};


// semantic template argument (semantic as opposed to syntactic); this
// breaks the argument down into the cases described in cppstd 14.3.2
// para 1, plus types, minus template parameters, then grouped into
// equivalence classes as implied by cppstd 14.4 para 1
class STemplateArgument {
public:
  // FIX: make these data members private
  enum Kind {
    STA_NONE,        // not yet resolved into a valid template argument
    STA_TYPE,        // type argument
    STA_INT,         // int or enum argument

    // dsw: this may not be a ref to a global object, but instead a
    // template parameter; in this example from in/t0180.cc, note the
    // use of 'C' here is an argument, where it is brought into
    // existence as a template parameter in the outer scope:
    //   template <typename A, typename B, int C>
    //   struct Traits<A, Helper1<B, Helper2<C> > >
    //
    // sm: STA_REFERENCE is being abused here; note how it would not
    // work if the code said "C+2".  We need something like STA_EXPR
    // that means "integer argument, but not evaluatable to a constant
    // in this context".
    STA_REFERENCE,   // reference to global object

    STA_POINTER,     // pointer to global object
    STA_MEMBER,      // pointer to class member
    STA_TEMPLATE,    // template argument (not implemented)
    NUM_STA_KINDS
  } kind;

  union {
    Type *t;         // (serf) for STA_TYPE
    int i;           // for STA_INT
    Variable *v;     // (serf) for STA_REFERENCE, STA_POINTER, STA_MEMBER
  } value;

public:
  STemplateArgument() : kind(STA_NONE) { value.i = 0; }
  STemplateArgument(Type *t) : kind(STA_TYPE) { value.t = t; }
  STemplateArgument(STemplateArgument const &obj);

  // 'new' + copy ctor
  STemplateArgument *shallowClone() const;

  // get 'value', ensuring correspondence between it and 'kind'
  Type *    getType()      const { xassert(kind==STA_TYPE);      return value.t; }
  int       getInt()       const { xassert(kind==STA_INT);       return value.i; }
  Variable *getReference() const { xassert(kind==STA_REFERENCE); return value.v; }
  Variable *getPointer()   const { xassert(kind==STA_POINTER);   return value.v; }
  Variable *getMember()    const { xassert(kind==STA_MEMBER);    return value.v; }

  // set 'value', ensuring correspondence between it and 'kind'
  void setType(Type *t)          { kind=STA_TYPE;      value.t=t; }
  void setInt(int i)             { kind=STA_INT;       value.i=i; }
  void setReference(Variable *v) { kind=STA_REFERENCE; value.v=v; }
  void setPointer(Variable *v)   { kind=STA_POINTER;   value.v=v; }
  void setMember(Variable *v)    { kind=STA_MEMBER;    value.v=v; }

  bool isObject() const;         // "non-type non-template" in the spec
  bool isType() const            { return kind==STA_TYPE;         }
  bool isTemplate() const        { return kind==STA_TEMPLATE;     }

  bool hasValue() const { return kind!=STA_NONE; }

  // true if it's '<dependent>'
  bool isDependent() const;

  // the point of boiling down the syntactic arguments into this
  // simpler semantic form is to make equality checking easy
  bool equals(STemplateArgument const *obj) const;

  // does it contain variables?
  bool containsVariables() const;

  // if it does contain variables, then 'equals' is inappropriate;
  // isomorphism is the right thing to check
  bool isomorphic(TypeFactory &tfac, STemplateArgument const *obj) const;

  // debug print
  string toString() const;

  // debugging
  void gdb();
  void debugPrint(int depth = 0);
};

SObjList<STemplateArgument> *cloneSArgs(SObjList<STemplateArgument> &sargs);
string sargsToString(SObjList<STemplateArgument> const &list);
inline string sargsToString(ObjList<STemplateArgument> const &list)
  { return sargsToString((SObjList<STemplateArgument> const &)list); }

bool containsTypeVariables(SObjList<STemplateArgument> const &args);
bool hasDependentArgs(SObjList<STemplateArgument> const &args);


// holder for the CompoundType template candidates
class TemplCandidates {
private:     // types
  // for comparing two STemplateArgument-s; There are four possible
  // answers: leftGreater, rightGreater, equal, and incomparable.
  enum STemplateArgsCmp {
    STAC_LEFT_MORE_SPEC,
    STAC_RIGHT_MORE_SPEC,
    STAC_EQUAL,
    STAC_INCOMPARABLE,
  };

public:      // data
  // needed to facilitate calls into matchtype ...
  TypeFactory &tfac;

  // the set of candidates
  ArrayStack<Variable*> candidates;

private:     // funcs
  // disallowed
  TemplCandidates(TemplCandidates const &);

  // compare two arguments
  static STemplateArgsCmp compareSTemplateArgs
    (TypeFactory &tfac, STemplateArgument const *larg, STemplateArgument const *rarg);

public:      // funcs
  TemplCandidates(TypeFactory &t) : tfac(t) {}

  // add a candidate
  void add(Variable *v) { candidates.push(v); }

  // compare two different templates (primary / specialization /
  // instantiation) to see which is more specific; used by
  // instantiateTemplate() to decide which to use for a given
  // instantiation request
  // return:
  //   -1 if left is better (more specific)
  //    0 if they are incomparable
  //   +1 if right is better
  int compareCandidates(Variable const *left, Variable const *right);

  // static version
  static int compareCandidatesStatic
    (TypeFactory &tfac, TemplateInfo const *lti, TemplateInfo const *rti);
};


#endif // TEMPLATE_H
