// vcgen.cc
// vcgen methods on the C abstract syntax tree

#include "c.ast.gen.h"          // C ast
#include "absval.ast.gen.h"     // abstract domain values
#include "aenv.h"               // AEnv
#include "cc_type.h"            // FunctionType, etc.
#include "sobjlist.h"           // SObjList
#include "trace.h"              // tracingSys
#include "strutil.h"            // quoted

#define IN_PREDICATE(env) Restorer<bool> restorer(env.inPredicate, true)


// use for places that aren't implemented yet
AbsValue *avTodo()
{
  return new AVint(12345678);   // hopefully recognizable
}


// ------------------- TranslationUnit ---------------------
void TranslationUnit::vcgen(AEnv &env) const
{
  FOREACH_ASTLIST(TopForm, topForms, iter) {
    iter.data()->vcgen(env);
  }
}


// --------------------- TopForm ----------------------
void TF_decl::vcgen(AEnv &env) const
{}


void TF_func::vcgen(AEnv &env) const
{
  FunctionType const &ft = *(ftype());

  traceProgress() << "analyzing " << name() << " ...\n";

  // clear anything left over in env
  env.clear();

  // add the function parameters to the environment
  FOREACH_OBJLIST(FunctionType::Param, ft.params, iter) {
    FunctionType::Param const *p = iter.data();

    if (p->name) {
      // the function parameter's initial value is represented
      // by a logic variable of the same name
      env.set(p->name, new AVvar(p->name,
        stringc << "initial value of parameter " << p->name));
    }
  }

  // let pre_mem be the name of memory now
  //env.set(env.str("pre_mem"), env.getMem());

  // add the precondition as an assumption
  if (ft.precondition) {
    IN_PREDICATE(env);

    // add any precondition bindings    
    FOREACH_ASTLIST(Declaration, ft.precondition->decls, iter) {
      iter.data()->vcgen(env);
    }

    env.addFact(ft.precondition->expr->vcgen(env));
  }

  // add 'result' to the environment so we can record what value
  // is actually returned
  if (!ft.retType->isVoid()) {
    env.set(env.str("result"), env.freshVariable("ret", "return value"));
  }

  // now interpret the function body
  body->vcgen(env);

  // print the results
  if (tracingSys("absInterp")) {
    cout << "interpretation after " << name() << ":\n";
    env.print();
  }

  // prove the postcondition now holds
  // ASSUMPTION: none of the parameters have been changed
  if (ft.postcondition) {
    IN_PREDICATE(env);
    env.prove(ft.postcondition->expr->vcgen(env), "postcondition");
  }
}


// --------------------- Declaration --------------------
void Declaration::vcgen(AEnv &env) const
{
  FOREACH_ASTLIST(Declarator, decllist, iter) {
    iter.data()->vcgen(env);
  }
}

void Declarator::vcgen(AEnv &env) const
{
  if (name) {
    // make up a name for the initial value
    AbsValue *value;
    if (init) {
      // evaluate 'init' to an abstract value; the initializer
      // will need to know which type it's constructing
      value = init->vcgen(env, type);
    }
    else {
      // make up a new name for the uninitialized value
      // (if it's global we get to assume it's 0... not implemented..)
      value = new AVvar(name, stringc << "UNINITialized value of var " << name);
    }
    
    if (addrTaken || type->isArrayType()) {
      // model this variable as a location in memory; make up a name
      // for its address
      AbsValue *addr = env.addMemVar(name);

      if (!type->isArrayType()) {
        // state that, right now, that memory location contains 'value'
        env.addFact(new AVbinary(value, BIN_EQUAL,
          env.avSelect(env.getMem(), addr, new AVint(0))));
      }
      else {
        // will model array contents as elements in memory, rather
        // than as symbolic whole-array values...
      }

      // variables have length 1
      int size = 1;             

      if (type->isArrayType() && type->asArrayTypeC().hasSize) {
        // state that the length is whatever the array length is
        // (if the size isn't specified I have to get it from the
        // initializer, but that will be TODO for now)
        size = type->asArrayTypeC().size;
      }

      // remember the length
      env.addFact(new AVbinary(env.avLength(addr), BIN_EQUAL,
                               new AVint(size)));
    }

    else {
      // model the variable as a simple, named, unaliasable variable
      env.set(name, value);
    }
  }
}


// ----------------------- Statement ---------------
// for now, ignore all control flow

void S_skip::vcgen(AEnv &env) const {}
void S_label::vcgen(AEnv &env) const {}
void S_case::vcgen(AEnv &env) const {}
void S_caseRange::vcgen(AEnv &env) const {}
void S_default::vcgen(AEnv &env) const {}

void S_expr::vcgen(AEnv &env) const
{        
  // evaluate and discard
  env.discard(expr->vcgen(env));
}

void S_compound::vcgen(AEnv &env) const
{
  FOREACH_ASTLIST(Statement, stmts, iter) {
    iter.data()->vcgen(env);
  }
}

void S_if::vcgen(AEnv &env) const {}
void S_switch::vcgen(AEnv &env) const {}
void S_while::vcgen(AEnv &env) const {}
void S_doWhile::vcgen(AEnv &env) const {}
void S_for::vcgen(AEnv &env) const {}
void S_break::vcgen(AEnv &env) const {}
void S_continue::vcgen(AEnv &env) const {}

void S_return::vcgen(AEnv &env) const
{
  if (expr) {
    // model as an assignment to 'result' (and leave the
    // control flow implications for later)
    env.set(env.str("result"), expr->vcgen(env));
  }
}

void S_goto::vcgen(AEnv &env) const {}


void S_decl::vcgen(AEnv &env) const
{
  decl->vcgen(env);
}


// eval 'expr' in a predicate context
AbsValue *vcgenPredicate(AEnv &env, Expression *expr)
{
  IN_PREDICATE(env);
  return expr->vcgen(env);
}


void S_assert::vcgen(AEnv &env) const
{
  // map the expression to my abstract domain
  AbsValue *v = vcgenPredicate(env, expr);
  xassert(v);     // already checked it was boolean

  // try to prove it (is not equal to 0)
  env.prove(v, "user assertion");
}

void S_assume::vcgen(AEnv &env) const
{
  // evaluate
  AbsValue *v = vcgenPredicate(env, expr);
  xassert(v);
  
  // remember it as a known fact
  env.addFact(v);
}

void S_invariant::vcgen(AEnv &env) const
{
  // ignore for now
  cout << "ignoring invariant\n";
}

void S_thmprv::vcgen(AEnv &env) const
{
  IN_PREDICATE(env);
  s->vcgen(env);
}


// ---------------------- Expression -----------------
AbsValue *E_intLit::vcgen(AEnv &env) const
{
  return env.grab(new AVint(i));
}

AbsValue *E_floatLit::vcgen(AEnv &env) const { return avTodo(); }
AbsValue *E_stringLit::vcgen(AEnv &env) const 
{ 
  // make a new variable to stand for this string's object (address)
  AbsValue *object = env.freshVariable("str", stringc << "address of " << quoted(s));
  env.addDistinct(object);

  // assert the length of this object is the length of the string (w/ null)
  env.addFact(new AVbinary(env.avLength(object), BIN_EQUAL,
              new AVint(strlen(s)+1)));
              
  // assert that the first zero is (currently?) at the end
  env.addFact(new AVbinary(env.avFirstZero(env.getMem(), object), BIN_EQUAL,
              new AVint(strlen(s)+1)));
         
  // the result of this expression is a pointer to the string's start
  return env.avPointer(object, new AVint(0));
}


AbsValue *E_charLit::vcgen(AEnv &env) const
{
  return env.grab(new AVint(c));
}

AbsValue *E_structLit::vcgen(AEnv &env) const { return avTodo(); }

AbsValue *E_variable::vcgen(AEnv &env) const
{
  if (!env.isMemVar(name)) {
    // ordinary variable: look up the current abstract value for this
    // variable, and simply return that
    return env.get(name);
  }
  else if (type->isArrayType()) {
    // is name of an array, in which case we return the name given
    // to the array's location (it's like an immutable pointer)
    return env.avPointer(env.getMemVarAddr(name), new AVint(0));
  }
  else {
    // memory variable: return an expression which will read it out of memory
    return env.avSelect(env.getMem(), env.getMemVarAddr(name), new AVint(0));
  }
}


#if 0
AbsValue *E_arrayAcc::vcgen(AEnv &env) const
{
  AbsValue *i = index->vcgen(env);

  if (arr->isE_variable()) {
    AbsValue *object = env.getMemVarAddr(arr->asE_variable()->name);

    // emit a proof obligation for safe access
    env.prove(new AVbinary(i, BIN_GREATEREQ, new AVint(0)), "array lower bound");
    env.prove(new AVbinary(i, BIN_LESS, env.avLength(object)), "array upper bound");

    // yield whatever is in that location
    return env.avSelect(env.getMem(), object, i);
  }
  else {
    cout << "unhandled array access\n";

    return env.freshVariable("arrayAcc", 
             stringc << "array access: " << toString());
  }
}
#endif // 0


AbsValue *E_funCall::vcgen(AEnv &env) const
{
  // evaluate the argument expressions
  SObjList<AbsValue> argExps;
  FOREACH_ASTLIST(Expression, args, iter) {
    // vcgen might return NULL, that's ok for an SObjList
    argExps.prepend(iter.data()->vcgen(env));
  }
  argExps.reverse();      // when it's easy to stay linear..

  FunctionType const &ft = func->type->asFunctionTypeC();

  // --------------- predicate: fn symbol application ----------------
  if (env.inPredicate) {
    StringRef funcName = func->asE_variable()->name;
    AVfunc *f = new AVfunc(funcName, NULL);
    while (argExps.isNotEmpty()) {
      f->args.append(argExps.removeAt(0));
    }
    return f;
  }

  // -------------- build an environment for pre/post ----------------
  // the pre- and postconditions get evaluated to an abstract value in
  // an environment containing only parameters (and in the case of the
  // postcondition, 'result); eventually it should also contain
  // globals, but right now I don't have globals in the abstract
  // environment..
  AEnv newEnv(env.stringTable);
  newEnv.inPredicate = true;       // will only be used to eval pre/post

  // bind the parameters in the new environment
  SObjListMutator<AbsValue> argExpIter(argExps);
  FOREACH_OBJLIST(FunctionType::Param, ft.params, paramIter) {
    // bind the names to the expressions that are passed; only
    // bind if the expression is not NULL
    if (argExpIter.data()) {
      newEnv.set(paramIter.data()->name,
                 argExpIter.data());
    }

    // I claim that putting the arg exp into the new environment
    // counts as "using" it, so discarding it is not necessary
    // (except that it was created in 'env' and passed to 'newEnv'...)

    argExpIter.adv();
  }

  // let mem = current-mem
  newEnv.setMem(env.getMem());

  // let pre_mem = mem
  //newEnv.set(env.str("pre_mem"), newEnv.getMem());

  // ----------------- prove precondition ---------------
  if (ft.precondition) {
    // include precondition bindings in the environment
    FOREACH_ASTLIST(Declaration, ft.precondition->decls, iter) {
      iter.data()->vcgen(newEnv);
    }

    // finally, interpret the precondition in the parameter-only
    // environment, to arrive at a predicate to prove
    AbsValue *predicate = ft.precondition->expr->vcgen(newEnv);
    xassert(predicate);      // tcheck should have verified we can represent it

    // prove this predicate; the assumptions from the *old* environment
    // are relevant
    env.prove(predicate, stringc << "precondition of call: " << toString());

    // no longer needed
    newEnv.discard(predicate);
  }

  // ----------------- assume postcondition ---------------
  // make a new variable to stand for the value returned
  AbsValue *result = NULL;
  if (!ft.retType->isVoid()) {
    result = env.freshVariable("result", 
               stringc << "function call result: " << toString());

    // add this to the mini environment so if the programmer talks
    // about the result, it will state something about the result variable
    newEnv.set(env.stringTable.add("result"), result);
  }

  // make up new variable to represent final contents of memory
  env.setMem(env.freshVariable("mem", "contents of memory after call"));
  newEnv.setMem(env.getMem());

  // NOTE: by keeping the environment from before, we are interpreting
  // all references to parameters as their values in the *pre* state

  if (ft.postcondition) {
    // evaluate it
    AbsValue *predicate = ft.postcondition->expr->vcgen(newEnv);
    xassert(predicate);
    
    // assume it
    env.addFact(predicate);
  }

  // result of calling this function is the result variable
  return result;
}


AbsValue *E_fieldAcc::vcgen(AEnv &env) const
{
  // good results here would require abstract values for structures
  return env.freshVariable("field", 
           stringc << "structure field access: " << toString());
}


AbsValue *E_unary::vcgen(AEnv &env) const
{
  // TODO: deal with ++, --
  
  if (op == UNY_SIZEOF) {
    return env.grab(new AVint(expr->type->reprSize()));
  }
  else {
    return env.grab(new AVunary(op, expr->vcgen(env)));
  }
}

AbsValue *E_binary::vcgen(AEnv &env) const
{
  // TODO: deal with &&, ||
  return env.grab(new AVbinary(e1->vcgen(env), op, e2->vcgen(env)));
}


AbsValue *E_addrOf::vcgen(AEnv &env) const
{
  if (expr->isE_variable()) {                            
    return avFunc2(env.str("pointer"),
                   env.getMemVarAddr(expr->asE_variable()->name), // obj
                   new AVint(0));                                 // offset
  }
  else {
    cout << "warning: unhandled addrof: " << toString() << endl;
    return avTodo();
  }
}


AbsValue *E_deref::vcgen(AEnv &env) const
{
  AbsValue *addr = ptr->vcgen(env);

  // emit a proof obligation for safe access
  env.prove(new AVbinary(env.avOffset(addr), BIN_GREATEREQ, 
                         new AVint(0)), "array lower bound");
  env.prove(new AVbinary(env.avOffset(addr), BIN_LESS,
                         env.avLength(env.avObject(addr))), "array upper bound");

  return env.avSelect(env.getMem(), env.avObject(addr), env.avOffset(addr));
}


AbsValue *E_cast::vcgen(AEnv &env) const
{
  // I don't know what account I should take of casts..
  return expr->vcgen(env);
}


AbsValue *E_cond::vcgen(AEnv &env) const
{
  // for now I'll assume there are no side effects in the branches;
  // same assumption applies to '&&' and '||' above
  return env.grab(new AVcond(cond->vcgen(env), th->vcgen(env), el->vcgen(env)));
}


AbsValue *E_gnuCond::vcgen(AEnv &env) const
{
  // again on the assumption there are no side effects in the 'else' branch
  // TODO: make a deep-copier for astgen
  return avTodo();
}


AbsValue *E_comma::vcgen(AEnv &env) const
{
  // evaluate and discard
  env.discard(e1->vcgen(env));

  // evaluate and keep
  return e2->vcgen(env);
}


AbsValue *E_sizeofType::vcgen(AEnv &env) const
{
  return env.grab(new AVint(size));
}


AbsValue *E_assign::vcgen(AEnv &env) const
{
  AbsValue *v = src->vcgen(env);

  // since I have no reasonable hold on pointers yet, I'm only
  // going to keep track of assignments to (integer) variables
  if (target->isE_variable()) {
    StringRef name = target->asE_variable()->name;
                                                  
    if (!env.isMemVar(name)) {
      // ordinary variable: replace the mapping
      env.set(name, v);
    }
    else {
      // memory variable: memory changes    
      env.setMem(env.avUpdate(env.getMem(), env.getMemVarAddr(name), 
                              new AVint(0) /*offset*/, v));
    }
  }
  else if (target->isE_deref()) {
    // get an abstract value for the address being modified
    AbsValue *addr = target->asE_deref()->ptr->vcgen(env);

    // emit a proof obligation for safe access
    env.prove(new AVbinary(env.avOffset(addr), BIN_GREATEREQ,
                           new AVint(0)), "array lower bound");
    env.prove(new AVbinary(env.avOffset(addr), BIN_LESS,
                           env.avLength(env.avObject(addr))), "array upper bound");

    // memory changes
    env.setMem(env.avUpdate(env.getMem(), env.avObject(addr),
                            env.avOffset(addr), v));
  }
  else {
    cout << "warning: unhandled assignment: " << toString() << endl;
  }

  return env.dup(v);
}


AbsValue *E_arithAssign::vcgen(AEnv &env) const
{
  AbsValue *v = src->vcgen(env);

  // again, only support variables
  if (target->isE_variable()) {
    StringRef name = target->asE_variable()->name;

    // extract old value
    AbsValue *old = env.get(name);
    
    // combine it
    v = env.grab(new AVbinary(old, op, v));
    
    // replace in environment
    env.set(name, v);

    return env.dup(v);
  }
  else {
    return v;
  }
}


// --------------------- Initializer --------------------
AbsValue *IN_expr::vcgen(AEnv &env, Type const *) const
{
  return e->vcgen(env);
}


AbsValue *IN_compound::vcgen(AEnv &/*env*/, Type const */*type*/) const
{
  // I have no representation for array and structure values
  return avTodo();
}


