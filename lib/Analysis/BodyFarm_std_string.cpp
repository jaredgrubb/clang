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
    static FunctionDecl* get_FD_csa_hook_content_set(ASTContext &C, ASTMaker &M);
    static FunctionDecl* get_FD_csa_hook_content_get_size(ASTContext &C, ASTMaker &M);

    static CallExpr* call_csa_hook_content_set(ASTContext &C, ASTMaker &M, CXXThisExpr *This, Expr *Content, Expr *Size);
    static CallExpr* call_csa_hook_content_get_size(ASTContext &C, ASTMaker &M, CXXThisExpr *This);

  protected: // FunctionDecl hooks:
    static FunctionDecl *FD_csa_hook_content_set;
    static FunctionDecl *FD_csa_hook_content_get_size;
  };
}

FunctionDecl *StdStringBodyFarm::FD_csa_hook_content_set = 0;
FunctionDecl *StdStringBodyFarm::FD_csa_hook_content_get_size = 0;

FunctionDecl* StdStringBodyFarm::get_FD_csa_hook_content_set(ASTContext &C, ASTMaker &M)
{
  if (!FD_csa_hook_content_set) {
    QualType ArgTypes[3] = {
      C.getPointerType(C.getConstType(C.VoidTy)), 
      C.getPointerType(C.getConstType(C.CharTy)), 
      C.getSizeType()
    };

    FD_csa_hook_content_set = M.makeFunction("_csa_hook_content_set", C.VoidTy, ArgTypes);
  }
  return FD_csa_hook_content_set;
}

FunctionDecl* StdStringBodyFarm::get_FD_csa_hook_content_get_size(ASTContext &C, ASTMaker &M)
{
  if (!FD_csa_hook_content_get_size) {
    QualType ArgTypes[1] = {
      C.getPointerType(C.getConstType(C.VoidTy))
    };

    FD_csa_hook_content_get_size = M.makeFunction("_csa_hook_content_get_size", C.getSizeType(), ArgTypes);
  }
  return FD_csa_hook_content_get_size;
}

CallExpr* StdStringBodyFarm::call_csa_hook_content_get_size(ASTContext &C, ASTMaker &M, CXXThisExpr *This)
{
  FunctionDecl* FD = get_FD_csa_hook_content_get_size(C, M);
  Expr* Args[1] = { This };
  return M.makeCall(FD, Args);
}

CallExpr* StdStringBodyFarm::call_csa_hook_content_set(ASTContext &C, ASTMaker &M, 
  CXXThisExpr *This, Expr *Content, Expr *Size)
{
  FunctionDecl* FD = get_FD_csa_hook_content_set(C, M);
  Expr* Args[3] = { This, Content, Size };
  return M.makeCall(FD, Args);
}

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

    case 1:
      // string::string(const allocator_type& a);
      // string::string(const string& str);
      // string::string(string&& str)
      // string::string(const_pointer s);
      // string::string(initializer_list<value_type>);

    case 2:
      // string::string(const string& str, size_type pos);
      // string::string(const_pointer s, const allocator_type&);
      // string::string(const_pointer s, size_type n);
      // string::string(InputIterator begin, InputIterator end);
      // string::string(initializer_list<value_type>, const Allocator& = Allocator());
      // string::string(const string&, const Allocator&);
      // string::string(string&&, const Allocator&);

    case 3:
      // string::string(const string& str, size_type pos, size_type n);
      // string::string(const_pointer s, size_type n, const allocator_type& a);
      // string::string(InputIterator begin, InputIterator end, const allocator_type&);


    case 4:
      // string::string(const string& str, size_type pos, size_type n, const allocator_type&);

    default:
      return NULL;
  }
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
