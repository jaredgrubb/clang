//===-- BlockRefCaptureChecker.cpp -----------------------------------------*- C++ -*--//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Defines a checker to detect blocks that capture reference types. For async calls
// this can lead to blocks capturing bad memory addresses.
//
//===----------------------------------------------------------------------===//

#include "ClangSACheckers.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"

#include "clang/AST/Attr.h"

using namespace clang;
using namespace ento;

namespace {

class BlockRefCaptureChecker : public Checker< check::PreCall {

  mutable IdentifierInfo *II_dispatch_async;

  OwningPtr<BugType> RefCaptureBugType;

  void initIdentifierInfo(ASTContext &Ctx) const;

  void checkBlockForBadCapture(const BlockExpr *Block, CheckerContext &C) const;

  void reportRefCaptureBug(SymbolRef FileDescSym,
                         const CallEvent &Call,
                         CheckerContext &C) const;
public:
  BlockRefCaptureChecker();

  /// Process dispatch_async.
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const;
};

} // end anonymous namespace

BlockRefCaptureChecker::BlockRefCaptureChecker() : II_dispatch_async(0) {
  // Initialize the bug types.
  RefCaptureBugType.reset(new BugType("Capture-by-reference warning",
                                       "Block capture error"));
}

void BlockRefCaptureChecker::checkPreCall(const CallEvent &Call,
                                       CheckerContext &C) const {
  initIdentifierInfo(C.getASTContext());

  if (!Call.isGlobalCFunction())
    return;

  // Check API that process blocks asynchronously:

  if (Call.getCalleeIdentifier() == II_dispatch_async) {
    if (Call.getNumArgs() != 2)
      return;

    // Get the symbolic value corresponding to the file handle.
    const Expr * E = Call.getArgExpr(1);
    if (!E)
      return;

    const BlockExpr * BE = dyn_cast<BlockExpr>(E);
    if (!BE) 
      return;

    checkBlockForBadCapture(BE);
    return;
  }
}

static const VarDecl *sFindProblemVarDecl(const VarDecl *VD)
{
    if (!VD) {
      std::cout << "  [ ok ] VD = null." << std::endl;
      return NULL;
    }
    std::cout << "  [ .. ] VarDecl dump:" << std::endl;
    VD->dump();
    std::cout << "  [ .. ] with type:" << std::endl;
    VD->getType()->dump();

    // if we hit a __block type or a non-local variable, we're good
    if (VD->getAttr<BlocksAttr>() || !VD->hasLocalStorage()) {
      std::cout << "  [ ok ] __block or non-local." << std::endl;
      return NULL;
    }

    // if we hit a non-ref type, then we dont care
    if (!VD->getType()->isReferenceType()) {
      std::cout << "  [!!!!] resolved to reference-to-local value!" << std::endl;
      return VD;
    }

    // if we reference a parameter variable, we have a problem
    if (dyn_cast<ParmVarDecl>(VD)) {
      std::cout << "  [!!!!] a parameter value!" << std::endl;
      return VD;
    }

    // figure out what the initialization for the reference is:
    Expr const* Init = VD->getInit();
    if (!Init) {
      std::cout << "  [ ok ] no initializer. done." << std::endl;
      return NULL;
    }

    std::cout << "  [ .. ] has initialization value:" << std::endl;
    Init->dump();

    const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Init);
    if (!DRE) {
      std::cout << "  [ ok ] no DRE. done." << std::endl;
      return NULL;
    }
    std::cout << "  [ .. ] has DeclRefExpr:" << std::endl;
    DRE->dump();

    return sFindProblemVarDecl(dyn_cast<VarDecl>(DRE->getDecl()));
}

void BlockRefCaptureChecker::checkBlockForBadCapture(const BlockExpr *Block, CheckerContext &C) const {
   if (!BE->getBlockDecl()->hasCaptures())
    return;

  ProgramStateRef state = C.getState();
  const BlockDataRegion *R =
    cast<BlockDataRegion>(state->getSVal(BE,
                                         C.getLocationContext()).getAsRegion());

  BlockDataRegion::referenced_vars_iterator I = R->referenced_vars_begin(),
                                            E = R->referenced_vars_end();

  for (; I != E; ++I) {
    // This VarRegion is the region associated with the block; we need
    // the one associated with the encompassing context.
    const VarRegion *VR = I.getOriginalRegion();
    const VarDecl *VD = VR->getDecl();

    std::cout << std::endl << "^^^ Check block var named: " << VD->getName().data() << std::endl;

    // if we hit a non-ref type, then we dont care
    if (!VD->getType()->isReferenceType()) {
      std::cout << "  [ ok ] not ref type." << std::endl;
      continue;
    }

    const VarDecl *ProbVD = sFindProblemVarDecl(VD);
    if (!ProbVD) {
      std::cout << "  [    ] continue." << std::endl;
      continue;
    }

    // problem!
}

// void BlockRefCaptureChecker::reportDoubleClose(SymbolRef FileDescSym,
//                                             const CallEvent &Call,
//                                             CheckerContext &C) const {
//   // We reached a bug, stop exploring the path here by generating a sink.
//   ExplodedNode *ErrNode = C.generateSink();
//   // If we've already reached this node on another path, return.
//   if (!ErrNode)
//     return;

//   // Generate the report.
//   BugReport *R = new BugReport(*DoubleCloseBugType,
//       "Closing a previously closed file stream", ErrNode);
//   R->addRange(Call.getSourceRange());
//   R->markInteresting(FileDescSym);
//   C.emitReport(R);
// }

// void BlockRefCaptureChecker::reportLeaks(SymbolVector LeakedStreams,
//                                                CheckerContext &C,
//                                                ExplodedNode *ErrNode) const {
//   // Attach bug reports to the leak node.
//   // TODO: Identify the leaked file descriptor.
//   for (SmallVectorImpl<SymbolRef>::iterator
//          I = LeakedStreams.begin(), E = LeakedStreams.end(); I != E; ++I) {
//     BugReport *R = new BugReport(*LeakBugType,
//         "Opened file is never closed; potential resource leak", ErrNode);
//     R->markInteresting(*I);
//     C.emitReport(R);
//   }
// }

void BlockRefCaptureChecker::initIdentifierInfo(ASTContext &Ctx) const {
  if (II_dispatch_async)
    return;
  II_dispatch_async = &Ctx.Idents.get("dispatch_async");
}

void ento::registerBlockRefCaptureChecker(CheckerManager &mgr) {
  mgr.registerChecker<BlockRefCaptureChecker>();
}
