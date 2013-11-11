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

  /// One-time setup for the II's used by this checker.
  void initIdentifierInfo(ASTContext &Ctx) const;

  /// Examine a block's captured variables for anything that looks suspect.
  void checkBlockForBadCapture(const BlockExpr *Block, CheckerContext &C) const;

  /// Generate a bug for the given variable.
  void reportRefCaptureBug(const VarDecl *VD,
                           CheckerContext &C) const;
public:
  BlockRefCaptureChecker();

  /// Process dispatch_async.
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const;
};

class BlockRefReportVisitor : public BugReporterVisitorImpl<BlockRefReportVisitor> {
protected:
  const VarDecl *Var;

  int count;
  
public:
  BlockRefReportVisitor(const VarDecl *Var) : Var(Var), count() {}

  virtual void Profile(llvm::FoldingSetNodeID &ID) const {
    static int x = 0;
    ID.AddPointer(&x);
  }

  virtual PathDiagnosticPiece *VisitNode(const ExplodedNode *N,
                                         const ExplodedNode *PrevN,
                                         BugReporterContext &BRC,
                                         BugReport &BR);

  // virtual PathDiagnosticPiece *getEndPath(BugReporterContext &BRC,
  //                                         const ExplodedNode *N,
  //                                         BugReport &BR);
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

  // Check API that process blocks asynchronously.
  // For now, that's only dispatch_async, but in the future it would be
  // good to generalize that (eg, std::async+lambdas, or dispatch sources)

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
// Return NULL if the VarDecl is no issue.
static const bool sFindProblemVarDecl(const VarDecl *VD, SmallVector<VarDecl*, 4>& ProblemDeclChain)
{
  if (!VD)
    return false;

  // Only local variables can cause a problem. Statics/globals are fine.
  if (!VD->hasLocalStorage())
    return false;

  // Local '__block' variables are acceptable too.
  if (VD->getAttr<BlocksAttr>() || !VD->hasLocalStorage())
    return false;

  // Local variables of non-reference type are the main problem we are searching for.
  if (!VD->getType()->isReferenceType()) {
    ProblemDeclChain.push_back(VD);
    return true;    
  }

  // In general, we dont know if a passed-in parameter references a local or global, 
  // but we take the pessimistic view that it probably is pointing to some stack var
  // and will report it.
  if (dyn_cast<ParmVarDecl>(VD)) {
    return true;
  }

  // VD is a local reference to some expression.
  Expr const* Init = VD->getInit();
  if (!Init)
    return false;

  // Strip any implicit casts (eg, upcasting), since these do not affect capture analysis.
  Init = Init->IgnoreImpCasts();

  // Reference-to-a-reference: safety depends on the safety of THAT reference.
  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Init)) {
    ProblemDeclChain.push_back(VD);
    return sFindProblemVarDecl(dyn_cast<VarDecl>(DRE->getDecl()), ProblemDeclChain);      
  }

  // Reference to temporary expression (eg return value) cant have local storage.
  if (const MaterializeTemporaryExpr *MTE = dyn_cast<MaterializeTemporaryExpr>(Init)) {
    if (MTE->getStorageDuration()==SD_Automatic) {
      ProblemDeclChain.push_back(VD);
      return true;
    }
    return false;
  }

  // Any other case is not handled, so we assume it's safe.
  return false;
}

void BlockRefCaptureChecker::checkBlockForBadCapture(const BlockExpr *BE, CheckerContext &C) const {
  // No block captures is no problem.
  if (!BE->getBlockDecl()->hasCaptures())
    return;

  // Iterate over every captured variable and report problems.
  ProgramStateRef state = C.getState();
  const BlockDataRegion *R = cast<BlockDataRegion>(
    state->getSVal(BE, C.getLocationContext()).getAsRegion());

  BlockDataRegion::referenced_vars_iterator I = R->referenced_vars_begin(),
                                            E = R->referenced_vars_end();
  for (; I != E; ++I) {
    const VarRegion *VR = I.getOriginalRegion();
    const VarDecl *VD = VR->getDecl();

    // Skip any non-reference captures.
    if (!VD->getType()->isReferenceType()) {
      continue;
    }

    SmallVector<VarDecl*, 4> ProblemDecl;
    bool HasProblem = sFindProblemVarDecl(VD, ProblemDecl);
    if (!HasProblem) {
      continue;
    }

    reportRefCaptureBug(VD, C);
  }
}

void BlockRefCaptureChecker::reportRefCaptureBug(
                         const VarDecl *VD,
                         SmallVector<VarDecl*, 4> ProblemDeclChain,
                         CheckerContext &C) const
{
  ExplodedNode *N = C.addTransition();

  SmallString<128> buf;
  llvm::raw_svector_ostream os(buf);

  os << "Variable '" << VD->getName() 
     << "' is captured as a reference to a value "
        "that may not exist when the block actually runs.";

  // PathDiagnosticLocation Loc =  PathDiagnosticLocation::create(
  //         VD, C.getSourceManager());

  BugReport *Bug = new BugReport(*BT_RefCaptureBug, os.str(), N);
  
  for(SmallVector<VarDecl*, 4>::const_iterator i = ProblemDeclChain.begin(),
    e = ProblemDeclChain.end();
    i != e;
    ++i) 
  {
    Bug->addVisitor(new BlockRefReportVisitor(*i));    
  }
  C.emitReport(Bug);
}

void BlockRefCaptureChecker::initIdentifierInfo(ASTContext &Ctx) const {
  if (II_dispatch_async)
    return;
  II_dispatch_async = &Ctx.Idents.get("dispatch_async");
}

void ento::registerBlockRefCaptureChecker(CheckerManager &mgr) {
  mgr.registerChecker<BlockRefCaptureChecker>();
}



PathDiagnosticPiece *BlockRefReportVisitor::VisitNode(const ExplodedNode *N,
                                                      const ExplodedNode *PrevN,
                                                      BugReporterContext &BRC,
                                                      BugReport &BR) 
{
  Optional<PostStmt> P = N->getLocationAs<PostStmt>();
  if (!P) {
    llvm::outs().changeColor(llvm::raw_ostream::GREEN) << " ------- " << ++count << " --------\n";
    llvm::outs().changeColor(llvm::raw_ostream::GREEN) << "     ------- Not a PostStmt\n";
    return NULL;
  }

  const DeclStmt *DS = P->getStmtAs<DeclStmt>();
  if (!DS) {
    llvm::outs().changeColor(llvm::raw_ostream::GREEN) << " ------- " << ++count << " --------\n";
    llvm::outs().changeColor(llvm::raw_ostream::GREEN) << "     ------- PostStmt, but not a DeclStmt\n";
    return NULL;
  }

  ProgramStateRef state = N->getState();
  ProgramStateRef statePrev = PrevN->getState();

  ProgramPoint ProgLoc = N->getLocation();

  llvm::outs().changeColor(llvm::raw_ostream::GREEN) << " ------- " << ++count << " --------\n";
  llvm::outs().changeColor(llvm::raw_ostream::GREEN) << "       ------- Decl\n";
  llvm::outs().resetColor();
  const Decl &D = N->getCodeDecl();
  DS->dump();

  llvm::outs().changeColor(llvm::raw_ostream::GREEN) << "       ------- Progam state\n";
  llvm::outs().resetColor();
  state->dump();

  return NULL;
}
