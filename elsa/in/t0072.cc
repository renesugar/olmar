// cc.in72
// declmodifiers after type specifier word

const static int x = 4;

//ERROR1: const const int q;    // duplicate 'const'

unsigned int r;


// some more hairy examples
long unsigned y;
const unsigned volatile long static int z;

long long LL;    // support this because my libc headers use it..

// may as well get the literal notation too
void foo()
{
  LL = 12LL;
  LL = -1LL;
}


//ERROR3: long float g;    // malformed type
                       
// too many!
//ERROR4: long long long LLL;
//ERROR5: long long long long LLLL;
//ERROR6: long long long long long LLLL;

