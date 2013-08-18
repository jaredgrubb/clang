//== ASTMaker.cpp  - Factory for conjuring up fake AST nodes ----------*- C++ -*-//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// ASTMaker is a factory for creating faux implementations for functions/methods
// for analysis purposes.
//
//===----------------------------------------------------------------------===//

#include "ASTMaker.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprObjC.h"
#include "clang/AST/ExprCXX.h"

using namespace clang;

BinaryOperator *ASTMaker::makeAssignment(const Expr *LHS, const Expr *RHS,
                                         QualType Ty) {
 return new (C) BinaryOperator(const_cast<Expr*>(LHS), const_cast<Expr*>(RHS),
                               BO_Assign, Ty, VK_RValue,
                               OK_Ordinary, SourceLocation(), false);
}

BinaryOperator *ASTMaker::makeComparison(const Expr *LHS, const Expr *RHS,
                                         BinaryOperatorKind Op) {
  assert(BinaryOperator::isLogicalOp(Op) ||
         BinaryOperator::isComparisonOp(Op));
  return new (C) BinaryOperator(const_cast<Expr*>(LHS),
                                const_cast<Expr*>(RHS),
                                Op,
                                C.getLogicalOperationType(),
                                VK_RValue,
                                OK_Ordinary, SourceLocation(), false);
}

CompoundStmt *ASTMaker::makeCompound(ArrayRef<Stmt *> Stmts) {
  return new (C) CompoundStmt(C, Stmts, SourceLocation(), SourceLocation());
}

DeclRefExpr *ASTMaker::makeDeclRefExpr(const VarDecl *D) {
  DeclRefExpr *DR =
    DeclRefExpr::Create(/* Ctx = */ C,
                        /* QualifierLoc = */ NestedNameSpecifierLoc(),
                        /* TemplateKWLoc = */ SourceLocation(),
                        /* D = */ const_cast<VarDecl*>(D),
                        /* isEnclosingLocal = */ false,
                        /* NameLoc = */ SourceLocation(),
                        /* T = */ D->getType(),
                        /* VK = */ VK_LValue);
  return DR;
}

UnaryOperator *ASTMaker::makeDereference(const Expr *Arg, QualType Ty) {
  return new (C) UnaryOperator(const_cast<Expr*>(Arg), UO_Deref, Ty,
                               VK_LValue, OK_Ordinary, SourceLocation());
}

ImplicitCastExpr *ASTMaker::makeLvalueToRvalue(const Expr *Arg, QualType Ty) {
  return ImplicitCastExpr::Create(C, Ty, CK_LValueToRValue,
                                  const_cast<Expr*>(Arg), 0, VK_RValue);
}

Expr *ASTMaker::makeIntegralCast(const Expr *Arg, QualType Ty) {
  if (Arg->getType() == Ty)
    return const_cast<Expr*>(Arg);
  
  return ImplicitCastExpr::Create(C, Ty, CK_IntegralCast,
                                  const_cast<Expr*>(Arg), 0, VK_RValue);
}

ImplicitCastExpr *ASTMaker::makeIntegralCastToBoolean(const Expr *Arg) {
  return ImplicitCastExpr::Create(C, C.BoolTy, CK_IntegralToBoolean,
                                  const_cast<Expr*>(Arg), 0, VK_RValue);
}

ObjCBoolLiteralExpr *ASTMaker::makeObjCBool(bool Val) {
  QualType Ty = C.getBOOLDecl() ? C.getBOOLType() : C.ObjCBuiltinBoolTy;
  return new (C) ObjCBoolLiteralExpr(Val, Ty, SourceLocation());
}

IntegerLiteral *ASTMaker::makeInteger(int Val) {
  return IntegerLiteral::Create(C, llvm::APInt(C.getTypeSize(C.IntTy), Val),
                                C.IntTy, SourceLocation());
}

ImplicitCastExpr *ASTMaker::makeNullPtr(QualType PointerType) {
  return ImplicitCastExpr::Create(C, PointerType, CK_NullToPointer,
                                  makeInteger(0), 0, VK_RValue);
}

ReturnStmt *ASTMaker::makeReturn(const Expr *RetVal) {
  return new (C) ReturnStmt(SourceLocation(), const_cast<Expr*>(RetVal), 0);
}

CallExpr *ASTMaker::makeCall(FunctionDecl *Function, ArrayRef<Expr*> Args) {
  DeclRefExpr *DR = makeDeclRefExpr(Function);
  ImplicitCastExpr *ICE = ImplicitCastExpr::Create(C, Function->getType(), 
                            CK_FunctionToPointerDecay, const_cast<Expr*>(DR), 
                            0, VK_RValue);

  QualType ResultType = Function->getCallResultType(C);
  return new (C) CallExpr(C, ICE, Args, ResultType, VK_RValue, SourceLocation());
}

CXXMemberCallExpr *ASTMaker::makeCxxMemberCall(Expr* Object, CXXMethodDecl *Method, ArrayRef<Expr*> Args) {
  MemberExpr* ME = new (C) MemberExpr(
    Object,
    false,  // arrow
    Method,
    SourceLocation(),
    C.BoundMemberTy,
    VK_RValue, // right?
    OK_Ordinary
  );

  QualType ResultType = Method->getResultType();
  ExprValueKind VK = Expr::getValueKindForType(ResultType);
  ResultType = ResultType.getNonLValueExprType(C);

  return new (C) CXXMemberCallExpr(C, ME, Args, ResultType, VK, SourceLocation());
}
