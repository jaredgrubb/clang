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

using namespace clang;

namespace {
  class StdStringBodyFarm {
  public:
    static Stmt *create_ctor_body(ASTContext &C, const FunctionDecl *D);

  protected:
    // ignore allocator parameters:
    static Stmt *create_ctor_default(ASTContext &C, const FunctionDecl *D);
    static Stmt *create_ctor_copy(ASTContext &C, const FunctionDecl *D);
    static Stmt *create_ctor_move(ASTContext &C, const FunctionDecl *D);
    static Stmt *create_ctor_char_ptr(ASTContext &C, const FunctionDecl *D);
    static Stmt *create_ctor_initializer_list(ASTContext &C, const FunctionDecl *D);
    static Stmt *create_ctor_copy_with_pos(ASTContext &C, const FunctionDecl *D);
    static Stmt *create_ctor_copy_with_pos_and_size(ASTContext &C, const FunctionDecl *D);
    static Stmt *create_ctor_char_ptr_with_size(ASTContext &C, const FunctionDecl *D);
    static Stmt *create_ctor_input_iterator_pair(ASTContext &C, const FunctionDecl *D);  
    };
}

BodyFarm::FunctionFarmer getFunctionFarmerForStdString(const CXXMethodDecl *D)
{
  return &StdStringBodyFarm::create_ctor_body;
}


/// Create a fake body for dispatch_once.
Stmt *StdStringBodyFarm::create_ctor_body(ASTContext &C, const FunctionDecl *D) {
  switch (D->param_size())
  {
    case 0:
      // string::string()
      return create_ctor_body_default(C,D);

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