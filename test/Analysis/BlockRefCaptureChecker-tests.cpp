// RUN: %clang_cc1 -fblocks -std=c++11 -analyze -analyzer-checker=core,alpha.cplusplus.BlockRefCapture -verify %s

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
Derived const& getMaybeGlobal(); // cant infer anything about this return value

void checkCapture_Nothing() {
  dispatch_async(DUMMY_QUEUE, ^{ // no warning
    int a = 1;
    (void)a;
  });
}

void checkCapture_StackVar() {
  int a = 7; // no warning
  dispatch_async(DUMMY_QUEUE, ^{ 
    (void)a;
  });
}

void checkCapture_RefToStackVar() {
  int a = 7;
  int& ref_to_stack = a; // expected-warning {{Variable 'ref_to_stack' is captured as a reference-type to a variable that may not exist when the block runs}}
  dispatch_async(DUMMY_QUEUE, ^{ 
    (void)ref_to_stack;
  });
}

void checkCapture_RefToStackVarViaImplicitCast() {
  int a = 7;
  const int& ref_to_stack = a; // expected-warning {{Variable 'ref_to_stack' is captured as a reference-type to a variable that may not exist when the block runs}}
  dispatch_async(DUMMY_QUEUE, ^{ 
    (void)ref_to_stack;
  });
}

void checkCapture_RefToTemporary() {
  int const& ref_to_temp = 7; // expected-warning {{Variable 'ref_to_temp' is captured as a reference-type to a variable that may not exist when the block runs}}
  dispatch_async(DUMMY_QUEUE, ^{ 
    (void)ref_to_temp;
  });
}

void checkCapture_RefToTemporaryReturnValue() {
  Derived const& ref_to_temp_obj = createObject();  // expected-warning {{Variable 'ref_to_temp_obj' is captured as a reference-type to a variable that may not exist when the block runs}}
  dispatch_async(DUMMY_QUEUE, ^{
    (void)ref_to_temp_obj;
  });
}

void checkCapture_RvalRefToTemporaryReturnValue() {
  Derived&& rval_ref_to_temp = createObject(); // expected-warning {{Variable 'rval_ref_to_temp' is captured as a reference-type to a variable that may not exist when the block runs}}
  dispatch_async(DUMMY_QUEUE, ^{
    (void)rval_ref_to_temp;
  });
}

void checkCapture_RefToUnknownReturnValue() {
  Derived const& ref_to_ambig_obj = getMaybeGlobal(); // no warning
  dispatch_async(DUMMY_QUEUE, ^{
    (void)ref_to_ambig_obj;
  });
}

void checkCapture_Param(int param) { // no warning
  dispatch_async(DUMMY_QUEUE, ^{
    (void)param;
  });
}

void checkCapture_RefToParam(const int& param_ref) { // expected-warning {{Variable 'param_ref' is captured as a reference-type to a variable that may not exist when the block runs}}
  dispatch_async(DUMMY_QUEUE, ^{
    (void)param_ref;
  });
}