// RUN: %clang_cc1 -fsyntax-only -verify -pedantic -fcxx-exceptions %s
class C;
class C {
public:
protected:
  typedef int A,B;
  static int sf(), u;

  struct S {};
  enum {}; // expected-warning{{declaration does not declare anything}}
  int; // expected-warning {{declaration does not declare anything}}
  int : 1, : 2;

public:
  void m0() {}; // ok, one extra ';' is permitted
  void m1() {}
  ; // ok, one extra ';' is permitted
  void m() {
    int l = 2;
  };; // expected-warning{{extra ';' after member function definition}}

  template<typename T> void mt(T) { }
  ;
  ; // expected-warning{{extra ';' inside a class}}

  virtual int vf() const volatile = 0;
  
private:
  int x,f(),y,g();
  inline int h();
  static const int sci = 10;
  mutable int mi;
};
void glo()
{
  struct local {};
}

// PR3177
typedef union {
  __extension__ union {
    int a;
    float b;
  } y;
} bug3177;

// check that we don't consume the token after the access specifier 
// when it's not a colon
class D {
public // expected-error{{expected ':'}}
  int i;
};

// consume the token after the access specifier if it's a semicolon 
// that was meant to be a colon
class E {
public; // expected-error{{expected ':'}}
  int i;
};

class F {
    int F1 { return 1; } // expected-error{{function definition does not declare parameters}}
    void F2 {} // expected-error{{function definition does not declare parameters}}
    typedef int F3() { return 0; } // expected-error{{function definition declared 'typedef'}}
    typedef void F4() {} // expected-error{{function definition declared 'typedef'}}
};

namespace ctor_error {
  class Foo {};
  // By [class.qual]p2, this is a constructor declaration.
  Foo::Foo (F) = F(); // expected-error{{does not match any declaration in 'ctor_error::Foo'}}

  class Ctor { // expected-note{{not complete until the closing '}'}}
    Ctor(f)(int); // ok
    Ctor(g(int)); // ok
    Ctor(x[5]); // expected-error{{incomplete type}}

    Ctor(UnknownType *); // expected-error{{unknown type name 'UnknownType'}}
    void operator+(UnknownType*); // expected-error{{unknown type name 'UnknownType'}}
  };

  Ctor::Ctor (x) = { 0 }; // \
    // expected-error{{qualified reference to 'Ctor' is a constructor name}}

  Ctor::Ctor(UnknownType *) {} // \
    // expected-error{{unknown type name 'UnknownType'}}
  void Ctor::operator+(UnknownType*) {} // \
    // expected-error{{unknown type name 'UnknownType'}}
}

namespace nns_decl {
  struct A {
    struct B;
  };
  namespace N {
    union C;
  }
  struct A::B; // expected-error {{forward declaration of struct cannot have a nested name specifier}}
  union N::C; // expected-error {{forward declaration of union cannot have a nested name specifier}}
}

// PR13775: Don't assert here.
namespace PR13775 {
  class bar
  {
   public:
    void foo ();
    void baz ();
  };
  void bar::foo ()
  {
    baz x(); // expected-error 3{{}}
  }
}

class pr16989 {
  void tpl_mem(int *) {
    return;
    class C2 {
      void f();
    };
    void C2::f() {} // expected-error{{function definition is not allowed here}}
  };
};

namespace CtorErrors {
  struct A {
    A(NonExistent); // expected-error {{unknown type name 'NonExistent'}}
  };
  struct B {
    B(NonExistent) : n(0) {} // expected-error {{unknown type name 'NonExistent'}}
    int n;
  };
  struct C {
    C(NonExistent) try {} catch (...) {} // expected-error {{unknown type name 'NonExistent'}}
  };
  struct D {
    D(NonExistent) {} // expected-error {{unknown type name 'NonExistent'}}
  };
}

namespace DtorErrors {
  struct A { ~A(); } a;
  ~A::A() {} // expected-error {{'~' in destructor name should be after nested name specifier}} expected-note {{previous}}
  A::~A() {} // expected-error {{redefinition}}

  struct B { ~B(); } *b;
  DtorErrors::~B::B() {} // expected-error {{'~' in destructor name should be after nested name specifier}}

  void f() {
    a.~A::A(); // expected-error {{'~' in destructor name should be after nested name specifier}}
    b->~DtorErrors::~B::B(); // expected-error {{'~' in destructor name should be after nested name specifier}}
  }
}

namespace BadFriend {
  struct A {
    friend int : 3; // expected-error {{friends can only be classes or functions}}
    friend void f() = 123; // expected-error {{illegal initializer}}
    friend virtual void f(); // expected-error {{'virtual' is invalid in friend declarations}}
    friend void f() final; // expected-error {{'final' is invalid in friend declarations}}
    friend void f() override; // expected-error {{'override' is invalid in friend declarations}}
  };
}

class PR20760_a {
  int a = ); // expected-warning {{extension}} expected-error {{expected expression}}
  int b = }; // expected-warning {{extension}} expected-error {{expected expression}}
  int c = ]; // expected-warning {{extension}} expected-error {{expected expression}}
};
class PR20760_b {
  int d = d); // expected-warning {{extension}} expected-error {{expected ';'}}
  int e = d]; // expected-warning {{extension}} expected-error {{expected ';'}}
  int f = d // expected-warning {{extension}} expected-error {{expected ';'}}
};

// PR11109 must appear at the end of the source file
class pr11109r3 { // expected-note{{to match this '{'}}
  public // expected-error{{expected ':'}} expected-error{{expected '}'}} expected-error{{expected ';' after class}}
