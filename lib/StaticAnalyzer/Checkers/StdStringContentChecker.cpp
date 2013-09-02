//===-- StdStringContentChecker.cpp -----------------------------------------*- C++ -*--//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Defines a checker to track the content/size of a std::string object. This
// is accomplished via BodyFarm emulation of the std::string implementation and
// hooks injected into the methods.
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

struct StringState {
  // enum Kind { Opened, Closed, OpenFailed, Escaped } K;
  // const Stmt *S;

  SVal Size;

  StringState(SVal Data, SVal Size) : Size(Size) {}

  SVal getSize() const { return Size; }
  // bool isOpened() const { return K == Opened; }
  // bool isClosed() const { return K == Closed; }

  bool operator==(const StringState &X) const {
    return Size == X.Size;
  }

  static StringState create(SVal Data, SVal Size) { return StringState(Data, Size); }
  // static StringState getClosed(const Stmt *s) { return StringState(Closed, s); }
  // static StringState getOpenFailed(const Stmt *s) { 
  //   return StringState(OpenFailed, s); 
  // }
  // static StringState getEscaped(const Stmt *s) {
  //   return StringState(Escaped, s);
  // }

  void Profile(llvm::FoldingSetNodeID &ID) const {
    Size.Profile(ID);
  }
};

class StdStringContentChecker : public Checker< eval::Call > {

  OwningPtr<BugType> BT_RefCaptureBug;

  bool handleContentSet(const CallExpr &CE, CheckerContext &C) const;
  bool handleContentSetWithCString(const CallExpr &CE, CheckerContext &C) const;
  bool handleContentGetSize(const CallExpr &CE, CheckerContext &C) const;

  static void *getTag() { static int tag; return &tag; }

public:
  StdStringContentChecker();

  bool evalCall(const CallExpr *CE, CheckerContext &C) const;
};

} // end anonymous namespace

REGISTER_MAP_WITH_PROGRAMSTATE(StringContentMap, const MemRegion *, StringState)


////////// The following are stolen from CStringChecker. That's not good.
////////// We need to figure out how to share these functions.

// copied from CStringChecker::getCStringLiteral
static const StringLiteral *getCStringLiteral(CheckerContext &C,
  const Expr *expr, SVal val) 
{
  // Get the memory region pointed to by the val.
  const MemRegion *bufRegion = val.getAsRegion();
  if (!bufRegion)
    return NULL; 

  // Strip casts off the memory region.
  bufRegion = bufRegion->StripCasts();

  // Cast the memory region to a string region.
  const StringRegion *strRegion= dyn_cast<StringRegion>(bufRegion);
  if (!strRegion)
    return NULL; 

  // Return the actual string in the string region.
  return strRegion->getStringLiteral();
}





StdStringContentChecker::StdStringContentChecker() 
{
  // Initialize the bug types.
  BT_RefCaptureBug.reset(new BugType("Capture-by-reference warning",
                                    "Block capture error"));
}

bool StdStringContentChecker::evalCall(const CallExpr *CE,
                                       CheckerContext &C) const 
{
  const FunctionDecl *FD = C.getCalleeDecl(CE);
  if (!FD || FD->getKind() != Decl::Function)
    return false;

  llvm::outs().changeColor(llvm::raw_ostream::GREEN) 
  << " -------- StdStringContentChecker::evalCall\n";
  llvm::outs().resetColor();

  // Note that we can't pre-cache the synthesized FunctionDecl hooks for pointer comparison
  // because the current implementation creates them lazily, so we never know which
  // ones are valid and whether some ever will be.

  IdentifierInfo *II = FD->getIdentifier();
  if (!II)
    return false;

  StringRef FDName = II->getName();
  if (!FDName.startswith("_csa_hook_"))
    return false;

  if (FDName.equals("_csa_hook_content_set"))
    return handleContentSet(*CE, C);
  else if (FDName.equals("_csa_hook_content_set_with_string"))
    return handleContentSetWithCString(*CE, C);
  else if (FDName.equals("_csa_hook_content_get_size"))
    return handleContentGetSize(*CE, C);

  return false;
}

bool StdStringContentChecker::handleContentSet(const CallExpr &CE, 
                                               CheckerContext &C) const
{
  if (CE.getNumArgs() != 3) 
    return false;

  llvm::outs().changeColor(llvm::raw_ostream::BLUE) 
  << " -------- StdStringContentChecker::handleContentSet\n";
  llvm::outs().resetColor();

  const LocationContext *LCtx = C.getLocationContext();
  ProgramStateRef State = C.getState();

  SVal This = State->getSVal(CE.getArg(0), LCtx);
  SVal Data = State->getSVal(CE.getArg(1), LCtx);
  SVal Size = State->getSVal(CE.getArg(2), LCtx);


  llvm::outs().changeColor(llvm::raw_ostream::YELLOW);
  This.dump();
  llvm::outs() << " :: ";
  Data.dump();
  llvm::outs() << " :: ";
  Size.dump();
  llvm::outs().resetColor();
  llvm::outs() << "\n";

  const MemRegion *StringObject = This.getAsRegion();
  if (!StringObject) 
    return true;  // the hook did its best, so we may as still swallow this call
  StringObject = StringObject->StripCasts();

  llvm::outs().changeColor(llvm::raw_ostream::RED)
  << "  -- recorded!";
  llvm::outs().resetColor();
  llvm::outs() << "\n";

  State = State->set<StringContentMap>(StringObject, StringState::create(Data, Size));

  C.addTransition(State);
  return true;
}

bool StdStringContentChecker::handleContentGetSize(const CallExpr &CE, 
                                               CheckerContext &C) const
{
  if (CE.getNumArgs() != 1) 
    return false;

  llvm::outs().changeColor(llvm::raw_ostream::BLUE) 
  << " -------- StdStringContentChecker::handleContentGetSize\n";
  llvm::outs().resetColor();

  const LocationContext *LCtx = C.getLocationContext();
  ProgramStateRef State = C.getState();

  SVal This = State->getSVal(CE.getArg(0), LCtx);
  llvm::outs().changeColor(llvm::raw_ostream::YELLOW);
  This.dump();
  llvm::outs() 
  << "\n  hasConjuredSymbol=" << This.hasConjuredSymbol()
  << "\n  getRawKind=" << This.getRawKind();
  llvm::outs().resetColor();
  llvm::outs() << "\n";

  const MemRegion *StringObject = This.getAsRegion();
  if (!StringObject) 
    return true;  // the hook did its best, so we may as still swallow this call
  StringObject = StringObject->StripCasts();

  llvm::outs().changeColor(llvm::raw_ostream::YELLOW);
  This.dump();
  llvm::outs().resetColor();
  llvm::outs() << "\n";

  SVal Size;

  const StringState *SS = State->get<StringContentMap>(StringObject);
  if (!SS) {
    // A new string that we've never seen before. Conjure up basic information.
    
    UnknownVal Data;
    Size = C.getSValBuilder().conjureSymbolVal(0, &CE, LCtx, C.blockCount());

    State = State->set<StringContentMap>(StringObject, StringState::create(Data, Size));
  } else {
    Size = SS->getSize();
  }

  llvm::outs().changeColor(llvm::raw_ostream::RED);
  Size.dump();
  llvm::outs().resetColor();
  llvm::outs() << "\n";

  State = State->BindExpr(&CE, LCtx, Size);

  C.addTransition(State);
  return true;
}

void ento::registerStdStringContentChecker(CheckerManager &mgr) {
  mgr.registerChecker<StdStringContentChecker>();
}
