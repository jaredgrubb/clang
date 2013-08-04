//== ASTMaker.h - Factory for conjuring up fake bodies -------------*- C++ -*-//
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

#ifndef LLVM_CLANG_ANALYSIS_BODYFARM_H
#define LLVM_CLANG_ANALYSIS_BODYFARM_H

#include "clang/Basic/LLVM.h"

namespace clang {
  class ASTContext;
  class Expr;
  class DeclRefExpr;
  class ImplicitCastExpr;
  class ObjCBoolLiteralExpr;
  class UnaryOperator;
  class BinaryOperator;
  class QualType;
  class Stmt;
  class CompoundStmt;
  class ReturnStmt;

class ASTMaker {
public:
  ASTMaker(ASTContext &C) : C(C) {}
 
 public: // validation helpers

 
 public: // creation functions

  /// Create a new BinaryOperator representing a simple assignment.
  BinaryOperator *makeAssignment(const Expr *LHS, const Expr *RHS, QualType Ty);
  
  /// Create a new BinaryOperator representing a comparison.
  BinaryOperator *makeComparison(const Expr *LHS, const Expr *RHS,
                                 BinaryOperator::Opcode Op);
  
  /// Create a new compound stmt using the provided statements.
  CompoundStmt *makeCompound(ArrayRef<Stmt*>);
  
  /// Create a new DeclRefExpr for the referenced variable.
  DeclRefExpr *makeDeclRefExpr(const VarDecl *D);
  
  /// Create a new UnaryOperator representing a dereference.
  UnaryOperator *makeDereference(const Expr *Arg, QualType Ty);
  
  /// Create an implicit cast for an integer conversion.
  Expr *makeIntegralCast(const Expr *Arg, QualType Ty);
  
  /// Create an implicit cast to a builtin boolean type.
  ImplicitCastExpr *makeIntegralCastToBoolean(const Expr *Arg);
  
  // Create an implicit cast for lvalue-to-rvaluate conversions.
  ImplicitCastExpr *makeLvalueToRvalue(const Expr *Arg, QualType Ty);
  
  /// Create an Objective-C bool literal.
  ObjCBoolLiteralExpr *makeObjCBool(bool Val);
  
  /// Create a Return statement.
  ReturnStmt *makeReturn(const Expr *RetVal);
  
private:
  ASTContext &C;
};

}

#endif
