//== BodyFarm_std_string.cpp  - std::string Body Factory  ----------*- C++ -*-//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// BodyFarm is a factory for creating faux implementations for functions/methods
// for analysis purposes. This version creates bodies for std::string.
//
//===----------------------------------------------------------------------===//

#include "BodyFarm.h"
#include "ASTMaker.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprObjC.h"

#include "llvm/ADT/StringSwitch.h"

/// XXXXXXX: REMOVE!
#include <iostream>

using namespace clang;

namespace {
  class StdStringBodyFarm {
  public:
    static Stmt *create_ctor(ASTContext &C, const CXXConstructorDecl *D);
    static Stmt *create_dtor(ASTContext &C, const CXXDestructorDecl *D);

    static Stmt *create_size(ASTContext &C, const CXXMethodDecl *D);
    static Stmt *create_length(ASTContext &C, const CXXMethodDecl *D);
    static Stmt *create_empty(ASTContext &C, const CXXMethodDecl *D);

    static Stmt *create_assign(ASTContext &C, const CXXMethodDecl *D);

  protected:

  protected:
    // ignore allocator parameters:
    static Stmt *create_ctor_default(ASTContext &C, const CXXConstructorDecl *D);
    static Stmt *create_ctor_copy(ASTContext &C, const CXXConstructorDecl *D);
    static Stmt *create_ctor_move(ASTContext &C, const CXXConstructorDecl *D);
    static Stmt *create_ctor_char_ptr(ASTContext &C, const CXXConstructorDecl *D);
    static Stmt *create_ctor_initializer_list(ASTContext &C, const CXXConstructorDecl *D);
    static Stmt *create_ctor_copy_with_pos(ASTContext &C, const CXXConstructorDecl *D);
    static Stmt *create_ctor_copy_with_pos_and_size(ASTContext &C, const CXXConstructorDecl *D);
    static Stmt *create_ctor_char_ptr_with_size(ASTContext &C, const CXXConstructorDecl *D);
    static Stmt *create_ctor_input_iterator_pair(ASTContext &C, const CXXConstructorDecl *D);

  protected:
    static FunctionDecl* get_FD_csa_hook_ptr_require_nonnull(ASTContext &C, ASTMaker &M);
    static FunctionDecl* get_FD_csa_hook_content_set(ASTContext &C, ASTMaker &M);
    static FunctionDecl* get_FD_csa_hook_content_get_size(ASTContext &C, ASTMaker &M);
    static FunctionDecl* get_FD_strlen(ASTContext &C, ASTMaker &M);

    static CallExpr* call_csa_hook_ptr_require_nonnull(ASTContext &C, ASTMaker &M, Expr *Pointer);
    static CallExpr* call_csa_hook_content_set(ASTContext &C, ASTMaker &M, CXXThisExpr *This, Expr *Content, Expr *Size);
    static CallExpr* call_csa_hook_content_get_size(ASTContext &C, ASTMaker &M, CXXThisExpr *This);
    static CallExpr* call_strlen(ASTContext &C, ASTMaker &M, Expr *Pointer);

  protected: // FunctionDecl hooks:
    static FunctionDecl *FD_csa_hook_ptr_require_nonnull;
    static FunctionDecl *FD_csa_hook_content_set;
    static FunctionDecl *FD_csa_hook_content_get_size;
    static FunctionDecl *FD_strlen;
  };
}

//===----------------------------------------------------------------------===//
// Creation functions for all the hook singletons.
//===----------------------------------------------------------------------===//

FunctionDecl *StdStringBodyFarm::FD_csa_hook_ptr_require_nonnull = 0;
FunctionDecl *StdStringBodyFarm::FD_csa_hook_content_set = 0;
FunctionDecl *StdStringBodyFarm::FD_csa_hook_content_get_size = 0;
FunctionDecl *StdStringBodyFarm::FD_strlen = 0;

FunctionDecl* StdStringBodyFarm::get_FD_csa_hook_ptr_require_nonnull(ASTContext &C, ASTMaker &M)
{
  if (!FD_csa_hook_ptr_require_nonnull) {
    QualType ArgTypes[1] = {
      C.getPointerType(C.getConstType(C.VoidTy))  // const void*
    };

    FD_csa_hook_ptr_require_nonnull = M.makeFunction("_csa_hook_ptr_require_nonnull", C.getSizeType(), ArgTypes);
  }
  return FD_csa_hook_ptr_require_nonnull;
}

FunctionDecl* StdStringBodyFarm::get_FD_csa_hook_content_set(ASTContext &C, ASTMaker &M)
{
  if (!FD_csa_hook_content_set) {
    QualType ArgTypes[3] = {
      C.getPointerType(C.getConstType(C.VoidTy)),   // const void*
      C.getPointerType(C.getConstType(C.CharTy)),   // const char*
      C.getSizeType()  // size_t
    };

    FD_csa_hook_content_set = M.makeFunction("_csa_hook_content_set", C.VoidTy, ArgTypes);
  }
  return FD_csa_hook_content_set;
}

FunctionDecl* StdStringBodyFarm::get_FD_csa_hook_content_get_size(ASTContext &C, ASTMaker &M)
{
  if (!FD_csa_hook_content_get_size) {
    QualType ArgTypes[1] = {
      C.getPointerType(C.getConstType(C.VoidTy))  // const void*
    };

    FD_csa_hook_content_get_size = M.makeFunction("_csa_hook_content_get_size", C.getSizeType(), ArgTypes);
  }
  return FD_csa_hook_content_get_size;
}

FunctionDecl* StdStringBodyFarm::get_FD_strlen(ASTContext &C, ASTMaker &M)
{
  if (!FD_strlen) {
    // XX: probably should try to look for the real one, but this works for now:
    QualType ArgTypes[1] = {
      C.getPointerType(C.getConstType(C.CharTy))  // const char*
    };

    FD_strlen = M.makeFunction("strlen", C.getSizeType(), ArgTypes);
  }
  return FD_strlen;
}

//===----------------------------------------------------------------------===//
// Creation calls out to the various hooks.
//===----------------------------------------------------------------------===//

CallExpr* StdStringBodyFarm::call_csa_hook_ptr_require_nonnull(ASTContext &C, ASTMaker &M, Expr *Pointer)
{
  FunctionDecl* FD = get_FD_csa_hook_ptr_require_nonnull(C, M);
  Expr* Args[1] = { Pointer };
  return M.makeCall(FD, Args);  
}

CallExpr* StdStringBodyFarm::call_csa_hook_content_set(ASTContext &C, ASTMaker &M, 
  CXXThisExpr *This, Expr *Content, Expr *Size)
{
  FunctionDecl* FD = get_FD_csa_hook_content_set(C, M);
  Expr* Args[3] = { This, Content, Size };
  return M.makeCall(FD, Args);
}

CallExpr* StdStringBodyFarm::call_csa_hook_content_get_size(ASTContext &C, ASTMaker &M, CXXThisExpr *This)
{
  FunctionDecl* FD = get_FD_csa_hook_content_get_size(C, M);
  Expr* Args[1] = { This };
  return M.makeCall(FD, Args);
}

CallExpr* StdStringBodyFarm::call_strlen(ASTContext &C, ASTMaker &M, Expr *Pointer)
{
  FunctionDecl* FD = get_FD_strlen(C, M);
  Expr* Args[1] = { Pointer };
  return M.makeCall(FD, Args);
}

//===----------------------------------------------------------------------===//
// Creation functions for faux ASTs.
//===----------------------------------------------------------------------===//

template<std::size_t Len>
static bool isNamed(const NamedDecl *ND, const char (&Str)[Len]) {
  IdentifierInfo *II = ND->getIdentifier();
  return II && II->isStr(Str);
}

template<std::size_t Len>
static CXXMethodDecl *getMember(const CXXRecordDecl* RD, QualType SigType, const char (&Str)[Len]) {
  CXXRecordDecl::method_iterator i = RD->method_begin();
  CXXRecordDecl::method_iterator e = RD->method_end();
  for( ; i != e; ++i) {
    if (i->getType() != SigType) {
      continue;
    }

    if (isNamed(*i, Str)) {
      return *i;
    }
  }

  return NULL;
}

bool isConstCharPointer(ASTContext &C, const ParmVarDecl *P) {
  llvm::outs().changeColor(llvm::raw_ostream::MAGENTA)
  << "   -- isConstCharPointer: ";
  C.getPointerType(C.getConstType(C.CharTy)).dump();
  llvm::outs() << "\n        - getLocalFastQualifiers = " << C.getPointerType(C.getConstType(C.CharTy)).getLocalFastQualifiers();
  llvm::outs() << "\n        - split().Ty = " << C.getPointerType(C.getConstType(C.CharTy)).split().Ty;
  C.getPointerType(C.getConstType(C.CharTy)).split().Ty->dump();

  llvm::outs() << "\n    P = ";
  P->getOriginalType().dump();
  llvm::outs() << "\n        - getLocalFastQualifiers = " << P->getOriginalType().getLocalFastQualifiers();
  llvm::outs() << "\n        - split().Ty = " << P->getOriginalType().split().Ty;
  P->getOriginalType().split().Ty->dump();

  llvm::outs() << "\n        - desugared.Ty = " << P->getOriginalType().getDesugaredType(C).split().Ty;
  P->getOriginalType().getDesugaredType(C).dump();

  llvm::outs() << "\n        - desugared.Canon.Ty = " << P->getOriginalType().getDesugaredType(C).getCanonicalType().split().Ty;
  P->getOriginalType().getDesugaredType(C).getCanonicalType().dump();

  llvm::outs() << "\n        - CanonOrig.Ty = " << P->getOriginalType().getCanonicalType().split().Ty;
  llvm::outs() << "\n        - Canon.Ty = " << P->getType().getCanonicalType().split().Ty;


  llvm::outs() << "\n    same = " << (P->getType().getCanonicalType() == C.getPointerType(C.getConstType(C.CharTy)));
  llvm::outs() << "\n    same split = " << (P->getOriginalType().split() == C.getPointerType(C.getConstType(C.CharTy)).split());
  llvm::outs().resetColor();
  llvm::outs() << "\n";

  return P->getType().getCanonicalType() == C.getPointerType(C.getConstType(C.CharTy));
}

bool isAllocatorType(ASTContext &C, CXXMethodDecl const* D, const ParmVarDecl *P)
{
  const CXXRecordDecl* Class = D->getParent();
  assert(Class && "This isnt a method of std::basic_string?");

  const ClassTemplateSpecializationDecl *T = dyn_cast<ClassTemplateSpecializationDecl>(Class);
  assert(T && "std::basic_string is a template, right?");

  const TemplateArgumentList &TArgs = T->getTemplateArgs();
  const TemplateArgument& T2 = TArgs.get(2);

  QualType AllocTy = T2.getAsType().getCanonicalType();
  QualType PTy = P->getType().getCanonicalType();

  // check that they are equal up to const-ref
  return AllocTy == PTy.getNonReferenceType().getUnqualifiedType();
}

//===----------------------------------------------------------------------===//
// Creation functions for the fake std::string functions.
//===----------------------------------------------------------------------===//

Stmt *BodyFarm::createBodyForStdString(ASTContext &C, const FunctionDecl *D)
{
  if (const CXXConstructorDecl* CD = dyn_cast<CXXConstructorDecl>(D)) {
    return StdStringBodyFarm::create_ctor(C, CD);
  } else if (const CXXDestructorDecl* DD = dyn_cast<CXXDestructorDecl>(D)) {
    return StdStringBodyFarm::create_dtor(C, DD);
  } else if (const CXXMethodDecl* MD = dyn_cast<CXXMethodDecl>(D)) {
    IdentifierInfo *II = MD->getIdentifier();
    if (!II) {
      // fall through to protect against null
    } else if (II->isStr("size")) {
      return StdStringBodyFarm::create_size(C, MD);
    } else if (II->isStr("length")) {
      return StdStringBodyFarm::create_length(C, MD);
    } else if (II->isStr("empty")) {
      return StdStringBodyFarm::create_empty(C, MD);
    }
  }

  return NULL;
}

Stmt *StdStringBodyFarm::create_ctor(ASTContext &C, const CXXConstructorDecl *D) {
  switch (D->param_size())
  {
    case 0:
      // string::string()
      return create_ctor_default(C,D);

    case 1: {
      const ParmVarDecl *P0 = D->getParamDecl(0);
      if (!P0) break;

      // string::string(const allocator_type& a);
      // string::string(const string& str);
      // string::string(string&& str)

      // string::string(const_pointer s);
      if (isConstCharPointer(C, P0)) {
        return create_ctor_char_ptr(C,D);
      }

      // string::string(initializer_list<value_type>);

      break;
    }
    case 2: {
      const ParmVarDecl *P0 = D->getParamDecl(0);
      const ParmVarDecl *P1 = D->getParamDecl(1);
      if (!P0 || !P1) break;

      // string::string(const string& str, size_type pos);

      // string::string(const_pointer s, const allocator_type&);
      if (isConstCharPointer(C, P0)
        && isAllocatorType(C, D, P1))
      {
        return create_ctor_char_ptr(C,D);
      }

      // string::string(const_pointer s, size_type n);
      // string::string(InputIterator begin, InputIterator end);
      // string::string(initializer_list<value_type>, const Allocator& = Allocator());
      // string::string(const string&, const Allocator&);
      // string::string(string&&, const Allocator&);
      break;
    }
    case 3: {
      // string::string(const string& str, size_type pos, size_type n);
      // string::string(const_pointer s, size_type n, const allocator_type& a);
      // string::string(InputIterator begin, InputIterator end, const allocator_type&);


      break;
    }
    case 4: {
      // string::string(const string& str, size_type pos, size_type n, const allocator_type&);

      break;
    }
    default:
      break;
  }

  llvm::outs().changeColor(llvm::raw_ostream::MAGENTA);
  D->dump();
  llvm::outs().resetColor();
  llvm::outs() << "\n";

  return NULL;
}

Stmt *StdStringBodyFarm::create_ctor_default(ASTContext &C, const CXXConstructorDecl *D) {
  std::cout << "########### ****   default ctor    ##################" << std::endl;

  // 
  // synthesize:
  // 
  //   basic_string::basic_string() const {
  //       _csa_hook_content_set(this, NULL, 0);  // (1)
  //   }
  // 

  ASTMaker M(C);

  CXXThisExpr *This = new (C) CXXThisExpr(SourceLocation(), D->getThisType(C), true);

  // (1)
  QualType ConstCharPtrTy = C.getPointerType(C.getConstType(C.CharTy));
  Expr *Null = M.makeNullPtr(ConstCharPtrTy);
  Expr *Zero = M.makeInteger(0);
  CallExpr *CE_Set = call_csa_hook_content_set(C, M, This, Null, Zero);

  return CE_Set;
}

Stmt *StdStringBodyFarm::create_ctor_char_ptr(ASTContext &C, const CXXConstructorDecl *D) {
  std::cout << "########### ****   string-literal ctor    ##################" << std::endl;

  // 
  // synthesize:
  // 
  // basic_string::basic_string(const char* str) const {
  // {
  //     _csa_hook_ptr_require_nonnull(str);   // (1)
  //     _csa_hook_content_set(this, str, strlen(str));  // (2)
  // }
  // 

  ASTMaker M(C);

  CXXThisExpr *This = new (C) CXXThisExpr(SourceLocation(), D->getThisType(C), true);
  DeclRefExpr *Str = M.makeDeclRefExpr(D->getParamDecl(0));

  QualType ConstCharPtrTy = C.getPointerType(C.getConstType(C.CharTy));

  Stmt* Stmts[2];

  // (1)
  ImplicitCastExpr *ICE0 = M.makeLvalueToRvalue(Str, ConstCharPtrTy);
  Stmts[0] = call_csa_hook_ptr_require_nonnull(C, M, ICE0);

  // (2)
  CallExpr *Strlen = call_strlen(C, M, ICE0);
  Stmts[1] = call_csa_hook_content_set(C, M, This, ICE0, Strlen);

  return M.makeCompound(Stmts);
}

Stmt *StdStringBodyFarm::create_dtor(ASTContext &C, const CXXDestructorDecl *D) {
  std::cout << "########### ****   dtor    ##################" << std::endl;
  // intentionally return empty body; this is one of the primitives that we hook into
  return NULL;
}

Stmt *StdStringBodyFarm::create_size(ASTContext &C, const CXXMethodDecl *D) {
  std::cout << "########### ****   size    ##################" << std::endl;

  // 
  // synthesize:
  // 
  // size_type size() const
  // {
  //     return _csa_hook_content_get_size(this);  // (1)
  // }

  ASTMaker M(C);

  CXXThisExpr *This = new (C) CXXThisExpr(SourceLocation(), D->getThisType(C), true);

  // (1)
  CallExpr *CE_GetSize = call_csa_hook_content_get_size(C, M, This);

  return M.makeReturn(CE_GetSize);
}

Stmt *StdStringBodyFarm::create_length(ASTContext &C, const CXXMethodDecl *D) {
  // Size and length are the same thing, so just reuse it!
  return create_size(C, D);
}

Stmt *StdStringBodyFarm::create_empty(ASTContext &C, const CXXMethodDecl *D) {
  std::cout << "########### ****   empty    ##################" << std::endl;

  // 
  // synthesize:
  // 
  // bool empty() const {
  //     return _csa_hook_content_get_size(this) == 0;  // (1)
  // }

  ASTMaker M(C);

  CXXThisExpr *This = new (C) CXXThisExpr(SourceLocation(), D->getThisType(C), true);

  // (1)
  CallExpr *CE_GetSize = call_csa_hook_content_get_size(C, M, This);
  Expr *Comparison =
    M.makeComparison(
      CE_GetSize,
      M.makeInteger(0),
      BO_EQ);

  return M.makeReturn(Comparison);
}
