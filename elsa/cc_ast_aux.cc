// cc_ast_aux.cc            see license.txt for copyright and terms of use
// auxilliary code (debug printing, etc.) for cc.ast

#include "strutil.h"        // plural
#include "generic_aux.h"    // C++ AST, and genericPrintAmbiguities, etc.


// TranslationUnit
// TopForm

// ---------------------- Function --------------------
void Function::printExtras(ostream &os, int indent) const
{
  if (funcType) {
    ind(os, indent) << "funcType: " << funcType->toString() << "\n";
  }
}


// ---------------------- MemberInit ----------------------
void MemberInit::printExtras(ostream &os, int indent) const
{
  if (member) {
    ind(os, indent) << "member: refers to " << toString(member->loc) << "\n";
  }       

  if (base) {
    ind(os, indent) << "base: " << base->toCString() << "\n";
  }
}


// Declaration

// ---------------------- ASTTypeId -----------------------
void ASTTypeId::printAmbiguities(ostream &os, int indent) const
{
  genericPrintAmbiguities(this, "ASTTypeId", os, indent);
  
  genericCheckNexts(this);
}

void ASTTypeId::addAmbiguity(ASTTypeId *alt)
{
  genericAddAmbiguity(this, alt);
}

void ASTTypeId::setNext(ASTTypeId *newNext)
{
  genericSetNext(this, newNext);
}


// ------------------------ PQName ------------------------
string targsToString(FakeList<TemplateArgument> const *list)
{
  stringBuilder sb;
  sb << "<";
  int ct=0;
  FAKELIST_FOREACH(TemplateArgument, list, iter) {
    if (ct++ > 0) {
      sb << ", ";
    }
    sb << iter->argString();
  }
  sb << ">";       
  return sb;
}


string PQName::qualifierString() const
{
  stringBuilder sb;

  PQName const *p = this;
  while (p->isPQ_qualifier()) {
    PQ_qualifier const *q = p->asPQ_qualifierC();
    if (q->qualifier) {
      sb << q->qualifier;

      if (q->targs) {
        sb << targsToString(q->targs);
      }
    }
    else {
      // for a NULL qualifier, don't print anything; it means
      // there was a leading "::" with no explicit qualifier,
      // and I'll use similar syntax on output
    }
    sb << "::";

    p = q->rest;
  }
  return sb;
}

stringBuilder& operator<< (stringBuilder &sb, PQName const &obj)
{ 
  // leading qualifiers, with template arguments as necessary
  sb << obj.qualifierString();

  // final simple name
  PQName const *final = obj.getUnqualifiedNameC();
  sb << final->getName();
                      
  // template arguments applied to final name
  if (final->isPQ_template()) {
    sb << targsToString(final->asPQ_templateC()->args);
  }

  return sb;
}

string PQName::toString() const
{
  stringBuilder sb;
  sb << *this;
  return sb;
}


StringRef PQ_qualifier::getName() const
{
  return rest->getName();
}

StringRef PQ_name::getName() const
{
  return name;
}

StringRef PQ_operator::getName() const
{
  return fakeName;
}

StringRef PQ_template::getName() const
{
  return name;
}


PQName const *PQName::getUnqualifiedNameC() const
{                   
  PQName const *p = this;
  while (p->isPQ_qualifier()) {
    p = p->asPQ_qualifierC()->rest;
  }
  return p;
}


//  ------------------- TypeSpecifier ---------------------
void TypeSpecifier::printExtras(ostream &os, int indent) const
{
  PRINT_GENERIC(cv);
}


// ------------------- BaseClassSpec ---------------------
void BaseClassSpec::printExtras(ostream &os, int indent) const
{
  if (type) {
    ind(os, indent) << "type: " << type->toCString() << "\n";
  }
}


// MemberList
// Member

// ---------------------- Enumerator ------------------
void Enumerator::printExtras(ostream &os, int indent) const
{
  if (var) {
    ind(os, indent) << "var: " 
      << toString(var->flags) << (var->flags? " " : "")
      << var->type->toCString(var->name) << "\n";
    PRINT_GENERIC(enumValue);
  }
}


// ---------------------- Declarator ---------------------------
void Declarator::printAmbiguities(ostream &os, int indent) const
{
  genericPrintAmbiguities(this, "Declarator", os, indent);
  
  // check 'next' fields
  for (Declarator *d = ambiguity; d != NULL; d = d->ambiguity) {
    xassert(this->next == d->next);
  }
}


void Declarator::addAmbiguity(Declarator *alt)
{
  genericAddAmbiguity(this, alt);
}

void Declarator::setNext(Declarator *newNext)
{
  genericSetNext(this, newNext);
}


PQName const *Declarator::getDeclaratorId() const
{
  return decl->getDeclaratorId();
}


void Declarator::printExtras(ostream &os, int indent) const
{
  if (var) {
    ind(os, indent) << "var: "
      << toString(var->flags) << (var->flags? " " : "")
      << var->type->toCString(var->name);

    if (var->overload) {
      int n = var->overload->count();
      os << " (" << n << " " << plural(n, "overloading") << ")";
    }

    os << "\n";
  }
}


// --------------------- IDeclarator ---------------------------
PQName const *D_name::getDeclaratorId() const
{
  return name;
}

PQName const *D_pointer::getDeclaratorId() const
{
  return base->getDeclaratorId();
}

PQName const *D_func::getDeclaratorId() const
{
  return base->getDeclaratorId();
}

PQName const *D_array::getDeclaratorId() const
{
  return base->getDeclaratorId();
}

PQName const *D_bitfield::getDeclaratorId() const
{
  // the ability to simply return 'name' here is why bitfields contain
  // a PQName instead of just a StringRef
  return name;
}

PQName const *D_ptrToMember::getDeclaratorId() const
{
  return base->getDeclaratorId();
}

PQName const *D_grouping::getDeclaratorId() const
{
  return base->getDeclaratorId();
}


IDeclarator *IDeclarator::skipGroups()
{
  if (isD_grouping()) {
    return asD_grouping()->base->skipGroups();
  }
  else {
    return this;
  }
}


// ExceptionSpec
// OperatorDeclarator

// ---------------------- Statement --------------------
void Statement::printAmbiguities(ostream &os, int indent) const
{
  genericPrintAmbiguities(this, "Statement", os, indent);
}


void Statement::addAmbiguity(Statement *alt)
{
  // this does not call 'genericAddAmbiguity' because Statements
  // do not have 'next' fields

  // prepend 'alt' to my list
  const_cast<Statement*&>(alt->ambiguity) = ambiguity;
  const_cast<Statement*&>(ambiguity) = alt;
}


string Statement::lineColString() const
{
  char const *fname;
  int line, col;
  sourceLocManager->decodeLineCol(loc, fname, line, col);

  return stringc << line << ":" << col;
}

string Statement::kindLocString() const
{
  return stringc << kindName() << "@" << lineColString();
}


// Condition

// ----------------------- Handler ----------------------
bool Handler::isEllipsis() const
{
  return typeId->spec->isTS_simple() &&
         typeId->spec->asTS_simple()->id == ST_ELLIPSIS;
}


// --------------------- Expression ---------------------
void Expression::printAmbiguities(ostream &os, int indent) const
{
  genericPrintAmbiguities(this, "Expression", os, indent);

  genericCheckNexts(this);
}


void Expression::addAmbiguity(Expression *alt)
{
  genericAddAmbiguity(this, alt);
}

void Expression::setNext(Expression *newNext)
{
  // relaxation: The syntax
  //   tok = strtok(((void *)0) , delim);
  // provokes a double-add, where 'next' is the same both
  // times.  I think this is because we merge a little
  // later than usual due to unexpected state splitting.
  // I might try to investigate this more carefully at a
  // later time, but for now..
  if (next == newNext) {
    return;    // bail if it's already what we want..
  }

  genericSetNext(this, newNext);
}


void Expression::printExtras(ostream &os, int indent) const
{         
  if (type) {
    ind(os, indent) << "type: " << type->toString() << "\n";
  }

  // print type-specific extras
  ASTSWITCHC(Expression, this) {
    ASTCASEC(E_intLit, i) {
      ind(os, indent) << "i: " << i->i << "\n";
    }

    ASTNEXTC(E_floatLit, f) {
      ind(os, indent) << "f: " << f->d << "\n";
    }

    ASTNEXTC(E_stringLit, s) {
      // nothing extra to print since there's no interpretation yet
      PRETEND_USED(s);
    }

    ASTNEXTC(E_charLit, c) {
      ind(os, indent) << "c: " << c->c << "\n";    // prints as an integer
    }

    ASTNEXTC(E_variable, v)
      if (v->var) {
        ind(os, indent) << "var: refers to " << toString(v->var->loc) << "\n";
      }

    ASTNEXTC(E_new, n)
      PRINT_SUBTREE(n->arraySize);

    ASTNEXTC(E_fieldAcc, f)
      if (f->field) {
        ind(os, indent) << "field: refers to " << toString(f->field->loc) << "\n";
      }

    ASTDEFAULTC
      /* do nothing */

    ASTENDCASEC
  }
}


// ExpressionListOpt
// Initializer
// InitLabel

// ------------------- TemplateDeclaration ------------------
void TD_class::printExtras(ostream &os, int indent) const
{
  if (type) {
    ind(os, indent) << "type: " << type->toString() << "\n";
  }
}


// TemplateParameter

// -------------------- TemplateArgument ---------------------
void TemplateArgument::printAmbiguities(ostream &os, int indent) const
{
  genericPrintAmbiguities(this, "TemplateArgument", os, indent);

  genericCheckNexts(this);
}


void TemplateArgument::addAmbiguity(TemplateArgument *alt)
{
  genericAddAmbiguity(this, alt);
}

void TemplateArgument::setNext(TemplateArgument *newNext)
{
  if (next == newNext) {
    return;    // bail if it's already what we want..
  }

  genericSetNext(this, newNext);
}


string TA_type::argString() const
{
  return type->getType()->toString();
}

string TA_nontype::argString() const
{
  return expr->exprToString();
}   


void TemplateArgument::printExtras(ostream &os, int indent) const
{
  if (sarg.hasValue()) {
    ind(os, indent) << "sarg: " << sarg.toString() << "\n";
  }
}
