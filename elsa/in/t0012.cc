// cc.in12
// experiments with qualifiers

class Foo {
public:
  static int x;
  int y;
  
  int func();
  int func2();
};

// qualifiers on a declarator, so it refers to
// something which has already been declared
int Foo::x = 5;

// violation of ODR
//ERROR1: int Foo::x = 5;

// can't define nonstatic data members
//ERROR2: int Foo::y = 7;


int main()
{
  // qualifiers on an E_variable
  return Foo::x;
}


int Foo::func()
{
  return x;    // requires that scope includes Foo's variables
}


//ERROR3: int Foo::func() {  return 18; }

//ERROR4: void Foo::func2() {}


// I think this doesn't work during printing..
int foo(Foo * const ths);

// what about this?
Foo * const arf;
