// clash between function and instantiated template parameter member

// error: during argument-dependent lookup of `foo', found non-function of
// type `int' in class B at 0x82258b8

class B { int foo; };
template <class T> class A { };
typedef A<B> C;

int foo(C *);

void bar() {
    C* c;
    foo(c);
}
