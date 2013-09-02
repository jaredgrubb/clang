// RUN: %clang_cc1 -analyze -analyzer-checker=core,unix.cstring,alpha.unix.cstring,debug.ExprInspection -analyzer-store=region -Wno-null-dereference -verify %s
// RUN: %clang_cc1 -analyze -DUSE_BUILTINS -analyzer-checker=core,unix.cstring,alpha.unix.cstring,debug.ExprInspection -analyzer-store=region -Wno-null-dereference -verify %s
// RUN: %clang_cc1 -analyze -DVARIANT -analyzer-checker=core,unix.cstring,alpha.unix.cstring,debug.ExprInspection -analyzer-store=region -Wno-null-dereference -verify %s
// RUN: %clang_cc1 -analyze -DUSE_BUILTINS -DVARIANT -analyzer-checker=alpha.security.taint,core,unix.cstring,alpha.unix.cstring,debug.ExprInspection -analyzer-store=region -Wno-null-dereference -verify %s

//===----------------------------------------------------------------------===
// Declarations
//===----------------------------------------------------------------------===

// Some functions are so similar to each other that they follow the same code
// path, such as memcpy and __memcpy_chk, or memcmp and bcmp. If VARIANT is
// defined, make sure to use the variants instead to make sure they are still
// checked by the analyzer.

// Some functions are implemented as builtins. These should be #defined as
// BUILTIN(f), which will prepend "__builtin_" if USE_BUILTINS is defined.

// Functions that have variants and are also available as builtins should be
// declared carefully! See memcpy() for an example.

#ifdef USE_BUILTINS
# define BUILTIN(f) __builtin_ ## f
#else /* USE_BUILTINS */
# define BUILTIN(f) f
#endif /* USE_BUILTINS */

#define NULL 0
typedef typeof(sizeof(int)) size_t;

void clang_analyzer_eval(int);

int scanf(const char *restrict format, ...);

//===----------------------------------------------------------------------===
// strlen()
//===----------------------------------------------------------------------===

#define strlen BUILTIN(strlen)
size_t strlen(const char *s);

void strlen_constant0() {
  clang_analyzer_eval(strlen("123") == 3); // expected-warning{{TRUE}}
}

void strlen_constant1() {
  const char *a = "123";
  clang_analyzer_eval(strlen(a) == 3); // expected-warning{{TRUE}}
}

void strlen_constant2(char x) {
  char b[] = "zyx";
  clang_analyzer_eval(strlen(b) == 3); // expected-warning{{TRUE}}

  b[0] = x;
  clang_analyzer_eval(strlen(b) == 3); // expected-warning{{UNKNOWN}}
}

void strlen_constant_with_offset0() {
  char a[] = "123";
  clang_analyzer_eval(strlen(a+0) == 3); // expected-warning{{TRUE}}
}

void strlen_constant_with_embedded_null1() {
  char a[] = "123\00045";
  clang_analyzer_eval(strlen(a) == 3); // expected-warning{{TRUE}}
}

void strlen_constant_with_embedded_null2() {
  const char *a = "123\00045";
  clang_analyzer_eval(strlen(a) == 3); // expected-warning{{TRUE}}
  clang_analyzer_eval(strlen(a+1) == 2); // expected-warning{{TRUE}}
  clang_analyzer_eval(strlen(a+3) == 0); // expected-warning{{TRUE}}
  clang_analyzer_eval(strlen(a+4) == 2); // expected-warning{{TRUE}}
  clang_analyzer_eval(strlen(a+6) == 0); // expected-warning{{TRUE}}
}

size_t strlen_null() {
  return strlen(0); // expected-warning{{Null pointer argument in call to string length function}}
}

size_t strlen_stringliteral_neg_index() {
  const char *a = "123";
  return strlen(a-1); // expected-warning{{Array index '-1' is a negative index into a string literal}}
}

size_t strlen_fn() {
  return strlen((char*)&strlen_fn); // expected-warning{{Argument to string length function is the address of the function 'strlen_fn', which is not a null-terminated string}}
}

size_t strlen_nonloc() {
label:
  return strlen((char*)&&label); // expected-warning{{Argument to string length function is the address of the label 'label', which is not a null-terminated string}}
}

void strlen_subregion() {
  struct two_strings { char a[2], b[2]; };
  extern void use_two_strings(struct two_strings *);

  struct two_strings z;
  use_two_strings(&z);

  size_t a = strlen(z.a);
  z.b[0] = 5;
  size_t b = strlen(z.a);
  if (a == 0)
    clang_analyzer_eval(b == 0); // expected-warning{{TRUE}}

  use_two_strings(&z);

  size_t c = strlen(z.a);
  if (a == 0)
    clang_analyzer_eval(c == 0); // expected-warning{{UNKNOWN}}
}

extern void use_string(char *);
void strlen_argument(char *x) {
  size_t a = strlen(x);
  size_t b = strlen(x);
  if (a == 0)
    clang_analyzer_eval(b == 0); // expected-warning{{TRUE}}

  use_string(x);

  size_t c = strlen(x);
  if (a == 0)
    clang_analyzer_eval(c == 0); // expected-warning{{UNKNOWN}}
}

extern char global_str[];
void strlen_global() {
  size_t a = strlen(global_str);
  size_t b = strlen(global_str);
  if (a == 0) {
    clang_analyzer_eval(b == 0); // expected-warning{{TRUE}}
    // Make sure clang_analyzer_eval does not invalidate globals.
    clang_analyzer_eval(strlen(global_str) == 0); // expected-warning{{TRUE}}    
  }

  // Call a function with unknown effects, which should invalidate globals.
  use_string(0);

  size_t c = strlen(global_str);
  if (a == 0)
    clang_analyzer_eval(c == 0); // expected-warning{{UNKNOWN}}
}

void strlen_indirect(char *x) {
  size_t a = strlen(x);
  char *p = x;
  char **p2 = &p;
  size_t b = strlen(x);
  if (a == 0)
    clang_analyzer_eval(b == 0); // expected-warning{{TRUE}}

  extern void use_string_ptr(char*const*);
  use_string_ptr(p2);

  size_t c = strlen(x);
  if (a == 0)
    clang_analyzer_eval(c == 0); // expected-warning{{UNKNOWN}}
}

void strlen_indirect2(char *x) {
  size_t a = strlen(x);
  char *p = x;
  char **p2 = &p;
  extern void use_string_ptr2(char**);
  use_string_ptr2(p2);

  size_t c = strlen(x);
  if (a == 0)
    clang_analyzer_eval(c == 0); // expected-warning{{UNKNOWN}}
}

void strlen_liveness(const char *x) {
  if (strlen(x) < 5)
    return;
  clang_analyzer_eval(strlen(x) < 5); // expected-warning{{FALSE}}
}

//===----------------------------------------------------------------------===
// strnlen()
//===----------------------------------------------------------------------===

size_t strnlen(const char *s, size_t maxlen);

void strnlen_constant0() {
  clang_analyzer_eval(strnlen("123", 10) == 3); // expected-warning{{TRUE}}
}

void strnlen_constant1() {
  const char *a = "123";
  clang_analyzer_eval(strnlen(a, 10) == 3); // expected-warning{{TRUE}}
}

void strnlen_constant2(char x) {
  char a[] = "123";
  clang_analyzer_eval(strnlen(a, 10) == 3); // expected-warning{{TRUE}}
  a[0] = x;
  clang_analyzer_eval(strnlen(a, 10) == 3); // expected-warning{{UNKNOWN}}
}

void strnlen_constant4() {
  clang_analyzer_eval(strnlen("123456", 3) == 3); // expected-warning{{TRUE}}
}

void strnlen_constant5() {
  const char *a = "123456";
  clang_analyzer_eval(strnlen(a, 3) == 3); // expected-warning{{TRUE}}
}

void strnlen_constant6(char x) {
  char a[] = "123456";
  clang_analyzer_eval(strnlen(a, 3) == 3); // expected-warning{{TRUE}}
  a[0] = x;
  clang_analyzer_eval(strnlen(a, 3) == 3); // expected-warning{{UNKNOWN}}
}

size_t strnlen_null() {
  return strnlen(0, 3); // expected-warning{{Null pointer argument in call to string length function}}
}

size_t strnlen_fn() {
  return strnlen((char*)&strlen_fn, 3); // expected-warning{{Argument to string length function is the address of the function 'strlen_fn', which is not a null-terminated string}}
}

size_t strnlen_nonloc() {
label:
  return strnlen((char*)&&label, 3); // expected-warning{{Argument to string length function is the address of the label 'label', which is not a null-terminated string}}
}

void strnlen_zero() {
  clang_analyzer_eval(strnlen("abc", 0) == 0); // expected-warning{{TRUE}}
  clang_analyzer_eval(strnlen(NULL, 0) == 0); // expected-warning{{TRUE}}
}

size_t strnlen_compound_literal() {
  // This used to crash because we don't model the string lengths of
  // compound literals.
  return strnlen((char[]) { 'a', 'b', 0 }, 1);
}

size_t strnlen_unknown_limit(float f) {
  // This used to crash because we don't model the integer values of floats.
  return strnlen("abc", (int)f);
}

void strnlen_is_not_strlen(char *x) {
  clang_analyzer_eval(strnlen(x, 10) == strlen(x)); // expected-warning{{UNKNOWN}}
}

void strnlen_at_limit(char *x) {
  size_t len = strnlen(x, 10);
  clang_analyzer_eval(len <= 10); // expected-warning{{TRUE}}
  clang_analyzer_eval(len == 10); // expected-warning{{UNKNOWN}}
  clang_analyzer_eval(len < 10); // expected-warning{{UNKNOWN}}
}

void strnlen_at_actual(size_t limit) {
  size_t len = strnlen("abc", limit);
  clang_analyzer_eval(len <= 3); // expected-warning{{TRUE}}
  // This is due to eager assertion in strnlen.
  if (limit == 0) {
    clang_analyzer_eval(len == 0); // expected-warning{{TRUE}}
  } else {
    clang_analyzer_eval(len == 3); // expected-warning{{UNKNOWN}}
    clang_analyzer_eval(len < 3); // expected-warning{{UNKNOWN}}
  }
}

//===----------------------------------------------------------------------===
// strcpy()
//===----------------------------------------------------------------------===

#ifdef VARIANT

#define __strcpy_chk BUILTIN(__strcpy_chk)
char *__strcpy_chk(char *restrict s1, const char *restrict s2, size_t destlen);

#define strcpy(a,b) __strcpy_chk(a,b,(size_t)-1)

#else /* VARIANT */

#define strcpy BUILTIN(strcpy)
char *strcpy(char *restrict s1, const char *restrict s2);

#endif /* VARIANT */


void strcpy_null_dst(char *x) {
  strcpy(NULL, x); // expected-warning{{Null pointer argument in call to string copy function}}
}

void strcpy_null_src(char *x) {
  strcpy(x, NULL); // expected-warning{{Null pointer argument in call to string copy function}}
}

void strcpy_fn(char *x) {
  strcpy(x, (char*)&strcpy_fn); // expected-warning{{Argument to string copy function is the address of the function 'strcpy_fn', which is not a null-terminated string}}
}

void strcpy_fn_const(char *x) {
  strcpy(x, (const char*)&strcpy_fn); // expected-warning{{Argument to string copy function is the address of the function 'strcpy_fn', which is not a null-terminated string}}
}

extern int globalInt;
void strcpy_effects(char *x, char *y) {
  char a = x[0];
  if (globalInt != 42)
    return;

  clang_analyzer_eval(strcpy(x, y) == x); // expected-warning{{TRUE}}
  clang_analyzer_eval(strlen(x) == strlen(y)); // expected-warning{{TRUE}}
  clang_analyzer_eval(a == x[0]); // expected-warning{{UNKNOWN}}
  clang_analyzer_eval(globalInt == 42); // expected-warning{{TRUE}}
}

void strcpy_overflow(char *y) {
  char x[4];
  if (strlen(y) == 4)
    strcpy(x, y); // expected-warning{{String copy function overflows destination buffer}}
}

void strcpy_no_overflow(char *y) {
  char x[4];
  if (strlen(y) == 3)
    strcpy(x, y); // no-warning
}

//===----------------------------------------------------------------------===
// stpcpy()
//===----------------------------------------------------------------------===

#ifdef VARIANT

#define __stpcpy_chk BUILTIN(__stpcpy_chk)
char *__stpcpy_chk(char *restrict s1, const char *restrict s2, size_t destlen);

#define stpcpy(a,b) __stpcpy_chk(a,b,(size_t)-1)

#else /* VARIANT */

#define stpcpy BUILTIN(stpcpy)
char *stpcpy(char *restrict s1, const char *restrict s2);

#endif /* VARIANT */


void stpcpy_effect(char *x, char *y) {
  char a = x[0];

  clang_analyzer_eval(stpcpy(x, y) == &x[strlen(y)]); // expected-warning{{TRUE}}
  clang_analyzer_eval(strlen(x) == strlen(y)); // expected-warning{{TRUE}}
  clang_analyzer_eval(a == x[0]); // expected-warning{{UNKNOWN}}
}

void stpcpy_overflow(char *y) {
  char x[4];
  if (strlen(y) == 4)
    stpcpy(x, y); // expected-warning{{String copy function overflows destination buffer}}
}

void stpcpy_no_overflow(char *y) {
  char x[4];
  if (strlen(y) == 3)
    stpcpy(x, y); // no-warning
}

//===----------------------------------------------------------------------===
// strcat()
//===----------------------------------------------------------------------===

#ifdef VARIANT

#define __strcat_chk BUILTIN(__strcat_chk)
char *__strcat_chk(char *restrict s1, const char *restrict s2, size_t destlen);

#define strcat(a,b) __strcat_chk(a,b,(size_t)-1)

#else /* VARIANT */

#define strcat BUILTIN(strcat)
char *strcat(char *restrict s1, const char *restrict s2);

#endif /* VARIANT */


void strcat_null_dst(char *x) {
  strcat(NULL, x); // expected-warning{{Null pointer argument in call to string copy function}}
}

void strcat_null_src(char *x) {
  strcat(x, NULL); // expected-warning{{Null pointer argument in call to string copy function}}
}

void strcat_fn(char *x) {
  strcat(x, (char*)&strcat_fn); // expected-warning{{Argument to string copy function is the address of the function 'strcat_fn', which is not a null-terminated string}}
}

void strcat_effects(char *y) {
  char x[8] = "123";
  size_t orig_len = strlen(x);
  char a = x[0];

  if (strlen(y) != 4)
    return;

  clang_analyzer_eval(strcat(x, y) == x); // expected-warning{{TRUE}}
  clang_analyzer_eval((int)strlen(x) == (orig_len + strlen(y))); // expected-warning{{TRUE}}
}

void strcat_overflow_0(char *y) {
  char x[4] = "12";
  if (strlen(y) == 4)
    strcat(x, y); // expected-warning{{String copy function overflows destination buffer}}
}

void strcat_overflow_1(char *y) {
  char x[4] = "12";
  if (strlen(y) == 3)
    strcat(x, y); // expected-warning{{String copy function overflows destination buffer}}
}

void strcat_overflow_2(char *y) {
  char x[4] = "12";
  if (strlen(y) == 2)
    strcat(x, y); // expected-warning{{String copy function overflows destination buffer}}
}

void strcat_no_overflow(char *y) {
  char x[5] = "12";
  if (strlen(y) == 2)
    strcat(x, y); // no-warning
}

void strcat_symbolic_dst_length(char *dst) {
	strcat(dst, "1234");
  clang_analyzer_eval(strlen(dst) >= 4); // expected-warning{{TRUE}}
}

void strcat_symbolic_dst_length_taint(char *dst) {
  scanf("%s", dst); // Taint data.
  strcat(dst, "1234");
  clang_analyzer_eval(strlen(dst) >= 4); // expected-warning{{TRUE}}
}

void strcat_unknown_src_length(char *src, int offset) {
	char dst[8] = "1234";
	strcat(dst, &src[offset]);
  clang_analyzer_eval(strlen(dst) >= 4); // expected-warning{{TRUE}}
}

// There is no strcat_unknown_dst_length because if we can't get a symbolic
// length for the "before" strlen, we won't be able to set one for "after".

void strcat_too_big(char *dst, char *src) {
  // We assume this can never actually happen, so we don't get a warning.
	if (strlen(dst) != (((size_t)0) - 2))
		return;
	if (strlen(src) != 2)
		return;
	strcat(dst, src);
}


//===----------------------------------------------------------------------===
// strncpy()
//===----------------------------------------------------------------------===

#ifdef VARIANT

#define __strncpy_chk BUILTIN(__strncpy_chk)
char *__strncpy_chk(char *restrict s1, const char *restrict s2, size_t n, size_t destlen);

#define strncpy(a,b,n) __strncpy_chk(a,b,n,(size_t)-1)

#else /* VARIANT */

#define strncpy BUILTIN(strncpy)
char *strncpy(char *restrict s1, const char *restrict s2, size_t n);

#endif /* VARIANT */


void strncpy_null_dst(char *x) {
  strncpy(NULL, x, 5); // expected-warning{{Null pointer argument in call to string copy function}}
}

void strncpy_null_src(char *x) {
  strncpy(x, NULL, 5); // expected-warning{{Null pointer argument in call to string copy function}}
}

void strncpy_fn(char *x) {
  strncpy(x, (char*)&strcpy_fn, 5); // expected-warning{{Argument to string copy function is the address of the function 'strcpy_fn', which is not a null-terminated string}}
}

void strncpy_effects(char *x, char *y) {
  char a = x[0];

  clang_analyzer_eval(strncpy(x, y, 5) == x); // expected-warning{{TRUE}}
  clang_analyzer_eval(strlen(x) == strlen(y)); // expected-warning{{UNKNOWN}}
  clang_analyzer_eval(a == x[0]); // expected-warning{{UNKNOWN}}
}

void strncpy_overflow(char *y) {
  char x[4];
  if (strlen(y) == 4)
    strncpy(x, y, 5); // expected-warning{{Size argument is greater than the length of the destination buffer}}
}

void strncpy_no_overflow(char *y) {
  char x[4];
  if (strlen(y) == 3)
    strncpy(x, y, 5); // expected-warning{{Size argument is greater than the length of the destination buffer}}
}

void strncpy_no_overflow2(char *y, int n) {
	if (n <= 4)
		return;

  char x[4];
  if (strlen(y) == 3)
    strncpy(x, y, n); // expected-warning{{Size argument is greater than the length of the destination buffer}}
}

void strncpy_truncate(char *y) {
  char x[4];
  if (strlen(y) == 4)
    strncpy(x, y, 3); // no-warning
}

void strncpy_no_truncate(char *y) {
  char x[4];
  if (strlen(y) == 3)
    strncpy(x, y, 3); // no-warning
}

void strncpy_exactly_matching_buffer(char *y) {
	char x[4];
	strncpy(x, y, 4); // no-warning

	// strncpy does not null-terminate, so we have no idea what the strlen is
	// after this.
  clang_analyzer_eval(strlen(x) > 4); // expected-warning{{UNKNOWN}}
}

void strncpy_zero(char *src) {
  char dst[] = "123";
  strncpy(dst, src, 0); // no-warning
}

void strncpy_empty() {
  char dst[] = "123";
  char src[] = "";
  strncpy(dst, src, 4); // no-warning
}

//===----------------------------------------------------------------------===
// strncat()
//===----------------------------------------------------------------------===

#ifdef VARIANT

#define __strncat_chk BUILTIN(__strncat_chk)
char *__strncat_chk(char *restrict s1, const char *restrict s2, size_t n, size_t destlen);

#define strncat(a,b,c) __strncat_chk(a,b,c, (size_t)-1)

#else /* VARIANT */

#define strncat BUILTIN(strncat)
char *strncat(char *restrict s1, const char *restrict s2, size_t n);

#endif /* VARIANT */


void strncat_null_dst(char *x) {
  strncat(NULL, x, 4); // expected-warning{{Null pointer argument in call to string copy function}}
}

void strncat_null_src(char *x) {
  strncat(x, NULL, 4); // expected-warning{{Null pointer argument in call to string copy function}}
}

void strncat_fn(char *x) {
  strncat(x, (char*)&strncat_fn, 4); // expected-warning{{Argument to string copy function is the address of the function 'strncat_fn', which is not a null-terminated string}}
}

void strncat_effects(char *y) {
  char x[8] = "123";
  size_t orig_len = strlen(x);
  char a = x[0];

  if (strlen(y) != 4)
    return;

  clang_analyzer_eval(strncat(x, y, strlen(y)) == x); // expected-warning{{TRUE}}
  clang_analyzer_eval(strlen(x) == (orig_len + strlen(y))); // expected-warning{{TRUE}}
}

void strncat_overflow_0(char *y) {
  char x[4] = "12";
  if (strlen(y) == 4)
    strncat(x, y, strlen(y)); // expected-warning{{Size argument is greater than the free space in the destination buffer}}
}

void strncat_overflow_1(char *y) {
  char x[4] = "12";
  if (strlen(y) == 3)
    strncat(x, y, strlen(y)); // expected-warning{{Size argument is greater than the free space in the destination buffer}}
}

void strncat_overflow_2(char *y) {
  char x[4] = "12";
  if (strlen(y) == 2)
    strncat(x, y, strlen(y)); // expected-warning{{Size argument is greater than the free space in the destination buffer}}
}

void strncat_overflow_3(char *y) {
  char x[4] = "12";
  if (strlen(y) == 4)
    strncat(x, y, 2); // expected-warning{{Size argument is greater than the free space in the destination buffer}}
}
void strncat_no_overflow_1(char *y) {
  char x[5] = "12";
  if (strlen(y) == 2)
    strncat(x, y, strlen(y)); // no-warning
}

void strncat_no_overflow_2(char *y) {
  char x[4] = "12";
  if (strlen(y) == 4)
    strncat(x, y, 1); // no-warning
}

void strncat_symbolic_dst_length(char *dst) {
  strncat(dst, "1234", 5);
  clang_analyzer_eval(strlen(dst) >= 4); // expected-warning{{TRUE}}
}

void strncat_symbolic_src_length(char *src) {
  char dst[8] = "1234";
  strncat(dst, src, 3);
  clang_analyzer_eval(strlen(dst) >= 4); // expected-warning{{TRUE}}

  char dst2[8] = "1234";
  strncat(dst2, src, 4); // expected-warning{{Size argument is greater than the free space in the destination buffer}}
}

void strncat_unknown_src_length(char *src, int offset) {
  char dst[8] = "1234";
  strncat(dst, &src[offset], 3);
  clang_analyzer_eval(strlen(dst) >= 4); // expected-warning{{TRUE}}

  char dst2[8] = "1234";
  strncat(dst2, &src[offset], 4); // expected-warning{{Size argument is greater than the free space in the destination buffer}}
}

// There is no strncat_unknown_dst_length because if we can't get a symbolic
// length for the "before" strlen, we won't be able to set one for "after".

void strncat_symbolic_limit(unsigned limit) {
  char dst[6] = "1234";
  char src[] = "567";
  strncat(dst, src, limit); // no-warning

  clang_analyzer_eval(strlen(dst) >= 4); // expected-warning{{TRUE}}
  clang_analyzer_eval(strlen(dst) == 4); // expected-warning{{UNKNOWN}}
}

void strncat_unknown_limit(float limit) {
  char dst[6] = "1234";
  char src[] = "567";
  strncat(dst, src, (size_t)limit); // no-warning

  clang_analyzer_eval(strlen(dst) >= 4); // expected-warning{{TRUE}}
  clang_analyzer_eval(strlen(dst) == 4); // expected-warning{{UNKNOWN}}
}

void strncat_too_big(char *dst, char *src) {
  // We assume this will never actually happen, so we don't get a warning.
  if (strlen(dst) != (((size_t)0) - 2))
    return;
  if (strlen(src) != 2)
    return;
  strncat(dst, src, 2);
}

void strncat_zero(char *src) {
  char dst[] = "123";
  strncat(dst, src, 0); // no-warning
}

void strncat_empty() {
  char dst[8] = "123";
  char src[] = "";
  strncat(dst, src, 4); // no-warning
}

//===----------------------------------------------------------------------===
// strcmp()
//===----------------------------------------------------------------------===

#define strcmp BUILTIN(strcmp)
int strcmp(const char * s1, const char * s2);

void strcmp_constant0() {
  clang_analyzer_eval(strcmp("123", "123") == 0); // expected-warning{{TRUE}}
}

void strcmp_constant_and_var_0() {
  char *x = "123";
  clang_analyzer_eval(strcmp(x, "123") == 0); // expected-warning{{TRUE}}
}

void strcmp_constant_and_var_1() {
  char *x = "123";
  clang_analyzer_eval(strcmp("123", x) == 0); // expected-warning{{TRUE}}
}

void strcmp_0() {
  char *x = "123";
  char *y = "123";
  clang_analyzer_eval(strcmp(x, y) == 0); // expected-warning{{TRUE}}
}

void strcmp_1() {
  char *x = "234";
  char *y = "123";
  clang_analyzer_eval(strcmp(x, y) == 1); // expected-warning{{TRUE}}
}

void strcmp_2() {
  char *x = "123";
  char *y = "234";
  clang_analyzer_eval(strcmp(x, y) == -1); // expected-warning{{TRUE}}
}

void strcmp_null_0() {
  char *x = NULL;
  char *y = "123";
  strcmp(x, y); // expected-warning{{Null pointer argument in call to string comparison function}}
}

void strcmp_null_1() {
  char *x = "123";
  char *y = NULL;
  strcmp(x, y); // expected-warning{{Null pointer argument in call to string comparison function}}
}

void strcmp_diff_length_0() {
  char *x = "12345";
  char *y = "234";
  clang_analyzer_eval(strcmp(x, y) == -1); // expected-warning{{TRUE}}
}

void strcmp_diff_length_1() {
  char *x = "123";
  char *y = "23456";
  clang_analyzer_eval(strcmp(x, y) == -1); // expected-warning{{TRUE}}
}

void strcmp_diff_length_2() {
  char *x = "12345";
  char *y = "123";
  clang_analyzer_eval(strcmp(x, y) == 1); // expected-warning{{TRUE}}
}

void strcmp_diff_length_3() {
  char *x = "123";
  char *y = "12345";
  clang_analyzer_eval(strcmp(x, y) == -1); // expected-warning{{TRUE}}
}

void strcmp_embedded_null () {
	clang_analyzer_eval(strcmp("\0z", "\0y") == 0); // expected-warning{{TRUE}}
}

void strcmp_unknown_arg (char *unknown) {
	clang_analyzer_eval(strcmp(unknown, unknown) == 0); // expected-warning{{TRUE}}
}

//===----------------------------------------------------------------------===
// strncmp()
//===----------------------------------------------------------------------===

#define strncmp BUILTIN(strncmp)
int strncmp(const char *s1, const char *s2, size_t n);

void strncmp_constant0() {
  clang_analyzer_eval(strncmp("123", "123", 3) == 0); // expected-warning{{TRUE}}
}

void strncmp_constant_and_var_0() {
  char *x = "123";
  clang_analyzer_eval(strncmp(x, "123", 3) == 0); // expected-warning{{TRUE}}
}

void strncmp_constant_and_var_1() {
  char *x = "123";
  clang_analyzer_eval(strncmp("123", x, 3) == 0); // expected-warning{{TRUE}}
}

void strncmp_0() {
  char *x = "123";
  char *y = "123";
  clang_analyzer_eval(strncmp(x, y, 3) == 0); // expected-warning{{TRUE}}
}

void strncmp_1() {
  char *x = "234";
  char *y = "123";
  clang_analyzer_eval(strncmp(x, y, 3) == 1); // expected-warning{{TRUE}}
}

void strncmp_2() {
  char *x = "123";
  char *y = "234";
  clang_analyzer_eval(strncmp(x, y, 3) == -1); // expected-warning{{TRUE}}
}

void strncmp_null_0() {
  char *x = NULL;
  char *y = "123";
  strncmp(x, y, 3); // expected-warning{{Null pointer argument in call to string comparison function}}
}

void strncmp_null_1() {
  char *x = "123";
  char *y = NULL;
  strncmp(x, y, 3); // expected-warning{{Null pointer argument in call to string comparison function}}
}

void strncmp_diff_length_0() {
  char *x = "12345";
  char *y = "234";
  clang_analyzer_eval(strncmp(x, y, 5) == -1); // expected-warning{{TRUE}}
}

void strncmp_diff_length_1() {
  char *x = "123";
  char *y = "23456";
  clang_analyzer_eval(strncmp(x, y, 5) == -1); // expected-warning{{TRUE}}
}

void strncmp_diff_length_2() {
  char *x = "12345";
  char *y = "123";
  clang_analyzer_eval(strncmp(x, y, 5) == 1); // expected-warning{{TRUE}}
}

void strncmp_diff_length_3() {
  char *x = "123";
  char *y = "12345";
  clang_analyzer_eval(strncmp(x, y, 5) == -1); // expected-warning{{TRUE}}
}

void strncmp_diff_length_4() {
  char *x = "123";
  char *y = "12345";
  clang_analyzer_eval(strncmp(x, y, 3) == 0); // expected-warning{{TRUE}}
}

void strncmp_diff_length_5() {
  char *x = "012";
  char *y = "12345";
  clang_analyzer_eval(strncmp(x, y, 3) == -1); // expected-warning{{TRUE}}
}

void strncmp_diff_length_6() {
  char *x = "234";
  char *y = "12345";
  clang_analyzer_eval(strncmp(x, y, 3) == 1); // expected-warning{{TRUE}}
}

void strncmp_embedded_null () {
	clang_analyzer_eval(strncmp("ab\0zz", "ab\0yy", 4) == 0); // expected-warning{{TRUE}}
}

//===----------------------------------------------------------------------===
// strcasecmp()
//===----------------------------------------------------------------------===

#define strcasecmp BUILTIN(strcasecmp)
int strcasecmp(const char *s1, const char *s2);

void strcasecmp_constant0() {
  clang_analyzer_eval(strcasecmp("abc", "Abc") == 0); // expected-warning{{TRUE}}
}

void strcasecmp_constant_and_var_0() {
  char *x = "abc";
  clang_analyzer_eval(strcasecmp(x, "Abc") == 0); // expected-warning{{TRUE}}
}

void strcasecmp_constant_and_var_1() {
  char *x = "abc";
  clang_analyzer_eval(strcasecmp("Abc", x) == 0); // expected-warning{{TRUE}}
}

void strcasecmp_0() {
  char *x = "abc";
  char *y = "Abc";
  clang_analyzer_eval(strcasecmp(x, y) == 0); // expected-warning{{TRUE}}
}

void strcasecmp_1() {
  char *x = "Bcd";
  char *y = "abc";
  clang_analyzer_eval(strcasecmp(x, y) == 1); // expected-warning{{TRUE}}
}

void strcasecmp_2() {
  char *x = "abc";
  char *y = "Bcd";
  clang_analyzer_eval(strcasecmp(x, y) == -1); // expected-warning{{TRUE}}
}

void strcasecmp_null_0() {
  char *x = NULL;
  char *y = "123";
  strcasecmp(x, y); // expected-warning{{Null pointer argument in call to string comparison function}}
}

void strcasecmp_null_1() {
  char *x = "123";
  char *y = NULL;
  strcasecmp(x, y); // expected-warning{{Null pointer argument in call to string comparison function}}
}

void strcasecmp_diff_length_0() {
  char *x = "abcde";
  char *y = "aBd";
  clang_analyzer_eval(strcasecmp(x, y) == -1); // expected-warning{{TRUE}}
}

void strcasecmp_diff_length_1() {
  char *x = "abc";
  char *y = "aBdef";
  clang_analyzer_eval(strcasecmp(x, y) == -1); // expected-warning{{TRUE}}
}

void strcasecmp_diff_length_2() {
  char *x = "aBcDe";
  char *y = "abc";
  clang_analyzer_eval(strcasecmp(x, y) == 1); // expected-warning{{TRUE}}
}

void strcasecmp_diff_length_3() {
  char *x = "aBc";
  char *y = "abcde";
  clang_analyzer_eval(strcasecmp(x, y) == -1); // expected-warning{{TRUE}}
}

void strcasecmp_embedded_null () {
	clang_analyzer_eval(strcasecmp("ab\0zz", "ab\0yy") == 0); // expected-warning{{TRUE}}
}

//===----------------------------------------------------------------------===
// strncasecmp()
//===----------------------------------------------------------------------===

#define strncasecmp BUILTIN(strncasecmp)
int strncasecmp(const char *s1, const char *s2, size_t n);

void strncasecmp_constant0() {
  clang_analyzer_eval(strncasecmp("abc", "Abc", 3) == 0); // expected-warning{{TRUE}}
}

void strncasecmp_constant_and_var_0() {
  char *x = "abc";
  clang_analyzer_eval(strncasecmp(x, "Abc", 3) == 0); // expected-warning{{TRUE}}
}

void strncasecmp_constant_and_var_1() {
  char *x = "abc";
  clang_analyzer_eval(strncasecmp("Abc", x, 3) == 0); // expected-warning{{TRUE}}
}

void strncasecmp_0() {
  char *x = "abc";
  char *y = "Abc";
  clang_analyzer_eval(strncasecmp(x, y, 3) == 0); // expected-warning{{TRUE}}
}

void strncasecmp_1() {
  char *x = "Bcd";
  char *y = "abc";
  clang_analyzer_eval(strncasecmp(x, y, 3) == 1); // expected-warning{{TRUE}}
}

void strncasecmp_2() {
  char *x = "abc";
  char *y = "Bcd";
  clang_analyzer_eval(strncasecmp(x, y, 3) == -1); // expected-warning{{TRUE}}
}

void strncasecmp_null_0() {
  char *x = NULL;
  char *y = "123";
  strncasecmp(x, y, 3); // expected-warning{{Null pointer argument in call to string comparison function}}
}

void strncasecmp_null_1() {
  char *x = "123";
  char *y = NULL;
  strncasecmp(x, y, 3); // expected-warning{{Null pointer argument in call to string comparison function}}
}

void strncasecmp_diff_length_0() {
  char *x = "abcde";
  char *y = "aBd";
  clang_analyzer_eval(strncasecmp(x, y, 5) == -1); // expected-warning{{TRUE}}
}

void strncasecmp_diff_length_1() {
  char *x = "abc";
  char *y = "aBdef";
  clang_analyzer_eval(strncasecmp(x, y, 5) == -1); // expected-warning{{TRUE}}
}

void strncasecmp_diff_length_2() {
  char *x = "aBcDe";
  char *y = "abc";
  clang_analyzer_eval(strncasecmp(x, y, 5) == 1); // expected-warning{{TRUE}}
}

void strncasecmp_diff_length_3() {
  char *x = "aBc";
  char *y = "abcde";
  clang_analyzer_eval(strncasecmp(x, y, 5) == -1); // expected-warning{{TRUE}}
}

void strncasecmp_diff_length_4() {
  char *x = "abcde";
  char *y = "aBc";
  clang_analyzer_eval(strncasecmp(x, y, 3) == 0); // expected-warning{{TRUE}}
}

void strncasecmp_diff_length_5() {
  char *x = "abcde";
  char *y = "aBd";
  clang_analyzer_eval(strncasecmp(x, y, 3) == -1); // expected-warning{{TRUE}}
}

void strncasecmp_diff_length_6() {
  char *x = "aBDe";
  char *y = "abc";
  clang_analyzer_eval(strncasecmp(x, y, 3) == 1); // expected-warning{{TRUE}}
}

void strncasecmp_embedded_null () {
	clang_analyzer_eval(strncasecmp("ab\0zz", "ab\0yy", 4) == 0); // expected-warning{{TRUE}}
}

//===----------------------------------------------------------------------===
// strsep()
//===----------------------------------------------------------------------===

char *strsep(char **stringp, const char *delim);

void strsep_null_delim(char *s) {
  strsep(&s, NULL); // expected-warning{{Null pointer argument in call to strsep()}}
}

void strsep_null_search() {
  strsep(NULL, ""); // expected-warning{{Null pointer argument in call to strsep()}}
}

void strsep_return_original_pointer(char *s) {
  char *original = s;
  char *result = strsep(&s, ""); // no-warning
  clang_analyzer_eval(original == result); // expected-warning{{TRUE}}
}

void strsep_null_string() {
  char *s = NULL;
  char *result = strsep(&s, ""); // no-warning
  clang_analyzer_eval(result == NULL); // expected-warning{{TRUE}}
}

void strsep_changes_input_pointer(char *s) {
  char *original = s;
  strsep(&s, ""); // no-warning
  clang_analyzer_eval(s == original); // expected-warning{{UNKNOWN}}
  clang_analyzer_eval(s == NULL); // expected-warning{{UNKNOWN}}

  // Check that the value is symbolic.
  if (s == NULL) {
    clang_analyzer_eval(s == NULL); // expected-warning{{TRUE}}
  }
}

void strsep_changes_input_string() {
  char str[] = "abc";

  clang_analyzer_eval(str[1] == 'b'); // expected-warning{{TRUE}}

  char *s = str;
  strsep(&s, "b"); // no-warning

  // The real strsep will change the first delimiter it finds into a NUL
  // character. For now, we just model the invalidation.
  clang_analyzer_eval(str[1] == 'b'); // expected-warning{{UNKNOWN}}
}

//===----------------------------------------------------------------------===
// FIXMEs
//===----------------------------------------------------------------------===

// The analyzer_eval call below should evaluate to true. We are being too 
// aggressive in marking the (length of) src symbol dead. The length of dst 
// depends on src. This could be explicitely specified in the checker or the 
// logic for handling MetadataSymbol in SymbolManager needs to change.
void strcat_symbolic_src_length(char *src) {
	char dst[8] = "1234";
	strcat(dst, src);
  clang_analyzer_eval(strlen(dst) >= 4); // expected-warning{{UNKNOWN}}
}

// The analyzer_eval call below should evaluate to true. Most likely the same
// issue as the test above.
void strncpy_exactly_matching_buffer2(char *y) {
	if (strlen(y) >= 4)
		return;

	char x[4];
	strncpy(x, y, 4); // no-warning

	// This time, we know that y fits in x anyway.
  clang_analyzer_eval(strlen(x) <= 3); // expected-warning{{UNKNOWN}}
}
