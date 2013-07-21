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
typedef SmallVector<SymbolRef, 2> SymbolVector;

class BlockRefCaptureChecker : public Checker< check::PreCall {

  mutable IdentifierInfo *II_dispatch_async;

  OwningPtr<BugType> RefCaptureBugType;

  void initIdentifierInfo(ASTContext &Ctx) const;

  void reportRefCaptureBug(SymbolRef FileDescSym,
                         const CallEvent &Call,
                         CheckerContext &C) const;
public:
  BlockRefCaptureChecker();

  /// Process fclose.
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

  if (Call.getCalleeIdentifier() != II_dispatch_async)
    return;

  if (Call.getNumArgs() != 2)
    return;

  // Get the symbolic value corresponding to the file handle.
  SymbolRef FileDesc = Call.getArgSVal(0).getAsSymbol();
  if (!FileDesc)
    return;

  // Check if the stream has already been closed.
  ProgramStateRef State = C.getState();
  const StreamState *SS = State->get<StreamMap>(FileDesc);
  if (SS && SS->isClosed()) {
    reportDoubleClose(FileDesc, Call, C);
    return;
  }

  // Generate the next transition, in which the stream is closed.
  State = State->set<StreamMap>(FileDesc, StreamState::getClosed());
  C.addTransition(State);
}

static bool isLeaked(SymbolRef Sym, const StreamState &SS,
                     bool IsSymDead, ProgramStateRef State) {
  if (IsSymDead && SS.isOpened()) {
    // If a symbol is NULL, assume that fopen failed on this path.
    // A symbol should only be considered leaked if it is non-null.
    ConstraintManager &CMgr = State->getConstraintManager();
    ConditionTruthVal OpenFailed = CMgr.isNull(State, Sym);
    return !OpenFailed.isConstrainedTrue();
  }
  return false;
}

void BlockRefCaptureChecker::checkDeadSymbols(SymbolReaper &SymReaper,
                                           CheckerContext &C) const {
  ProgramStateRef State = C.getState();
  SymbolVector LeakedStreams;
  StreamMapTy TrackedStreams = State->get<StreamMap>();
  for (StreamMapTy::iterator I = TrackedStreams.begin(),
                             E = TrackedStreams.end(); I != E; ++I) {
    SymbolRef Sym = I->first;
    bool IsSymDead = SymReaper.isDead(Sym);

    // Collect leaked symbols.
    if (isLeaked(Sym, I->second, IsSymDead, State))
      LeakedStreams.push_back(Sym);

    // Remove the dead symbol from the streams map.
    if (IsSymDead)
      State = State->remove<StreamMap>(Sym);
  }

  ExplodedNode *N = C.addTransition(State);
  reportLeaks(LeakedStreams, C, N);
}

void BlockRefCaptureChecker::reportDoubleClose(SymbolRef FileDescSym,
                                            const CallEvent &Call,
                                            CheckerContext &C) const {
  // We reached a bug, stop exploring the path here by generating a sink.
  ExplodedNode *ErrNode = C.generateSink();
  // If we've already reached this node on another path, return.
  if (!ErrNode)
    return;

  // Generate the report.
  BugReport *R = new BugReport(*DoubleCloseBugType,
      "Closing a previously closed file stream", ErrNode);
  R->addRange(Call.getSourceRange());
  R->markInteresting(FileDescSym);
  C.emitReport(R);
}

void BlockRefCaptureChecker::reportLeaks(SymbolVector LeakedStreams,
                                               CheckerContext &C,
                                               ExplodedNode *ErrNode) const {
  // Attach bug reports to the leak node.
  // TODO: Identify the leaked file descriptor.
  for (SmallVectorImpl<SymbolRef>::iterator
         I = LeakedStreams.begin(), E = LeakedStreams.end(); I != E; ++I) {
    BugReport *R = new BugReport(*LeakBugType,
        "Opened file is never closed; potential resource leak", ErrNode);
    R->markInteresting(*I);
    C.emitReport(R);
  }
}

bool BlockRefCaptureChecker::guaranteedNotToCloseFile(const CallEvent &Call) const{
  // If it's not in a system header, assume it might close a file.
  if (!Call.isInSystemHeader())
    return false;

  // Handle cases where we know a buffer's /address/ can escape.
  if (Call.argumentsMayEscape())
    return false;

  // Note, even though fclose closes the file, we do not list it here
  // since the checker is modeling the call.

  return true;
}

// If the pointer we are tracking escaped, do not track the symbol as
// we cannot reason about it anymore.
ProgramStateRef
BlockRefCaptureChecker::checkPointerEscape(ProgramStateRef State,
                                        const InvalidatedSymbols &Escaped,
                                        const CallEvent *Call,
                                        PointerEscapeKind Kind) const {
  // If we know that the call cannot close a file, there is nothing to do.
  if (Kind == PSK_DirectEscapeOnCall && guaranteedNotToCloseFile(*Call)) {
    return State;
  }

  for (InvalidatedSymbols::const_iterator I = Escaped.begin(),
                                          E = Escaped.end();
                                          I != E; ++I) {
    SymbolRef Sym = *I;

    // The symbol escaped. Optimistically, assume that the corresponding file
    // handle will be closed somewhere else.
    State = State->remove<StreamMap>(Sym);
  }
  return State;
}

void BlockRefCaptureChecker::initIdentifierInfo(ASTContext &Ctx) const {
  if (IIfopen)
    return;
  IIfopen = &Ctx.Idents.get("fopen");
  IIfclose = &Ctx.Idents.get("fclose");
}

static const VarDecl * sFindProblemVarDecl(const VarDecl *VD)
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

void BlockRefCaptureChecker::checkPostStmt(const BlockExpr *BE, CheckerContext &C) const {
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

    std::cout << "  -- DANGER WILL ROBINSON!" << std::endl;


    // // Get the VarRegion associated with VD in the local stack frame.
    // if (Optional<UndefinedVal> V =
    //       state->getSVal(I.getOriginalRegion()).getAs<UndefinedVal>()) {
    //   if (ExplodedNode *N = C.generateSink()) {
    //     if (!BT)
    //       BT.reset(new BuiltinBug("uninitialized variable captured by block"));

    //     // Generate a bug report.
    //     SmallString<128> buf;
    //     llvm::raw_svector_ostream os(buf);

    //     os << "Variable '" << VD->getName()
    //        << "' is uninitialized when captured by block";

    //     BugReport *R = new BugReport(*BT, os.str(), N);
    //     if (const Expr *Ex = FindBlockDeclRefExpr(BE->getBody(), VD))
    //       R->addRange(Ex->getSourceRange());
    //     R->addVisitor(new FindLastStoreBRVisitor(*V, VR,
    //                                          /*EnableNullFPSuppression*/false));
    //     R->disablePathPruning();
    //     // need location of block
    //     C.emitReport(R);
    //   }
    // }
  }
}



void ento::registerBlockRefCaptureChecker(CheckerManager &mgr) {
  mgr.registerChecker<BlockRefCaptureChecker>();
}
