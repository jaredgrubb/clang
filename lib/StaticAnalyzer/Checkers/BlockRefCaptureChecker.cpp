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

#include <iostream>

using namespace clang;
using namespace ento;

namespace {

class BlockRefCaptureChecker : public Checker< check::PreCall > {

  mutable IdentifierInfo *II_dispatch_async;

  OwningPtr<BugType> BT_RefCaptureBug;

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

BlockRefCaptureChecker::BlockRefCaptureChecker() 
: II_dispatch_async(0) 
{
  // Initialize the bug types.
  BT_RefCaptureBug.reset(new BugType("Capture-by-reference warning",
                                    "Block capture error"));
}

void BlockRefCaptureChecker::checkPreCall(const CallEvent &Call,
                                       CheckerContext &C) const {
  if (!Call.isGlobalCFunction())
    return;

  initIdentifierInfo(C.getASTContext());

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

    checkBlockForBadCapture(BE, C);
    return;
  }
}

// Figure out if a VarDecl is a problem, and return the problem VD if so
// Return NULL if the VarDecl is no issue
static const VarDecl *sFindProblemVarDecl(const VarDecl *VD)
{
  if (!VD) {
    return NULL;
  }

  // if we hit a __block type or a non-local variable, we're good
  if (VD->getAttr<BlocksAttr>() || !VD->hasLocalStorage()) {
    return NULL;
  }

  // if we hit a non-ref type then we have a problem (because it has local storage)
  if (!VD->getType()->isReferenceType()) {
    return VD;
  }

  // In general, we cant know if a passed-in paramter is local or global, but
  // we take the pessimistic view that it probably is a temporary and report it
  if (dyn_cast<ParmVarDecl>(VD)) {
    return VD;
  }

  // Use the expression that the reference points to and figure out if it's a problem:
  Expr const* Init = VD->getInit();
  if (!Init) {
    return NULL;
  }

  // ignore any implicit casts (eg, upcasting) that dont affect our analysis
  Init = Init->IgnoreImpCasts();

  // if we have a ref-to-ref, the recurse on its reference value
  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Init)) {
    return sFindProblemVarDecl(dyn_cast<VarDecl>(DRE->getDecl()));      
  }

  // reference to temporary expression (eg return value) cant have local storage
  if (const MaterializeTemporaryExpr *MTE = dyn_cast<MaterializeTemporaryExpr>(Init)) {
    if (MTE->getStorageDuration()==SD_Automatic) {
      return VD;
    }
    return NULL;
  }

  // all other cases are not handled and presumed safe
  return NULL;
}

void BlockRefCaptureChecker::checkBlockForBadCapture(const BlockExpr *BE, CheckerContext &C) const {
  // no captures, no problem.
  if (!BE->getBlockDecl()->hasCaptures())
    return;

  // otherwise check every variable captured in the block
  ProgramStateRef state = C.getState();
  const BlockDataRegion *R =
    cast<BlockDataRegion>(state->getSVal(BE,
                                         C.getLocationContext()).getAsRegion());

  BlockDataRegion::referenced_vars_iterator I = R->referenced_vars_begin(),
                                            E = R->referenced_vars_end();

  for (; I != E; ++I) {
    const VarRegion *VR = I.getOriginalRegion();
    const VarDecl *VD = VR->getDecl();

    // we only care about reference captures
    if (!VD->getType()->isReferenceType()) {
      continue;
    }

    const VarDecl *ProbVD = sFindProblemVarDecl(VD);
    if (!ProbVD) {
      continue;
    }

    SmallString<128> buf;
    llvm::raw_svector_ostream os(buf);

    os << "Variable '" << VD->getName() 
       << "' is captured as a reference-type to a value "
          "that may not exist when the block runs.";

    PathDiagnosticLocation Loc =  PathDiagnosticLocation::create(
            VD, C.getSourceManager());

    BugReport *Bug = new BugReport(*BT_RefCaptureBug, os.str(), Loc);
    Bug->markInteresting(VR);
    C.emitReport(Bug);
  }
}

void BlockRefCaptureChecker::initIdentifierInfo(ASTContext &Ctx) const {
  if (II_dispatch_async)
    return;
  II_dispatch_async = &Ctx.Idents.get("dispatch_async");
}

void ento::registerBlockRefCaptureChecker(CheckerManager &mgr) {
  mgr.registerChecker<BlockRefCaptureChecker>();
}
