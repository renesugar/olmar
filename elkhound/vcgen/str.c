// str.c
// trying some str* functions
// NUMERRORS 1


//int *mem;
int offset(int *ptr);
int *object(int *ptr);
int firstZero(int *mem, int *obj);
int length(int *obj);
int *changed(int *mem, int *obj);



void strcpy(char *dest, char const *src)
  thmprv_pre
    // bind a name to the pre-state's memory
    int *pre_mem = mem;

    // src points to a string suitable for reading
    0 <= offset(src) && offset(src) <= firstZero(mem, object(src)) &&

    // dest points to a writable buffer
    0 <= offset(dest) && offset(dest) < length(object(dest)) &&

    // src and dest should not overlap
    object(dest) != object(src) &&

    // the # of bytes we will copy from src will fit into dest
    // (not counting the null, but using '<', I avoid a "+1")
    (firstZero(mem, object(src)) - offset(src)) <  // # of bytes to copy (not incl. null)
      (length(object(dest)) - offset(dest))        // space remaining
    ;

  thmprv_post
    // memory will change, but only in 'dest'
    mem == changed(pre_mem, object(dest)) &&

    // the string in dest will have the same length as the string in src
    (firstZero(mem, object(dest)) - offset(dest)) ==
      (firstZero(mem, object(src)) - offset(src))
                   // could also say pre_mem  ^^^; above assertion verifies they are same
    ;
;


void strcat(char *dest, char const *src)
  thmprv_pre
    // bind a name to the pre-state's memory
    int *pre_mem = mem;

    // src points to a string suitable for reading
    0 <= offset(src) && offset(src) <= firstZero(mem, object(src)) &&

    // dest points to a writable buffer
    0 <= offset(dest) && offset(dest) < length(object(dest)) &&

    // src and dest should not overlap (?)
    object(dest) != object(src) &&

    // the # of bytes we will copy from src will fit into
    // the space after dest's firstZero;
    // offby1 reasoning: if the top term is 1, it means both a char
    // and null will be copied; if the bottom term is 1 it means there
    // is no room left (b/c firstZero is an index)
    (firstZero(mem, object(src)) - offset(src)) <    // # of bytes to copy (not incl. null)
      (length(object(dest)) - firstZero(mem, object(dest)))  // space after 0
    ;

  thmprv_post
    // memory will change, but only in 'dest'
    mem == changed(pre_mem, object(dest)) &&

    // the string in dest will grow by the length of the string in src
    (firstZero(mem, object(dest)) - offset(dest)) ==
      (firstZero(pre_mem, object(src)) + (firstZero(mem, object(src)) - offset(src)))
                        // could also say pre_mem  ^^^; above assertion verifies they are same
    ;
;


void foo()
{
  char buf[80];
  strcpy(buf, "hello there");
  strcat(buf, " and how are you?");
  strcat(buf, " but this is now certainly too long for an 80-char buffer");  // ERROR(1)
}
