// cil.cc
// code for cil.h

// TODO9: virtually all of this code could have been
// automatically generated from cil.h .. so, write a
// tool to do that (or a language extension, or ..)

#include "cil.h"        // this module
#include "cc_type.h"    // Type, etc.
#include "cc_env.h"     // Env
#include "macros.h"     // STATICASSERT
#include "cc_tree.h"    // CCTreeNode
#include "fileloc.h"    // SourceLocation
#include "mlvalue.h"    // ml string support stuff
#include "cilxform.h"   // CilXform

#include <stdlib.h>     // rand
#include <string.h>     // memset


// ------------- names --------------
LabelName newTmpLabel()
{
  // TODO2: fix this sorry hack
  return stringc << "tmplabel" << (rand() % 1000);
}


// ------------ CilThing --------------
CilThing::CilThing(CilExtraInfo n)
{
  _loc = n;   //->loc();
}

string CilThing::locString() const
{
  SourceLocation const *l = loc();
  if (l) {
    return l->toString();
  }
  else {
    return "(?loc)";
  }
}

string CilThing::locComment() const
{
  if (!tracingSys("omit-loc")) {
    return stringc << "                  // " << locString() << "\n";
  }
  else {
    return "\n";
  }
}

SourceLocation const *CilThing::loc() const
{
  return _loc;
}


MLValue unknownMLLoc()
{
  if (tracingSys("omit-loc")) {
    return "{}";     // they really clutter the output
  }
  else {
    return mlRecord3("line", mlInt(0),
                     "col", mlInt(0),
                     "file", mlString("?"));
  }
}

MLValue CilThing::locMLValue() const
{
  if (tracingSys("omit-loc")) {
    return "{}";     // they really clutter the output
  }
  else {
    SourceLocation const *l = loc();
    if (l) {
      return mlRecord3("line", mlInt(l->line),
                       "col", mlInt(l->col),
                       "file", mlString(l->fname()));
    }
    else {
      return unknownMLLoc();
    }
  }
}


// ------------ operators -------------
BinOpInfo binOpArray[] = {
  // text, mlText,    mlTag
  {  "+",  "Plus",    0      },
  {  "-",  "Minus",   1      },
  {  "*",  "Mult",    2      },
  {  "/",  "Div",     3      },
  {  "%",  "Mod",     4      },
  {  "<<", "Shiftlt", 5      },
  {  ">>", "Shiftrt", 6      },
  {  "<",  "Lt",      7      },
  {  ">",  "Gt",      8      },
  {  "<=", "Le",      9      },
  {  ">=", "Ge",      10     },
  {  "==", "Eq",      11     },
  {  "!=", "Ne",      12     },
  {  "&",  "BAnd",    13     },
  {  "^",  "BXor",    14     },
  {  "|",  "BOr",     15     },
};

BinOpInfo const &binOp(BinOp op)
{
  STATIC_ASSERT(TABLESIZE(binOpArray) == NUM_BINOPS);

  validate(op);
  return binOpArray[op];
}

void validate(BinOp op)
{
  xassert(0 <= op && op < NUM_BINOPS);
}

MLValue binOpMLText(BinOp op)
{
  return stringc << binOp(op).mlText
                 << "(" << binOp(op).mlTag << ")";
}


UnaryOpInfo unaryOpArray[] = {
  // text, mlText, mlTag
  {  "-",  "Neg",  0       },
  {  "!",  "LNot", 1       },
  {  "~",  "BNot", 2       },
};

UnaryOpInfo const &unOp(UnaryOp op)
{
  STATIC_ASSERT(TABLESIZE(unaryOpArray) == NUM_UNOPS);

  validate(op);
  return unaryOpArray[op];
}

void validate(UnaryOp op)
{
  xassert(0 <= op && op < NUM_UNOPS);
}

MLValue unOpMLText(UnaryOp op)
{
  return stringc << unOp(op).mlText
                 << "(" << unOp(op).mlTag << ")";
}               


// -------------- CilExpr ------------
int CilExpr::numAllocd = 0;
int CilExpr::maxAllocd = 0;

CilExpr::CilExpr(CilExtraInfo tn, ETag t)
  : CilThing(tn),
    etag(t)
{
  INC_HIGH_WATER(numAllocd, maxAllocd);

  validate(etag);

  // for definiteness, blank the union fields
  memset(&binop, 0, sizeof(binop));    // largest field
}


CilExpr::~CilExpr()
{
  switch (etag) {
    case T_LVAL:
      delete lval;
      break;

    case T_UNOP:
      delete unop.exp;
      break;

    case T_BINOP:
      delete binop.left;
      delete binop.right;
      break;

    case T_CASTE:
      delete caste.exp;
      break;

    case T_ADDROF:
      delete addrof.lval;
      break;

    INCL_SWITCH
  }

  numAllocd--;
}


Type const *CilExpr::getType(Env *env) const
{
  // little hack: let the NULL get all the way here before
  // dealing with it
  if (this == NULL) {
    return env->getSimpleType(ST_VOID);
  }

  switch (etag) {
    default: xfailure("bad tag");
    case T_LITERAL:    return env->getSimpleType(ST_INT);
    case T_LVAL:       return lval->getType(env);
    case T_UNOP:       return unop.exp->getType(env);
    case T_BINOP:      return binop.left->getType(env);    // TODO: check that arguments have related types??
    case T_CASTE:      return caste.type;                  // TODO: check castability?
    case T_ADDROF:
      return env->makePtrOperType(PO_POINTER, CV_NONE,
                                  addrof.lval->getType(env));
  }
}


STATICDEF void CilExpr::validate(ETag tag)
{
  xassert(0 <= tag && tag < NUM_ETAGS);
}


STATICDEF void CilExpr::printAllocStats(bool anyway)
{
  if (anyway || numAllocd != 0) {
    cout << "cil expr nodes: " << numAllocd
         << ", max  nodes: " << maxAllocd
         << endl;
  }
}


CilExpr *CilExpr::clone() const
{
  switch (etag) {
    default: xfailure("bad tag");

    case T_LITERAL:
      return newIntLit(extra(), lit.value);

    case T_LVAL:
      return newLvalExpr(extra(), lval->clone());

    case T_UNOP:
      return newUnaryExpr(extra(), unop.op, unop.exp->clone());

    case T_BINOP:
      return newBinExpr(extra(),
                        binop.op,
                        binop.left->clone(),
                        binop.right->clone());

    case T_CASTE:
      return newCastExpr(extra(), caste.type, caste.exp->clone());

    case T_ADDROF:
      return newAddrOfExpr(extra(), addrof.lval->clone());
  }
}


// goal is a syntax that is easy to read (i.e. uses
// infix binary operators) but also easy to parse (so
// it heavily uses parentheses -- no prec/assoc issues)
string CilExpr::toString() const
{
  switch (etag) {
    default: xfailure("bad tag");
    case T_LITERAL:   return stringc << (unsigned)lit.value;
    case T_LVAL:      return lval->toString();
    case T_UNOP:
      return stringc << "(" << unOpText(unop.op) << " "
                     << unop.exp->toString() << ")";
    case T_BINOP:
      return stringc << "(" << binop.left->toString() << " "
                     << binOpText(binop.op) << " "
                     << binop.right->toString() << ")";
    case T_CASTE:
      return stringc << "([" << caste.type->toString(0)
                     << "] " << caste.exp->toString() << ")";
    case T_ADDROF:
      return stringc << "(& " << addrof.lval->toString() << ")";
  }
}


#define MKTAG(n,t) MAKE_ML_TAG(exp, n, t)
MKTAG(0, Const)
MKTAG(1, Lval)
MKTAG(2, SizeOf)
MKTAG(3, UnOp)
MKTAG(4, BinOp)
MKTAG(5, CastE)
MKTAG(6, AddrOf)
#undef MKTAG


// goal here is a syntax that corresponds to the structure
// ML uses internally, so we can perhaps write a general-purpose
// ascii-to-ML parser (or perhaps one exists?)
MLValue CilExpr::toMLValue() const
{
  switch (etag) {
    default: xfailure("bad tag");
    case T_LITERAL:   
      return stringc << "(" << exp_Const << " "
                     <<   "(Int " << mlInt(lit.value) << ") "
                     <<   locMLValue()
                     << ")";

    case T_LVAL:
      return stringc << "(" << exp_Lval << " "
                     <<   lval->toMLValue()
                     << ")";

    case T_UNOP:
      return stringc << "(" << exp_UnOp << " "
                     <<   unOpMLText(unop.op) << " "
                     <<   unop.exp->toMLValue() << " "
                     <<   locMLValue()
                     << ")";

    case T_BINOP:
      return stringc << "(" << exp_BinOp << " "
                     <<   binOpMLText(binop.op) << " "
                     <<   binop.left->toMLValue() << " "
                     <<   binop.right->toMLValue() << " "
                     <<   locMLValue()
                     << ")";

    case T_CASTE:
      return stringc << "(" << exp_CastE << " "
                     <<   caste.type->toMLValue() << " "
                     <<   caste.exp->toMLValue() << " "
                     <<   locMLValue()
                     << ")";

    case T_ADDROF:
      return stringc << "(" << exp_AddrOf << " "
                     <<   addrof.lval->toMLValue() << " "
                     <<   locMLValue()
                     << ")";
  }
}


void CilExpr::xform(CilXform &x)
{
  switch (etag) {
    default: xfailure("bad tag");

    case T_LITERAL:
      x.callTransformInt(lit.value);
      break;

    case T_LVAL:
      x.callTransformLval(lval);
      break;

    case T_UNOP:
      x.callTransformUnaryOp(unop.op);
      x.callTransformExpr(unop.exp);
      break;

    case T_BINOP:
      x.callTransformBinOp(binop.op);
      x.callTransformExpr(binop.left);
      x.callTransformExpr(binop.right);
      break;

    case T_CASTE:
      x.callTransformType(caste.type);
      x.callTransformExpr(caste.exp);
      break;
      
    case T_ADDROF:
      x.callTransformLval(addrof.lval);
      break;
  }
}


CilLval /*owner*/ *getLval(CilExpr /*owner*/ *expr)
{
  xassert(expr->etag == CilExpr::T_LVAL);
  CilLval *ret = xfr(expr->lval);
  delete expr;
  return ret;
}


CilExpr *newIntLit(CilExtraInfo tn, int val)
{
  CilExpr *ret = new CilExpr(tn, CilExpr::T_LITERAL);
  ret->lit.value = val;
  return ret;
}

CilExpr *newUnaryExpr(CilExtraInfo tn, UnaryOp op, CilExpr *expr)
{
  CilExpr *ret = new CilExpr(tn, CilExpr::T_UNOP);
  ret->unop.op = op;
  ret->unop.exp = expr;
  return ret;
}

CilExpr *newBinExpr(CilExtraInfo tn, BinOp op, CilExpr *e1, CilExpr *e2)
{
  CilExpr *ret = new CilExpr(tn, CilExpr::T_BINOP);
  ret->binop.op = op;
  ret->binop.left = e1;
  ret->binop.right = e2;
  return ret;
}

CilExpr *newCastExpr(CilExtraInfo tn, Type const *type, CilExpr *expr)
{
  CilExpr *ret = new CilExpr(tn, CilExpr::T_CASTE);
  ret->caste.type = type;
  ret->caste.exp = expr;
  return ret;
}

CilExpr *newAddrOfExpr(CilExtraInfo tn, CilLval *lval)
{
  CilExpr *ret = new CilExpr(tn, CilExpr::T_ADDROF);
  ret->addrof.lval = lval;
  return ret;
}

CilExpr *newLvalExpr(CilExtraInfo tn, CilLval *lval)
{
  CilExpr *ret = new CilExpr(tn, CilExpr::T_LVAL);
  ret->lval = lval;
  return ret;
}


// ------------------- CilLval -------------------
CilLval::CilLval(CilExtraInfo tn, LTag tag)
  : CilThing(tn),
    ltag(tag)
{
  validate(ltag);

  // clear fields
  memset(&fieldref, 0, sizeof(fieldref));
}


CilLval::~CilLval()
{
  switch (ltag) {
    case T_DEREF:
      delete deref.addr;
      break;

    case T_FIELDREF:
      delete fieldref.record;
      break;

    case T_CASTL:
      delete castl.lval;
      break;

    case T_ARRAYELT:
      delete arrayelt.array;
      delete arrayelt.index;
      break;

    case T_VAROFS:
      delete ofs.offsets;
      break;

    case T_DEREFOFS:
      delete ofs.addr;
      delete ofs.offsets;
      break;

    INCL_SWITCH
  }
}


Type const *CilLval::getType(Env *env) const
{
  switch (ltag) {
    default: xfailure("bad tag");
    case T_VARREF:    return varref.var->type;
    case T_DEREF:     return deref.addr->getType(env)
                                       ->asPointerTypeC().atType;
    case T_FIELDREF:  return fieldref.field->type;
    case T_CASTL:     return castl.type;
    case T_ARRAYELT:  return arrayelt.array->getType(env)
                                           ->asArrayTypeC().eltType;

    case T_VAROFS:
    case T_DEREFOFS: {
      // get the base type
      Type const *t = ltag==T_VAROFS?
                        ofs.var->type :
                        ofs.addr->getType(env)
                                ->asPointerTypeC().atType;
                                
      // process each offset record
      FOREACH_OBJLIST(CilOffset, *ofs.offsets, iter) {
        if (iter.data()->otag == CilOffset::T_FIELDOFS) {
          // type is now the type of the field
          t = iter.data()->field->type;
        }
        else {
          // indexing doesn't change the type
        }
      }
      
      return t;
    }
  }
}


STATICDEF void CilLval::validate(LTag ltag)
{
  xassert(0 <= ltag && ltag < NUM_LTAGS);
}


ObjList<CilOffset> *cloneOffsets(ObjList<CilOffset> const *src)
{   
  ObjList<CilOffset> *ret = new ObjList<CilOffset>;
  FOREACH_OBJLIST(CilOffset, *src, iter) {
    ret->append(iter.data()->clone());
  }
  return ret;
}


CilLval *CilLval::clone() const
{
  switch (ltag) {
    default: xfailure("bad tag");

    case T_VARREF:
      return newVarRef(extra(), varref.var);

    case T_DEREF:
      return newDeref(extra(), deref.addr->clone());

    case T_FIELDREF:
      return newFieldRef(extra(),
                         fieldref.record->clone(),
                         fieldref.field);

    case T_CASTL:
      return newCastLval(extra(),
                         castl.type,
                         castl.lval->clone());

    case T_ARRAYELT:
      return newArrayAccess(extra(),
                            arrayelt.array->clone(),
                            arrayelt.index->clone());

    case T_VAROFS:
      return newVarOfs(extra(),
                       ofs.var,
                       cloneOffsets(ofs.offsets));

    case T_DEREFOFS:
      return newDerefOfs(extra(),
                         ofs.addr->clone(),
                         cloneOffsets(ofs.offsets));
  }
}


string offsetString(OffsetList const *ofs)
{
  stringBuilder sb;
  FOREACH_OBJLIST(CilOffset, *ofs, iter) {
    switch (iter.data()->otag) {
      case CilOffset::T_FIELDOFS:
        sb << " ." << iter.data()->field->name;
        break;

      case CilOffset::T_INDEXOFS:
        sb << " [" << iter.data()->index->toString() << "]";
        break;
        
      default: xfailure("bad tag");
    }
  }
  return sb;
}


string CilLval::toString() const
{
  switch (ltag) {
    default: xfailure("bad tag");
    case T_VARREF:
      return varref.var->name;
    case T_DEREF:
      return stringc << "(* " << deref.addr->toString() << ")";
    case T_FIELDREF:
      return stringc << "(" << fieldref.record->toString()
                     //<< " /""*" << fieldref.recType->name << "*/"    // odd syntax to avoid irritating emacs' highlighting
                     << " . " << fieldref.field->name << ")";
    case T_CASTL:
      return stringc << "(@[" << castl.type->toString(0)
                     << "] " << castl.lval->toString() << ")";
    case T_ARRAYELT:
      return stringc << "(" << arrayelt.array->toString()
                     << " [" << arrayelt.index->toString() << "])";

    case T_VAROFS:
      return stringc << "(" << ofs.var->name
                     << offsetString(ofs.offsets) << ")";
    case T_DEREFOFS:
      return stringc << "*(" << ofs.addr->toString()
                     << offsetString(ofs.offsets) << ")";

  }
}


// we make the implicit list:
//  and offset =
//    | NoOffset
//    | Field      of fieldinfo * offset    (* l.f + offset *)
//    | Index      of exp * offset          (* l[e] + offset *)
#define MKTAG(n, t) MAKE_ML_TAG(offset, n, t)
MKTAG(0, NoOffset)
MKTAG(1, Field)
MKTAG(2, Index)
#undef MKTAG

MLValue offsetMLValue(OffsetList const *ofs)
{
  MLValue ret = mlTuple0(offset_NoOffset);
    
  // construct it backwards
  for (int i=ofs->count()-1; i>=0; i--) {
    CilOffset const *o = ofs->nthC(i);
    if (o->otag == CilOffset::T_FIELDOFS) {
      ret = mlTuple2(offset_Field,
                     o->field->toMLValue(),
                     ret);
    }
    else { xassert(o->otag == CilOffset::T_INDEXOFS);
      ret = mlTuple2(offset_Index,
                     o->index->toMLValue(),
                     ret);
    }                      
  }
  
  return ret;
}


#define MKTAG(n, t) MAKE_ML_TAG(lval, n, t)
MKTAG(0, Var)         // both new and old
MKTAG(1, Deref)       // ditto
MKTAG(2, Field)
MKTAG(3, CastL)
MKTAG(4, ArrayElt)
#undef MKTAG


MLValue CilLval::toMLValue() const
{
  switch (ltag) {
    default: xfailure("bad tag");

    case T_VARREF:
      // Var        of varinfo * location
      return mlTuple2(lval_Var,
                      varref.var->toMLValue(),
                      locMLValue());

    case T_DEREF:
      return stringc << "(" << lval_Deref << " "
                     <<   deref.addr->toMLValue() << " "
                     <<   locMLValue()
                     << ")";

    case T_FIELDREF:
      return stringc << "(" << lval_Field << " "
                     <<   fieldref.record->toMLValue() << " "
                     <<   fieldref.field->toMLValue() << " "
                     <<   locMLValue()
                     << ")";

    case T_CASTL:
      return stringc << "(" << lval_CastL << " "
                     <<   castl.type->toMLValue() << " "
                     <<   castl.lval->toMLValue() << " "
                     <<   locMLValue()
                     << ")";

    case T_ARRAYELT:
      return stringc << "(" << lval_ArrayElt << " "
                     <<   arrayelt.array->toMLValue() << " "
                     <<   arrayelt.index->toMLValue() << " "
                     << ")";

    case T_VAROFS:
      // Var        of varinfo * offset * location
      return mlTuple3(lval_Var,
                      ofs.var->toMLValue(),
                      offsetMLValue(ofs.offsets),
                      locMLValue());

    case  T_DEREFOFS:
      // Mem        of exp * offset * location(* memory location + offset *)
      return mlTuple3(lval_Deref,
                      ofs.addr->toMLValue(),
                      offsetMLValue(ofs.offsets),
                      locMLValue());
  }
}


void CilLval::xform(CilXform &x)
{
  switch (ltag) {
    default: xfailure("bad tag");

    case T_VARREF:
      x.callTransformVar(varref.var);
      break;

    case T_DEREF:
      x.callTransformExpr(deref.addr);
      break;

    case T_FIELDREF:
      x.callTransformLval(fieldref.record);
      x.callTransformVar(fieldref.field);
      break;

    case T_CASTL:
      x.callTransformType(castl.type);
      x.callTransformLval(castl.lval);
      break;

    case T_ARRAYELT:
      x.callTransformExpr(arrayelt.array);
      x.callTransformExpr(arrayelt.index);
      break;

    case T_VAROFS: {
      x.callTransformVar(ofs.var);
      MUTATE_EACH_OBJLIST(CilOffset, *ofs.offsets, iter) {
        x.callTransformOffset(iter.dataRef());
      }
      break;
    }

    case T_DEREFOFS: {
      x.callTransformExpr(ofs.addr);
      MUTATE_EACH_OBJLIST(CilOffset, *ofs.offsets, iter) {
        x.callTransformOffset(iter.dataRef());
      }
      break;
    }
  }
}


CilLval *newVarRef(CilExtraInfo tn, Variable *var)
{
  // must be an lvalue
  xassert(!var->isEnumValue());

  CilLval *ret = new CilLval(tn, CilLval::T_VARREF);
  ret->varref.var = var;
  return ret;
}

CilExpr *newVarRefExpr(CilExtraInfo tn, Variable *var)
{
  if (var->isEnumValue()) {
    // since Cil doesn't have enums, instead generate a literal
    return newIntLit(tn, var->enumValue);
  }
  else {
    // usual case
    return newLvalExpr(tn, newVarRef(tn, var));
  }
}


CilLval *newDeref(CilExtraInfo tn, CilExpr *ptr)
{
  CilLval *ret = new CilLval(tn, CilLval::T_DEREF);
  ret->deref.addr = ptr;
  return ret;
}

CilLval *newFieldRef(CilExtraInfo tn, CilLval *record, Variable *field)
{
  CilLval *ret = new CilLval(tn, CilLval::T_FIELDREF);
  ret->fieldref.record = record;
  ret->fieldref.field = field;
  return ret;
}

CilLval *newCastLval(CilExtraInfo tn, Type const *type, CilLval *lval)
{
  CilLval *ret = new CilLval(tn, CilLval::T_CASTL);
  ret->castl.type = type;
  ret->castl.lval = lval;
  return ret;
}

CilLval *newArrayAccess(CilExtraInfo tn, CilExpr *array, CilExpr *index)
{
  CilLval *ret = new CilLval(tn, CilLval::T_ARRAYELT);
  ret->arrayelt.array = array;
  ret->arrayelt.index = index;
  return ret;
}

CilLval *newVarOfs(CilExtraInfo tn, Variable *var, OffsetList *offsets)
{
  if (!offsets) {
    // support passing NULL as syntactic sugar for an empty list
    offsets = new OffsetList;
  }
  CilLval *ret = new CilLval(tn, CilLval::T_VAROFS);
  ret->ofs.var = var;
  ret->ofs.offsets = offsets;
  return ret;
}

CilLval *newDerefOfs(CilExtraInfo tn, CilExpr *addr, OffsetList *offsets)
{
  if (!offsets) {
    offsets = new OffsetList;
  }
  CilLval *ret = new CilLval(tn, CilLval::T_DEREFOFS);
  ret->ofs.addr = addr;
  ret->ofs.offsets = offsets;
  return ret;
}



// ------------------- CilOffset ----------------
CilOffset::CilOffset(Variable *f)
  : otag(T_FIELDOFS)
{
  field = f;
}

CilOffset::CilOffset(CilExpr *i)
  : otag(T_INDEXOFS)
{
  index = i;
}

CilOffset::~CilOffset()
{
  if (otag == T_INDEXOFS) {
    delete index;
  }
}


CilOffset *CilOffset::clone() const
{
  switch (otag) {
    default: xfailure("bad tag");
    case T_FIELDOFS:   return new CilOffset(field);
    case T_INDEXOFS:   return new CilOffset(index->clone());
  }
}


void CilOffset::xform(CilXform &x)
{
  if (otag == T_FIELDOFS) {
    x.callTransformVar(field);
  }
  else if (otag == T_INDEXOFS) {
    x.callTransformExpr(index);
  }
  else {
    xfailure("bad tag");
  }
}


// ---------------------- CilInst ----------------
int CilInst::numAllocd = 0;
int CilInst::maxAllocd = 0;

CilInst::CilInst(CilExtraInfo tn, ITag tag)
  : CilThing(tn),
    itag(tag)
{
  INC_HIGH_WATER(numAllocd, maxAllocd);

  validate(itag);

  // clear fields
  memset(&assign, 0, sizeof(assign));

  // init some fields
  switch (itag) {
    case T_CALL:
      call = (CilFnCall*)this;
      break;

    INCL_SWITCH
  }
}


CilInst::~CilInst()
{
  switch (itag) {
    case T_ASSIGN:
      delete assign.lval;
      delete assign.expr;
      break;

    case T_CALL:
      if (call) {
        CilFnCall *ths = call;
        ths->CilFnCall::~CilFnCall();
        numAllocd++;          // counteract double-call
      }
      break;

    case NUM_ITAGS:
      xfailure("bad tag");
  }

  numAllocd--;
}


STATICDEF void CilInst::validate(ITag tag)
{
  xassert(0 <= tag && tag < NUM_ITAGS);
}


STATICDEF void CilInst::printAllocStats(bool anyway)
{                                                  
  if (anyway || numAllocd != 0) {
    cout << "cil inst nodes: " << numAllocd
         << ", max  nodes: " << maxAllocd
         << endl;
  }
}


CilExpr *nullableCloneExpr(CilExpr *expr)
{
  return expr? expr->clone() : NULL;
}


CilInst *CilInst::clone() const
{
  // note: every owner pointer must be cloned, not
  // merely copied!
  switch (itag) {
    case NUM_ITAGS:
      xfailure("bad tag");

    case T_ASSIGN:
      return newAssignInst(extra(),
                           assign.lval->clone(), 
                           assign.expr->clone());

    case T_CALL:
      return call->clone();
  }

  xfailure("bad tag");
}


static ostream &indent(int ind, ostream &os)
{
  while (ind--) {
    os << ' ';
  }
  return os;
}


#define MKTAG(n, t) MAKE_ML_TAG(instr, n, t)
MKTAG(0, Set)
MKTAG(1, Call)
MKTAG(2, Asm)
#undef MKTAG


void CilInst::printTree(int ind, ostream &os) const
{
  if (itag == T_CALL) {
    call->printTree(ind, os);
    return;
  }

  indent(ind, os);

  switch (itag) {
    case T_CALL:
      // handled above already; not reached

    case T_ASSIGN:
      os << "assign " << assign.lval->toString()
         << " := " << assign.expr->toString()
         << ";" << locComment();
      break;

    case NUM_ITAGS:
      xfailure("bad tag");
  }
}


MLValue CilInst::toMLValue() const
{
  switch (itag) {
    default: xfailure("bad tag");

    case T_ASSIGN:
      // Set        of lval * exp * location
      return mlTuple3(instr_Set,
                      assign.lval->toMLValue(),
                      assign.expr->toMLValue(),
                      locMLValue());

    case T_CALL:
      return call->toMLValue();  
  }
}


void CilInst::xform(CilXform &x)
{
  switch (itag) {
    default: xfailure("bad tag");

    case T_ASSIGN:
      if (assign.lval) {
        x.callTransformLval(assign.lval);
      }
      x.callTransformExpr(assign.expr);
      break;

    case T_CALL:
      call->xform(x);
      break;
  }
}


CilInst *newAssignInst(CilExtraInfo tn, CilLval *lval, CilExpr *expr)
{
  xassert(lval && expr);
  CilInst *ret = new CilInst(tn, CilInst::T_ASSIGN);
  ret->assign.lval = lval;
  ret->assign.expr = expr;
  return ret;
}


// -------------------- CilFnCall -------------------
CilFnCall::CilFnCall(CilExtraInfo tn, CilLval *r, CilExpr *f)
  : CilInst(tn, CilInst::T_CALL),
    result(r),
    func(f),
    args()      // initially empty
{}

CilFnCall::~CilFnCall()
{
  call = NULL;          // prevent infinite recursion

  if (result) {
    delete result;
  }
  delete func;
  // args automatically deleted
}


CilLval *nullableCloneLval(CilLval *src)
{
  if (src) {
    return src->clone();
  }
  else {
    return NULL;
  }
}


CilFnCall *CilFnCall::clone() const
{
  CilFnCall *ret =
    new CilFnCall(extra(), nullableCloneLval(result), func->clone());

  FOREACH_OBJLIST(CilExpr, args, iter) {
    ret->args.append(iter.data()->clone());
  }

  return ret;
}


void CilFnCall::printTree(int ind, ostream &os) const
{
  indent(ind, os);
  os << "call ";
  if (result) {
    os << result->toString() << " ";
  }
  os << ":= " << func->toString()
     << "( ";

  int ct=0;
  FOREACH_OBJLIST(CilExpr, args, iter) {
    if (++ct > 1) {
      os << " ,";
    }
    os << " " << iter.data()->toString();
  }

  os << ") ;" << locComment();
}


MLValue CilFnCall::toMLValue() const
{
  // Call       of varinfo option * exp * exp list * location
  return mlTuple4(instr_Call,
                  result?
                    mlSome(result->toMLValue()) :
                    mlNone(),
                  func->toMLValue(),
                  mlObjList(args),
                  locMLValue());
}


void CilFnCall::xform(CilXform &x)
{
  if (result) {
    x.callTransformLval(result);
  }
  x.callTransformExpr(func);
  
  MUTATE_EACH_OBJLIST(CilExpr, args, iter) {
    x.callTransformExpr(iter.dataRef());
  }
}


void CilFnCall::appendArg(CilExpr *arg)
{
  args.append(arg);
}


CilFnCall *newFnCall(CilExtraInfo tn, CilLval *result, CilExpr *fn)
{
  return new CilFnCall(tn, result, fn);
}


// ------------------ CilStmt ---------------
int CilStmt::numAllocd = 0;
int CilStmt::maxAllocd = 0;

CilStmt::CilStmt(CilExtraInfo tn, STag tag)
  : CilThing(tn),
    stag(tag)
{
  INC_HIGH_WATER(numAllocd, maxAllocd);

  validate(stag);

  // clear fields; 'ifthenelse' is the largest in
  // the union
  memset(&ifthenelse, 0, sizeof(ifthenelse));

  // experiment ... statically verify the assumption
  // that 'ifthenelse' is really the biggest
  STATIC_ASSERT((char*)(this + 1) - (char*)&ifthenelse == sizeof(ifthenelse));

  // init some fields
  switch (stag) {
    case T_COMPOUND:
      comp = (CilCompound*)this;
      break;

    INCL_SWITCH
  }
}


CilStmt::~CilStmt()
{
  switch (stag) {
    case T_COMPOUND:
      if (comp) {
        CilCompound *ths = comp;
        ths->CilCompound::~CilCompound();
        numAllocd++;          // counteract double-call
      }
      break;

    case T_LOOP:
      delete loop.cond;
      delete loop.body;
      break;

    case T_IFTHENELSE:
      delete ifthenelse.cond;
      delete ifthenelse.thenBr;
      delete ifthenelse.elseBr;
      break;

    case T_LABEL:
      delete label.name;
      break;

    case T_JUMP:
      delete jump.dest;
      break;

    case T_RET:
      if (ret.expr) {
        // I know 'delete' accepts NULL (and I rely on that
        // in other modules); I just like to emphasize when
        // something is nullable
        delete ret.expr;
      }
      break;

    case T_SWITCH:
      delete switchStmt.expr;
      delete switchStmt.body;
      break;

    case T_CASE:
      break;

    case T_DEFAULT:
      break;

    case T_INST:
      delete inst.inst;
      break;  

    case NUM_STAGS:
      xfailure("bad tag");
  }

  numAllocd--;
}


STATICDEF void CilStmt::validate(STag tag)
{
  xassert(0 <= tag && tag < NUM_STAGS);
}


STATICDEF void CilStmt::printAllocStats(bool anyway)
{
  if (anyway || numAllocd != 0) {
    cout << "cil stmt nodes: " << numAllocd
         << ", max  nodes: " << maxAllocd
         << endl;
  }
}


CilStmt *CilStmt::clone() const
{
  // note: every owner pointer must be cloned, not
  // merely copied!
  switch (stag) {
    case NUM_STAGS:
      xfailure("bad tag");

    case T_COMPOUND:
      return comp->clone();

    case T_LOOP:
      return newWhileLoop(extra(), 
                          loop.cond->clone(), 
                          loop.body->clone());

    case T_IFTHENELSE:
      return newIfThenElse(extra(),
                           ifthenelse.cond->clone(),
                           ifthenelse.thenBr->clone(),
                           ifthenelse.elseBr->clone());

    case T_LABEL:
      return newLabel(extra(), *(label.name));

    case T_JUMP:
      return newGoto(extra(), *(jump.dest));

    case T_RET:
      return newReturn(extra(), nullableCloneExpr(ret.expr));

    case T_SWITCH:
      return newSwitch(extra(), 
                       switchStmt.expr->clone(),
                       switchStmt.body->clone());

    case T_CASE:
      return newCase(extra(), caseStmt.value);

    case T_INST:
      return newInst(extra(), inst.inst->clone());

    case T_DEFAULT:
      return newDefault(extra());
  }

  xfailure("bad tag");
}


void CilStmt::printTree(int ind, ostream &os) const
{
  if (stag == T_COMPOUND) {
    comp->printTree(ind, os);
    return;
  }
  else if (stag == T_INST) {
    inst.inst->printTree(ind, os);
    return;
  }

  indent(ind, os);

  switch (stag) {
    case T_COMPOUND:
    case T_INST:
      // handled above already; not reached

    case T_LOOP:
      os << "while ( " << loop.cond->toString()
         << " ) {" << locComment();
      loop.body->printTree(ind+2, os);
      indent(ind, os) << "}\n";
      break;

    case T_IFTHENELSE:
      os << "if ( " << ifthenelse.cond->toString()
         << ") {" << locComment();
      ifthenelse.thenBr->printTree(ind+2, os);
      indent(ind, os) << "}\n";
      indent(ind, os) << "else {\n";
      ifthenelse.elseBr->printTree(ind+2, os);
      indent(ind, os) << "}\n";
      break;

    case T_LABEL:
      os << "label " << *(label.name)
         << " :" << locComment();
      break;

    case T_JUMP:
      os << "goto " << *(jump.dest)
         << " ;" << locComment();
      break;

    case T_RET:
      os << "return";
      if (ret.expr) {
        os << " " << ret.expr->toString();
      }
      os << " ;" << locComment();
      break;

    case T_SWITCH:
      os << "switch (" << switchStmt.expr->toString()
         << ") {" << locComment();
      switchStmt.body->printTree(ind+2, os);
      indent(ind, os) << "}\n";
      break;

    case T_CASE:
      os << "case " << caseStmt.value
         << ":" << locComment();
      break;

    case T_DEFAULT:
      os << "default:" << locComment();
      break;

    case NUM_STAGS:
      xfailure("bad tag");
  }
}


#define MKTAG(n, t) MAKE_ML_TAG(stmt, n, t)
MKTAG(0, Skip)
MKTAG(1, Sequence)
MKTAG(2, While)
MKTAG(3, IfThenElse)
MKTAG(4, Label)
MKTAG(5, Goto)
MKTAG(6, Return)
MKTAG(7, Switch)
MKTAG(8, Case)
MKTAG(9, Default)
MKTAG(10, Break)
MKTAG(11, Continue)
MKTAG(12, Instruction)
#undef MKTAG


MLValue CilStmt::toMLValue() const
{
  switch (stag) {
    default:
      xfailure("bad tag");

    case T_COMPOUND:
      return comp->toMLValue();

    case T_LOOP:
      // While of exp * stmt
      return mlTuple2(stmt_While,
                      loop.cond->toMLValue(),
                      loop.body->toMLValue());

    case T_IFTHENELSE:
      // IfThenElse of exp * stmt * stmt
      return mlTuple3(stmt_IfThenElse,
                      ifthenelse.cond->toMLValue(),
                      ifthenelse.thenBr->toMLValue(),
                      ifthenelse.elseBr->toMLValue());

    case T_LABEL:
      // Label of string
      return mlTuple1(stmt_Label,
                      mlString(*(label.name)));

    case T_JUMP:
      // Goto of string
      return mlTuple1(stmt_Goto,
                      mlString(*(jump.dest)));

    case T_RET:
      // Return of exp option
      return mlTuple1(stmt_Return,
                      ret.expr?
                        mlSome(ret.expr->toMLValue()) :
                        mlNone());

    case T_SWITCH:
      // Switch of exp * stmt
      return mlTuple2(stmt_Switch,
                      switchStmt.expr->toMLValue(),
                      switchStmt.body->toMLValue());

    case T_CASE:
      // Case of int
      return mlTuple1(stmt_Case,
                      mlInt(caseStmt.value));

    case T_DEFAULT:
      // Default
      return mlTuple0(stmt_Default);

    case T_INST:
      // Instruction of instr
      return mlTuple1(stmt_Instruction,
                      inst.inst->toMLValue());
  }
}


void CilStmt::xform(CilXform &x)
{
  switch (stag) {
    default: xfailure("bad tag");

    case T_COMPOUND:
      comp->xform(x);
      break;

    case T_LOOP:
      x.callTransformExpr(loop.cond);
      x.callTransformStmt(loop.body);
      break;
      
    case T_IFTHENELSE:
      x.callTransformExpr(ifthenelse.cond);
      x.callTransformStmt(ifthenelse.thenBr);
      x.callTransformStmt(ifthenelse.elseBr);
      break;
      
    case T_LABEL:
      x.callTransformLabel(label.name);
      break;

    case T_JUMP:
      x.callTransformLabel(jump.dest);
      break;

    case T_RET:
      if (ret.expr) {
        x.callTransformExpr(ret.expr);
      }
      break;
      
    case T_SWITCH:
      x.callTransformExpr(switchStmt.expr);
      x.callTransformStmt(switchStmt.body);
      break;
      
    case T_CASE:
      x.callTransformInt(caseStmt.value);
      break;
      
    case T_DEFAULT:
      break;
      
    case T_INST:
      x.callTransformInst(inst.inst);
      break;
  }
}


CilStmt *newWhileLoop(CilExtraInfo tn, CilExpr *expr, CilStmt *body)
{
  xassert(expr && body);
  CilStmt *ret = new CilStmt(tn, CilStmt::T_LOOP);
  ret->loop.cond = expr;
  ret->loop.body = body;
  return ret;
}

CilStmt *newIfThenElse(CilExtraInfo tn, CilExpr *cond, CilStmt *thenBranch, CilStmt *elseBranch)
{
  xassert(cond && thenBranch && elseBranch);
  CilStmt *ret = new CilStmt(tn, CilStmt::T_IFTHENELSE);
  ret->ifthenelse.cond = cond;
  ret->ifthenelse.thenBr = thenBranch;
  ret->ifthenelse.elseBr = elseBranch;
  return ret;
}

CilStmt *newLabel(CilExtraInfo tn, LabelName label)
{
  CilStmt *ret = new CilStmt(tn, CilStmt::T_LABEL);
  ret->label.name = new LabelName(label);
  return ret;
}

CilStmt *newGoto(CilExtraInfo tn, LabelName label)
{
  CilStmt *ret = new CilStmt(tn, CilStmt::T_JUMP);
  ret->jump.dest = new LabelName(label);
  return ret;
}

CilStmt *newReturn(CilExtraInfo tn, CilExpr *expr /*nullable*/)
{
  CilStmt *ret = new CilStmt(tn, CilStmt::T_RET);
  ret->ret.expr = expr;
  return ret;
}

CilStmt *newSwitch(CilExtraInfo tn, CilExpr *expr, CilStmt *body)
{
  xassert(expr && body);
  CilStmt *ret = new CilStmt(tn, CilStmt::T_SWITCH);
  ret->switchStmt.expr = expr;
  ret->switchStmt.body = body;
  return ret;
}

CilStmt *newCase(CilExtraInfo tn, int val)
{
  CilStmt *ret = new CilStmt(tn, CilStmt::T_CASE);
  ret->caseStmt.value = val;
  return ret;
}

CilStmt *newDefault(CilExtraInfo tn)
{
  CilStmt *ret = new CilStmt(tn, CilStmt::T_DEFAULT);
  return ret;
}

CilStmt *newInst(CilExtraInfo tn, CilInst *inst)
{
  xassert(inst);
  CilStmt *ret = new CilStmt(tn, CilStmt::T_INST);
  ret->inst.inst = inst;
  return ret;
}

CilStmt *newAssign(CilExtraInfo tn, CilLval *lval, CilExpr *expr)
{
  return newInst(tn, newAssignInst(tn, lval, expr));
}


// ------------------- CilStatements ---------------------
CilStatements::CilStatements()
{}

CilStatements::~CilStatements()
{}


void CilStatements::append(CilStmt *inst)
{
  stmts.append(inst);
}


// the point of separating this from CilCompound::printTree
// is to get a version which doesn't wrap braces around the
// code; this is useful in cc.gr where I am artificially
// creating compounds but they're all really at the same scope
// (namely toplevel)
void CilStatements::printTreeNoBraces(int ind, ostream &os) const
{
  FOREACH_OBJLIST(CilStmt, stmts, iter) {
    try {
      iter.data()->printTree(ind, os);
    }
    catch (xBase &x) {
      os << "$$$ // error printing: " << x << endl;
    }
  }
}


// ----------------- CilCompound ---------------------
CilCompound::CilCompound(CilExtraInfo tn)
  : CilStmt(tn, CilStmt::T_COMPOUND)
{}

CilCompound::~CilCompound()
{
  comp = NULL;          // prevent infinite recursion
}


CilCompound *CilCompound::clone() const
{
  CilCompound *ret = new CilCompound(extra());

  FOREACH_OBJLIST(CilStmt, stmts, iter) {
    ret->append(iter.data()->clone());
  }

  return ret;
}


void CilCompound::printTree(int ind, ostream &os) const
{
  printTreeNoBraces(ind, os);
}


MLValue CilCompound::toMLValue() const
{
  // Sequence of stmt list
  return mlTuple1(stmt_Sequence,
                  mlObjList(stmts));
}


void CilCompound::xform(CilXform &x)
{
  MUTATE_EACH_OBJLIST(CilStmt, stmts, iter) {
    x.callTransformStmt(iter.dataRef());
  }
}


CilCompound *newCompound(CilExtraInfo tn)
{
  return new CilCompound(tn);
}


// ------------------- CilBB ------------------
int CilBB::nextId = 0;
int CilBB::numAllocd = 0;
int CilBB::maxAllocd = 0;

CilBB::CilBB(BTag t)
  : id(nextId++),            // don't know what to do with this..
    insts(),
    btag(t)
{
  INC_HIGH_WATER(numAllocd, maxAllocd);
  validate(btag);

  // clear variant fields
  memset(&ifbb, 0, sizeof(ifbb));

  // init some fields
  if (btag == T_SWITCH) {
    switchbb = (CilBBSwitch*)this;
  }
}

CilBB::~CilBB()
{
  switch (btag) {
    case T_RETURN:
      //delete ret.expr;
      break;

    case T_IF:
      //delete ifbb.expr;
      delete ifbb.thenBB;
      delete ifbb.elseBB;
      break;

    case T_SWITCH:
      if (switchbb) {
        CilBBSwitch *ths = switchbb;
        ths->CilBBSwitch::~CilBBSwitch();     // nullifies switchbb
        numAllocd++;                          // counteract double call
      }
      break;

    case T_JUMP:
      if (jump.targetLabel) {
        delete jump.targetLabel;
      }
      break;

    default:
      xfailure("bad tag");
  }

  numAllocd--;
}


void CilBB::printTree(int ind, ostream &os) const
{
  indent(ind, os) << "basicBlock_" << id << ":\n";
  ind += 2;

  SFOREACH_OBJLIST(CilInst, insts, iter) {
    iter.data()->printTree(ind, os);
  }

  if (btag == T_SWITCH) {
    switchbb->printTree(ind, os);
    return;
  }

  indent(ind, os);
  switch (btag) {
    default:
      xfailure("bad tag");

    case T_RETURN:
      if (ret.expr) {
        os << "return " << ret.expr->toString() << ";\n";
      }
      else {
        os << "return;\n";
      }
      break;

    case T_IF:
      if (ifbb.loopHint) {
        os << "while-if";
      }
      else {
        os << "if";
      }
      os << " (" << ifbb.expr->toString() << ") {\n";
      ifbb.thenBB->printTree(ind+2, os);
      indent(ind, os) << "}\n";
      indent(ind, os) << "else {\n";
      ifbb.elseBB->printTree(ind+2, os);
      indent(ind, os) << "}\n";
      break;

    case T_SWITCH:
      // handled above; not reached

    case T_JUMP:
      if (jump.nextBB) {
        os << "goto basicBlock_" << jump.nextBB->id << ";\n";
      }
      else {
        // goto whose label hasn't yet been fixed-up
        os << "incompleteGoto " << *(jump.targetLabel) << ";\n";
      }
      break;
  }
}


STATICDEF void CilBB::validate(BTag btag)
{
  xassert(0 <= btag && btag < NUM_BTAGS);
}


STATICDEF void CilBB::printAllocStats(bool anyway)
{
  if (anyway || numAllocd != 0) {
    cout << "cil bb nodes: " << numAllocd
         << ", max nodes: " << maxAllocd << endl;
  }
}


CilBB *newJumpBB(CilBB *target)
{
  CilBB *ret = new CilBB(CilBB::T_JUMP);
  ret->jump.nextBB = target;
  return ret;
}


// ------------------ BBSwitchCase -----------------
BBSwitchCase::~BBSwitchCase()
{}


// ------------------ CilBBSwitch ---------------
CilBBSwitch::CilBBSwitch(CilExpr * /*owner*/ e)
  : CilBB(T_SWITCH),
    expr(e),
    exits(),
    defaultCase(NULL)
{}

CilBBSwitch::~CilBBSwitch()
{
  // prevent infinite recursion
  switchbb = NULL;
}


void CilBBSwitch::printTree(int ind, ostream &os) const
{
  // assume I already printed the block id and the instructions
  
  indent(ind, os) << "switch (" << expr->toString() << ") {\n";
           
  FOREACH_OBJLIST(BBSwitchCase, exits, iter) {
    indent(ind+2, os) << "case " << iter.data()->value << ":\n";
    iter.data()->code->printTree(ind+4, os);
  }
  
  indent(ind+2, os) << "default:\n";
  if (defaultCase) {
    defaultCase->printTree(ind+4, os);
  }
  else {
    // defaultCase isn't supposed to be nullable, but since
    // I create it NULL, it seems prudent to handle this anyway
    indent(ind+4, os) << "// NULL defaultCase!\n";
  }

  indent(ind, os) << "}\n";
}


// -------------------- CilFnDefn -----------------
CilFnDefn::~CilFnDefn()
{}
                                                   

void CilFnDefn::printTree(int ind, ostream &os, bool stmts) const
{
  os << "fundecl " << var->name
     << " : " << var->type->toString(0);
  indent(ind, os) << " {" << locComment();

  // print locals
  FOREACH_VARIABLE(locals, iter) {
    indent(ind+2, os)
       << "local " << iter->name
       << " : " << iter->type->toString(0 /*depth*/) << " ;\n";
  }
  os << endl;

  if (stmts) {
    // print statements
    bodyStmt.printTree(ind+2, os);
  }
  else {
    // print basic blocks
    indent(ind+2, os) << "startBB " << startBB->id << ";\n";
    FOREACH_OBJLIST(CilBB, bodyBB, iter) {
      iter.data()->printTree(ind+2, os);
    }
  }

  indent(ind, os) << "}\n";
}


MLValue CilFnDefn::toMLValue() const
{
  //    type fundec =
  //        { sname: string;                    (* function name *)
  //          slocals: varinfo list;            (* locals *)
  //          smaxid: int;                      (* max local id. Starts at 0 *)
  //          sbody: stmt;                      (* the body *)
  //          stype: typ;                       (* the function type *)
  //          sstorage: storage;
  //          sattr: attributes list;
  //        }

  // build list of locals
  MLValue mlLocals = mlNil();
  for (VariableId id = locals.numVars()-1;
       id >= FIRST_VARIABLEID;
       id--) {
    mlLocals = mlCons(locals.lookupC(id)->toMLValue(), mlLocals);
  }

  return mlRecord7("sname", mlString(var->name),
                   "slocals", mlLocals,
                   "smaxid", mlInt(locals.numVars()),
                   "sbody", bodyStmt.toMLValue(),
                   "stype", var->type->toMLValue(),
                   "sstorage", mlStorage(var->declFlags),
                   "sattr", mlNil());
}


void CilFnDefn::xform(CilXform &x)
{
  x.callTransformVar(var);
  
  // doesn't allow replacement of this compound
  bodyStmt.xform(x);

  for (VariableId id=FIRST_VARIABLEID;
       id < locals.numVars();
       id++) {
    // might consider defining something like
    //   callTrasnsformLocal( .. )
    x.callTransformOwnerVar(locals.lookupRef(id));
  }
}


// ------------------ CilProgram ---------------
CilProgram::CilProgram()
{}

CilProgram::~CilProgram()
{}


void CilProgram::printTree(int ind, ostream &os, bool ml) const
{
  if (!ml) {
    // globals
    for (VariableId vid=FIRST_VARIABLEID /*0*/;
         vid < globals.numVars();
         vid++) {
      Variable const *v = globals.lookupC(vid);
      indent(ind, os)
         << "global " << v->name << " : ";
      os << v->type->toString(0 /*depth*/) << " ;\n";
    }

    FOREACH_OBJLIST(CilFnDefn, funcs, iter) {
      iter.data()->printTree(ind, os, true /*stmts*/);
    }
  }

  else /*ml*/ {
    // type file = global list
    indent(ind, os) << "[    // program start\n\n";

    // type global =
    //     GFun of fundec
    //   | GType of string * typ               (* A typedef *)
    //   | GVar of varinfo * exp option        (* A global variable with
    //                                          * initializer. Includes function prototypes *)
    //   | GAsm of string                      (* Global asm statement *)
    MAKE_ML_TAG(global, 0, GFun);
    MAKE_ML_TAG(global, 1, GType);
    MAKE_ML_TAG(global, 2, GVar);
    MAKE_ML_TAG(global, 3, GAsm);

    // print all types first, skipping the simple types first
    indent(ind,os) << "// ============ types ==============\n";
    for (AtomicTypeId a=NUM_SIMPLE_TYPES;
         a < types.numAtomicTypes();
         a++) {
      AtomicType const *at = types.lookupAtomicC(a);
      
      // depth of 2 is used here because otherwise we just say:
      //   typedef foo foo;    
      // instead of expanding the definition of foo
      indent(ind,os) << mlTuple2(global_GType,
                                 mlString(at->uniqueName()),
                                 at->toMLValue(2 /*depth*/, CV_NONE))
                     << " ;\n";
    }

    // then all globals
    indent(ind,os) << "\n// ============= globals ============\n";
    for (VariableId vid=FIRST_VARIABLEID /*0*/;
         vid < globals.numVars();
         vid++) {
      Variable const *v = globals.lookupC(vid);
      if (!( v->declFlags & DF_BUILTIN )) {
        indent(ind,os) << mlTuple2(global_GVar,
                                   v->toMLValue(),
                                   mlNone())
                       << " ;\n";
      }
      else {
        // don't emit builtins because I expect this to
        // eventually be re-emitted as C, to be processed
        // by gcc, which would be upset about redefinitions
        // of builtins
      }
    }

    // finally, all functions
    indent(ind,os) << "\n// ============ functions ==========\n";
    FOREACH_OBJLIST(CilFnDefn, funcs, iter) {
      indent(ind,os) << "// ---- " 
                     << iter.data()->var->toString()
                     << " ----\n";
      indent(ind,os) << mlTuple1(global_GFun,
                                 iter.data()->toMLValue())
                     << " ;\n\n\n";
    }

    // end the list; put something here so I don't have to
    // do special things to get semicolons *between* the
    // things printed already
    indent(ind,os) << "\n"
                   << "// end of program\n"
                   << mlTuple1(global_GAsm,
                               mlString(".nop")) << "\n"
                   << "]\n";
  }
}


void CilProgram::empty()
{                 
  types.empty();
  globals.empty();
  funcs.deleteAll();
}


// --------------------- CilContext -----------------
void CilContext::append(CilStmt * /*owner*/ s) const
{
  if (!isTrial) {
    stmts->append(s);
  }
  else {
    delete s;
  }
}


void CilContext::addVarDecl(Variable *var) const
{
  // I don't think this is needed anymore ...
  #if 0
  if (!isTrial) {
    if (fn) {
      xassert(!var->isGlobal());
      fn->locals.append(var);
    }
    else {
      // it's a global if we're not in the context
      // of any function definition
      xassert(var->isGlobal());
      prog->globals.append(var);
    }
  }
  #endif // 0
}
