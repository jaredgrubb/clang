// RUN: %clang_cc1 -fblocks -analyze -analyzer-checker=core,alpha.cplusplus.BlockRefCapture -verify %s

// Fake the signatures of the GCD functions:
//   -- note we use NULL for queues because it's easy and doesnt affect any of our checks
typedef void (^dispatch_block_t)(void);
void dispatch_async(const void* queue, dispatch_block_t block);
void dispatch_sync(const void* queue, dispatch_block_t block);

void checkCapture_Nothing() {
  dispatch_async(NULL, ^{ // no warning
    int a = 1;
    (void)a;
  });
}

void checkCapture_Local() {
  int a = 7;
  dispatch_async(NULL, ^{ // no warning
    (void)a;
  });
}

void checkCapture_LocalByRef() {
  int a = 7;
  const int& a_ref = a;
  dispatch_async(NULL, ^{ // expected-warning {{Capturing}}
    (void)a_ref;
  });
}
