// t0157.cc
// very simple namespace example

asm("collectLookupResults x 7:7");

namespace N {
  int x /*7:7*/;
}

int f()
{
  return N::x;
}
