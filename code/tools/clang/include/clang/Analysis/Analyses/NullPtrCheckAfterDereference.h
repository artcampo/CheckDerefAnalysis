//===- NullPtrCheckAfterDereference.h --------------------------*- C++ --*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the check for a ptr check against null after has been
// dereferenced in all paths leading to it.
//
//===----------------------------------------------------------------------===//


#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_NULLPTRCHECKAFTERDEREFERENCE_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_NULLPTRCHECKAFTERDEREFERENCE_H

#include "clang/AST/Stmt.h"
#include "llvm/ADT/SmallVector.h"

namespace clang {

class AnalysisDeclContext;
class CFG;
class DeclContext;
class Expr;
class VarDecl;

class NullPtrCheckAfterDereferenceHandler {
public:
  NullPtrCheckAfterDereferenceHandler() {}
  virtual ~NullPtrCheckAfterDereferenceHandler(){};

};

/*
struct UninitVariablesAnalysisStats {
  unsigned NumVariablesAnalyzed;
  unsigned NumBlockVisits;
};
*/

void runNullPtrCheckAfterDereferenceAnalysis(const DeclContext &dc,
                                  const CFG &cfg,
                                  AnalysisDeclContext &ac,
                                  NullPtrCheckAfterDereferenceHandler &handler);
                                  //UninitVariablesAnalysisStats &stats);

}
#endif
