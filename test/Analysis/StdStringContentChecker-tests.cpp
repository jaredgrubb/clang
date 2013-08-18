// RUN: %clang_cc1 -fblocks -std=c++98 -analyze -analyzer-checker=core.NullDereference,alpha.cplusplus.StdStringContent -verify %s
// RUN: %clang_cc1 -fblocks -std=c++11 -analyze -analyzer-checker=core.NullDereference,alpha.cplusplus.StdStringContent -verify %s
// RUN: %clang_cc1 -fblocks -std=c++11 -stdlib=libc++ -analyze -analyzer-checker=core.NullDereference,alpha.cplusplus.StdStringContent -verify %s
// RUN: %clang_cc1 -fblocks -std=c++1y -analyze -analyzer-checker=core.NullDereference,alpha.cplusplus.StdStringContent -verify %s

// going away soon:
// expected-no-diagnostics

#include <string>

// We use NULL dereference to check for places the analyzer shouldnt go. 
//   TODO: There should be a "expect-unreachable" annotation for unit tests!
static int * const NULL_PTR = 0;

std::string CreateString();  // dummy function to generate unknown string

void check_ctor_default() {
  std::string str;

  if (!str.empty()) {
    *NULL_PTR = 42;  // not reachable
  }
}

void check_ctor_string_literal() {
  std::string str ("ABC");

  if (str.empty()) {
    *NULL_PTR = 42;  // not reachable
  }

  if (str.size() != 3) {
    *NULL_PTR = 42;  // not reachable
  }
}

void check_CreateString_size_tracked() {
  std::string str = CreateString();

  // nothing is known about 'str', but once we conjure a size,
  // we should be able to use it again later. This makes sure we're
  // tracking it right.
  if (str.empty()) {
    if (str.size()) { // contraction that it's empty, so this is all unreachable
      *NULL_PTR = 42;  // not reachable      
    }
  }
}
