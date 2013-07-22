// RUN: %clang_cc1 -fblocks -analyze -analyzer-checker=core,alpha.cplusplus.BlockRefCapture -verify %s

// Fake the signatures of the GCD functions:
//   -- note we use NULL for queues because it's easy and doesnt affect any of our checks
typedef void (^dispatch_block_t)(void);
void dispatch_async(const void* queue, dispatch_block_t block);
void dispatch_sync(const void* queue, dispatch_block_t block);

static const void* const DUMMY_QUEUE = 0;

// helper types
struct Base {};
struct Derived : Base {};
Derived createObject();

void checkCapture_Nothing() {
  dispatch_async(DUMMY_QUEUE, ^{ // no warning
    int a = 1;
    (void)a;
  });
}

void checkCapture_StackVar() {
  int a = 7;
  dispatch_async(DUMMY_QUEUE, ^{ // no warning
    (void)a;
  });
}

void checkCapture_RefToStackVar() {
  int a = 7;
  int& a_ref = a;
  dispatch_async(DUMMY_QUEUE, ^{ // expected-warning {{Variable 'a_ref' is captured as a reference-type to a variable that may not exist when the block runs}}
    (void)a_ref;
  });
}

void checkCapture_RefToStackVarViaImplicitCast() {
  int a = 7;
  const int& a_ref = a;
  dispatch_async(DUMMY_QUEUE, ^{ // expected-warning {{Variable 'a_ref' is captured as a reference-type to a variable that may not exist when the block runs}}
    (void)a_ref;
  });
}

void checkCapture_RefToTemporary() {
  int const& a_ref = 7;
  dispatch_async(DUMMY_QUEUE, ^{ // expected-warning {{Variable 'a_ref' is captured as a reference-type to a variable that may not exist when the block runs}}
    (void)a_ref;
  });
}

void checkCapture_Param(int param) {
  dispatch_async(DUMMY_QUEUE, ^{ // no warning
    (void)param;
  });
}

void checkCapture_RefToParam(const int& param_ref) {
  dispatch_async(DUMMY_QUEUE, ^{ // expected-warning {{Variable 'param_ref' is captured as a reference-type to a variable that may not exist when the block runs}}
    (void)param_ref;
  });
}