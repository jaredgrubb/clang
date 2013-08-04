//== BodyFarm.cpp  - Factory for conjuring up fake bodies ----------*- C++ -*-//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// BodyFarm is a factory for creating faux implementations for functions/methods
// for analysis purposes.
//
//===----------------------------------------------------------------------===//

#include "BodyFarm.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprObjC.h"
#include "llvm/ADT/StringSwitch.h"

using namespace clang;

//===----------------------------------------------------------------------===//
// Creation functions for faux ASTs.
//===----------------------------------------------------------------------===//

static bool isDispatchBlock(QualType Ty) {
  // Is it a block pointer?
  const BlockPointerType *BPT = Ty->getAs<BlockPointerType>();
  if (!BPT)
    return false;

  // Check if the block pointer type takes no arguments and
  // returns void.
  const FunctionProtoType *FT =
  BPT->getPointeeType()->getAs<FunctionProtoType>();
  if (!FT || !FT->getResultType()->isVoidType()  ||
      FT->getNumArgs() != 0)
    return false;

  return true;
}
/// Create a fake body for dispatch_once.
static Stmt *create_dispatch_once(ASTContext &C, const FunctionDecl *D) {
  // Check if we have at least two parameters.
  if (D->param_size() != 2)
    return 0;

  // Check if the first parameter is a pointer to integer type.
  const ParmVarDecl *Predicate = D->getParamDecl(0);
  QualType PredicateQPtrTy = Predicate->getType();
  const PointerType *PredicatePtrTy = PredicateQPtrTy->getAs<PointerType>();
  if (!PredicatePtrTy)
    return 0;
  QualType PredicateTy = PredicatePtrTy->getPointeeType();
  if (!PredicateTy->isIntegerType())
    return 0;
  
  // Check if the second parameter is the proper block type.
  const ParmVarDecl *Block = D->getParamDecl(1);
  QualType Ty = Block->getType();
  if (!isDispatchBlock(Ty))
    return 0;
  
  // Everything checks out.  Create a fakse body that checks the predicate,
  // sets it, and calls the block.  Basically, an AST dump of:
  //
  // void dispatch_once(dispatch_once_t *predicate, dispatch_block_t block) {
  //  if (!*predicate) {
  //    *predicate = 1;
  //    block();
  //  }
  // }
  
  ASTMaker M(C);
  
  // (1) Create the call.
  DeclRefExpr *DR = M.makeDeclRefExpr(Block);
  ImplicitCastExpr *ICE = M.makeLvalueToRvalue(DR, Ty);
  CallExpr *CE = new (C) CallExpr(C, ICE, None, C.VoidTy, VK_RValue,
                                  SourceLocation());

  // (2) Create the assignment to the predicate.
  IntegerLiteral *IL =
    IntegerLiteral::Create(C, llvm::APInt(C.getTypeSize(C.IntTy), (uint64_t) 1),
                           C.IntTy, SourceLocation());
  BinaryOperator *B =
    M.makeAssignment(
       M.makeDereference(
          M.makeLvalueToRvalue(
            M.makeDeclRefExpr(Predicate), PredicateQPtrTy),
            PredicateTy),
       M.makeIntegralCast(IL, PredicateTy),
       PredicateTy);
  
  // (3) Create the compound statement.
  Stmt *Stmts[2];
  Stmts[0] = B;
  Stmts[1] = CE;
  CompoundStmt *CS = M.makeCompound(ArrayRef<Stmt*>(Stmts, 2));
  
  // (4) Create the 'if' condition.
  ImplicitCastExpr *LValToRval =
    M.makeLvalueToRvalue(
      M.makeDereference(
        M.makeLvalueToRvalue(
          M.makeDeclRefExpr(Predicate),
          PredicateQPtrTy),
        PredicateTy),
    PredicateTy);
  
  UnaryOperator *UO = new (C) UnaryOperator(LValToRval, UO_LNot, C.IntTy,
                                           VK_RValue, OK_Ordinary,
                                           SourceLocation());
  
  // (5) Create the 'if' statement.
  IfStmt *If = new (C) IfStmt(C, SourceLocation(), 0, UO, CS);
  return If;
}

/// Create a fake body for dispatch_sync.
static Stmt *create_dispatch_sync(ASTContext &C, const FunctionDecl *D) {
  // Check if we have at least two parameters.
  if (D->param_size() != 2)
    return 0;
  
  // Check if the second parameter is a block.
  const ParmVarDecl *PV = D->getParamDecl(1);
  QualType Ty = PV->getType();
  if (!isDispatchBlock(Ty))
    return 0;
  
  // Everything checks out.  Create a fake body that just calls the block.
  // This is basically just an AST dump of:
  //
  // void dispatch_sync(dispatch_queue_t queue, void (^block)(void)) {
  //   block();
  // }
  //  
  ASTMaker M(C);
  DeclRefExpr *DR = M.makeDeclRefExpr(PV);
  ImplicitCastExpr *ICE = M.makeLvalueToRvalue(DR, Ty);
  CallExpr *CE = new (C) CallExpr(C, ICE, None, C.VoidTy, VK_RValue,
                                  SourceLocation());
  return CE;
}

static Stmt *create_OSAtomicCompareAndSwap(ASTContext &C, const FunctionDecl *D)
{
  // There are exactly 3 arguments.
  if (D->param_size() != 3)
    return 0;
  
  // Signature:
  // _Bool OSAtomicCompareAndSwapPtr(void *__oldValue,
  //                                 void *__newValue,
  //                                 void * volatile *__theValue)
  // Generate body:
  //   if (oldValue == *theValue) {
  //    *theValue = newValue;
  //    return YES;
  //   }
  //   else return NO;
  
  QualType ResultTy = D->getResultType();
  bool isBoolean = ResultTy->isBooleanType();
  if (!isBoolean && !ResultTy->isIntegralType(C))
    return 0;
  
  const ParmVarDecl *OldValue = D->getParamDecl(0);
  QualType OldValueTy = OldValue->getType();

  const ParmVarDecl *NewValue = D->getParamDecl(1);
  QualType NewValueTy = NewValue->getType();
  
  assert(OldValueTy == NewValueTy);
  
  const ParmVarDecl *TheValue = D->getParamDecl(2);
  QualType TheValueTy = TheValue->getType();
  const PointerType *PT = TheValueTy->getAs<PointerType>();
  if (!PT)
    return 0;
  QualType PointeeTy = PT->getPointeeType();
  
  ASTMaker M(C);
  // Construct the comparison.
  Expr *Comparison =
    M.makeComparison(
      M.makeLvalueToRvalue(M.makeDeclRefExpr(OldValue), OldValueTy),
      M.makeLvalueToRvalue(
        M.makeDereference(
          M.makeLvalueToRvalue(M.makeDeclRefExpr(TheValue), TheValueTy),
          PointeeTy),
        PointeeTy),
      BO_EQ);

  // Construct the body of the IfStmt.
  Stmt *Stmts[2];
  Stmts[0] =
    M.makeAssignment(
      M.makeDereference(
        M.makeLvalueToRvalue(M.makeDeclRefExpr(TheValue), TheValueTy),
        PointeeTy),
      M.makeLvalueToRvalue(M.makeDeclRefExpr(NewValue), NewValueTy),
      NewValueTy);
  
  Expr *BoolVal = M.makeObjCBool(true);
  Expr *RetVal = isBoolean ? M.makeIntegralCastToBoolean(BoolVal)
                           : M.makeIntegralCast(BoolVal, ResultTy);
  Stmts[1] = M.makeReturn(RetVal);
  CompoundStmt *Body = M.makeCompound(ArrayRef<Stmt*>(Stmts, 2));
  
  // Construct the else clause.
  BoolVal = M.makeObjCBool(false);
  RetVal = isBoolean ? M.makeIntegralCastToBoolean(BoolVal)
                     : M.makeIntegralCast(BoolVal, ResultTy);
  Stmt *Else = M.makeReturn(RetVal);
  
  /// Construct the If.
  Stmt *If =
    new (C) IfStmt(C, SourceLocation(), 0, Comparison, Body,
                   SourceLocation(), Else);
  
  return If;  
}

//===----------------------------------------------------------------------===//
// Detection for whether fake ASTs can/should be created
//===----------------------------------------------------------------------===//

template<std::size_t Len>
static bool isNamed(const NamedDecl *ND, const char (&Str)[Len]) {
  IdentifierInfo *II = ND->getIdentifier();
  return II && II->isStr(Str);
}

static bool isNamespaceStd(const DeclContext *DC) {
  const NamespaceDecl *ND = dyn_cast<NamespaceDecl>(DC->getRedeclContext());
  return ND && isNamed(ND, "std") &&
         ND->getParent()->getRedeclContext()->isTranslationUnit();
}

typedef Stmt *(*FunctionFarmer)(ASTContext &C, const FunctionDecl *FD);

static FunctionFarmer getFunctionFarmerForCxxMethod(const CXXMethodDecl *MD)
{
  // get the class decl
  const CXXRecordDecl *RD = MD->getParent();

  if (isNamespaceStd(RD->getRedeclContext())) {
    if (isNamed(RD, "basic_string")) {

    }
  }
  return NULL;
}

static FunctionFarmer getFunctionFarmerForGlobalCFunction(const FunctionDecl *FD)
{
  if (FD->getIdentifier() == 0)
    return NULL;

  StringRef Name = FD->getName();
  if (Name.empty())
    return NULL;

  if (Name.startswith("OSAtomicCompareAndSwap") ||
      Name.startswith("objc_atomicCompareAndSwap")) {
    return create_OSAtomicCompareAndSwap;
  }
  else {
    return llvm::StringSwitch<FunctionFarmer>(Name)
          .Case("dispatch_sync", create_dispatch_sync)
          .Case("dispatch_once", create_dispatch_once)
        .Default(NULL);
  }
}

static FunctionFarmer getFunctionFarmer(const FunctionDecl *FD)
{
  // C++ member function
  if (const CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(FD)) {
    return getFunctionFarmerForCxxMethod(MD);
  }

  // One day: check for handled non-member C++ functions

  const DeclContext *DC = FD->getDeclContext()->getRedeclContext();
  if (DC->isTranslationUnit()) {
    // Global C functions now, which cannot be in a namespace
    return getFunctionFarmerForGlobalCFunction(FD);
  }

  return NULL;
}

bool BodyFarm::canAutosynthesize(const FunctionDecl *FD) const
{
  FD = FD->getCanonicalDecl();
  return getFunctionFarmer(FD);
}

Stmt *BodyFarm::getBody(const FunctionDecl *FD) {
  FD = FD->getCanonicalDecl();
  
  // use cached one if we have one
  Optional<Stmt *> &Val = Bodies[FD];
  if (Val.hasValue())
    return Val.getValue();
  
  Val = 0;
  
  FunctionFarmer FF = getFunctionFarmer(FD);
  if (FF) { 
    Val = FF(C, FD); 
  }

  return Val.getValue();
}
