//==- NullPtrCheckAfterDereference.cpp - Ptr Check after Deref -*- C++ --*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//

//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Analysis/Analyses/PostOrderCFGView.h"
#include "clang/Analysis/Analyses/NullPtrCheckAfterDereference.h"
#include "clang/Analysis/AnalysisContext.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/DomainSpecific/ObjCNoReturn.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/PackedVector.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/PriorityQueue.h"
#include "llvm/Support/SaveAndRestore.h"
#include <utility>

//TODO:remove
//#include <llvm/Support/Debug.h>
#include "llvm/Support/raw_ostream.h"
#include "clang/Basic/LangOptions.h" //test:cfg.dump

using namespace clang;



//------------------------------------------------------------------------====//
// dataflow worklist for NullPtrCheckAfterDereference
//====------------------------------------------------------------------------//
namespace {
class DataflowWorklist {
  PostOrderCFGView::iterator PO_I, PO_E;
  SmallVector<const CFGBlock *, 20> worklist;
  llvm::BitVector enqueuedBlocks;
public:
  DataflowWorklist(const CFG &cfg, PostOrderCFGView &view)
    : PO_I(view.begin()), PO_E(view.end()),
      enqueuedBlocks(cfg.getNumBlockIDs(), true) {
        // Treat the first block as already analyzed.
        if (PO_I != PO_E) {
          assert(*PO_I == &cfg.getEntry());
          enqueuedBlocks[(*PO_I)->getBlockID()] = false;
          ++PO_I;
        }
      }
  
  void enqueueSuccessors(const CFGBlock *block);
  void enqueueBlock(const clang::CFGBlock *block); //test
  const CFGBlock *dequeue();
};
}

void DataflowWorklist::enqueueSuccessors(const clang::CFGBlock *block) {
  for (CFGBlock::const_succ_iterator I = block->succ_begin(),
       E = block->succ_end(); I != E; ++I) {
    const CFGBlock *Successor = *I;
    if (!Successor || enqueuedBlocks[Successor->getBlockID()])
      continue;
    worklist.push_back(Successor);
    enqueuedBlocks[Successor->getBlockID()] = true;
  }
}

const CFGBlock *DataflowWorklist::dequeue() {
  const CFGBlock *B = nullptr;

  // First dequeue from the worklist.  This can represent
  // updates along backedges that we want propagated as quickly as possible.
  if (!worklist.empty())
    B = worklist.pop_back_val();

  // Next dequeue from the initial reverse post order.  This is the
  // theoretical ideal in the presence of no back edges.
  else if (PO_I != PO_E) {
    B = *PO_I;
    ++PO_I;
  }
  else {
    return nullptr;
  }

  assert(enqueuedBlocks[B->getBlockID()] == true);
  enqueuedBlocks[B->getBlockID()] = false;
  return B;
}

//TEST-DELETE
void DataflowWorklist::enqueueBlock(const clang::CFGBlock *block) {
  llvm::errs() << "Enqeue block with ID=" << block->getBlockID() << "\n";
  if (enqueuedBlocks[block->getBlockID()])
    return;
  worklist.push_back(block);
  enqueuedBlocks[block->getBlockID()] = true;
}  
//TEST-DELETE end

static const Expr *stripCasts(ASTContext &C, const Expr *Ex) {
  while (Ex) {
    Ex = Ex->IgnoreParenNoopCasts(C);
    if (const CastExpr *CE = dyn_cast<CastExpr>(Ex)) {
      if (CE->getCastKind() == CK_LValueBitCast) {
        Ex = CE->getSubExpr();
        continue;
      }
    }
    break;
  }
  return Ex;
}

const VarDecl* findVar(const Expr *E, const DeclContext *DC) {
  if (not E->getType()->isPointerType())
    return nullptr;
  if (const DeclRefExpr *DRE =
        dyn_cast<DeclRefExpr>(stripCasts(DC->getParentASTContext(), E)))
    if (const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())){
      return VD;
    }
  return nullptr;
}

const VarDecl* findDeref(const Expr *E, const DeclContext *DC) {
  // The result of a ?: could also be an lvalue.
  E = E->IgnoreParenCasts();
  if (const ConditionalOperator *CO = dyn_cast<ConditionalOperator>(E)) {
    const VarDecl* VD = findDeref(CO->getTrueExpr(), DC);
    if(VD) return VD;
    VD = findDeref(CO->getFalseExpr(), DC);
    return VD;
  }

  if (const BinaryConditionalOperator *BCO =
          dyn_cast<BinaryConditionalOperator>(E)) {
    return findDeref(BCO->getFalseExpr(), DC);
  }

  if (const OpaqueValueExpr *OVE = dyn_cast<OpaqueValueExpr>(E)) {
    return findDeref(OVE->getSourceExpr(), DC);
  }

  if (const MemberExpr *ME = dyn_cast<MemberExpr>(E)) {
    if (VarDecl *VD = dyn_cast<VarDecl>(ME->getMemberDecl())) {
      if (!VD->isStaticDataMember())
        return findDeref(ME->getBase(), DC);
    }
  }

  if (const BinaryOperator *BO = dyn_cast<BinaryOperator>(E)) {
    const VarDecl* VD;
    switch (BO->getOpcode()) {
    case BO_PtrMemD:
    case BO_PtrMemI:
      VD = findDeref(BO->getLHS(), DC);
      if(VD) 
        return VD;
    case BO_Comma:
      return findDeref(BO->getRHS(), DC);
      
    default:
      return nullptr;
    }
  }

  if (const UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
    if(UO->getOpcode() == UO_Deref) {
      const VarDecl* VD = findVar(UO->getSubExpr()->IgnoreParenCasts(), DC);      
      llvm::errs() << "Deref on: "; VD->dump(); llvm::errs() << "\n";
    }
    return nullptr;
  }

}

//only returns VarDecl for (ptr) or (!ptr)
const VarDecl* findPtrCheck(const Expr *E, const DeclContext *DC) {
  E = E->IgnoreParenCasts();
  
  //(!ptr)
  if (const UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
    if(UO->getOpcode() == UO_Not) {
      const VarDecl* VD = findVar(UO->getSubExpr()->IgnoreParenCasts(), DC);      
      llvm::errs() << "Check on: "; VD->dump(); llvm::errs() << "\n";
    }
    return nullptr;
  }  
  
  //(ptr)
  return findVar(E, DC);      
}

//------------------------------------------------------------------------====//
// Transfer function for NullPtrCheckAfterDereference
//====------------------------------------------------------------------------//

namespace {
class TransferFunctions : public StmtVisitor<TransferFunctions> {
  
  const CFG &cfg;
  const CFGBlock *block;
  AnalysisDeclContext &ac;


public:
  TransferFunctions( const CFG &cfg,
                    const CFGBlock *block, AnalysisDeclContext &ac)
    : cfg(cfg), block(block), ac(ac){}

  void VisitBinaryOperator(BinaryOperator *bo);
  void VisitBlockExpr(BlockExpr *be);
  void VisitCallExpr(CallExpr *ce);
  void VisitDeclRefExpr(DeclRefExpr *dr);
  void VisitDeclStmt(DeclStmt *ds);
  void VisitObjCForCollectionStmt(ObjCForCollectionStmt *FS);
  void VisitObjCMessageExpr(ObjCMessageExpr *ME);
  
  void VisitConditionExpr(const Expr *ex);

  const VarDecl* findVar(const Expr *ex) {
    return ::findVar(ex, cast<DeclContext>(ac.getDecl()));
  }  
  
  const VarDecl* findDeref(const Expr *ex) {
    return ::findDeref(ex, cast<DeclContext>(ac.getDecl()));
  }  
  
  const VarDecl* findPtrCheck(const Expr *ex){
    return ::findPtrCheck(ex, cast<DeclContext>(ac.getDecl()));
  }
};
}



void TransferFunctions::VisitBlockExpr(BlockExpr *be) {
}

void TransferFunctions::VisitCallExpr(CallExpr *ce) {

}

void TransferFunctions::VisitDeclRefExpr(DeclRefExpr *dr) {
//   dr->dump();
}

void TransferFunctions::VisitBinaryOperator(BinaryOperator *BO) {
  llvm::errs() << "Binary: "; BO->dump(); llvm::errs() << "\n";
  
  if (BO->getOpcode() == BO_Assign) {
    const VarDecl* VD = findVar(BO->getLHS());
    if(VD) {
      llvm::errs() << "Assignment to: "; VD->dump(); llvm::errs() << "\n";
    }
  }
  
  findDeref(BO->getLHS());
  findDeref(BO->getRHS());
}

void TransferFunctions::VisitConditionExpr(const Expr *ex) {
  const VarDecl* VD = findPtrCheck(ex);
  if(VD) {
    llvm::errs() << "Check to: "; VD->dump(); llvm::errs() << "\n";
  }
}


void TransferFunctions::VisitDeclStmt(DeclStmt *DS) {
  

}

void TransferFunctions::VisitObjCMessageExpr(ObjCMessageExpr *ME) {
}

void TransferFunctions::VisitObjCForCollectionStmt(ObjCForCollectionStmt *FS) {
}



  

#define DEBUG_LOGGING 0

static void runOnBlock(const CFGBlock *block, const CFG &cfg,
                       AnalysisDeclContext &ac) {
  llvm::errs() << "Block with ID=" << block->getBlockID() << "\n";
  
  TransferFunctions tf(cfg, block, ac);

  for (CFGBlock::const_iterator I = block->begin(), E = block->end(); 
       I != E; ++I) {
    if (Optional<CFGStmt> cs = I->getAs<CFGStmt>())
      tf.Visit(const_cast<Stmt*>(cs->getStmt()));
  }
  

  if (const IfStmt *IfNode =
    dyn_cast_or_null<IfStmt>(block->getTerminator().getStmt())) {
     llvm::errs() << "If: "; IfNode->dump(); llvm::errs() << "\n";
     tf.VisitConditionExpr(IfNode->getCond());
  }
  
  if (const ConditionalOperator *TernaryOpNode =
    dyn_cast_or_null<ConditionalOperator>(block->getTerminator().getStmt())) {
     llvm::errs() << "Ternary: "; TernaryOpNode->dump(); llvm::errs() << "\n";
     tf.VisitConditionExpr(TernaryOpNode->getCond());
  }  

}

void clang::runNullPtrCheckAfterDereferenceAnalysis(
    const DeclContext &dc,
    const CFG &cfg,
    AnalysisDeclContext &ac,
    NullPtrCheckAfterDereferenceHandler &handler) {
  llvm::errs() << "NullPtrCheckAfterDereference,\n";
  
  LangOptions L;
  llvm::errs() << "CFG: "; cfg.dump(L, false); llvm::errs() << "\n";
  
  
  
  DataflowWorklist worklist(cfg, *ac.getAnalysis<PostOrderCFGView>());
//   worklist.enqueueSuccessors(&cfg.getEntry());
  worklist.enqueueBlock(&cfg.getEntry());
  
  
  while (const CFGBlock *block = worklist.dequeue()){
    runOnBlock(block, cfg, ac );
  }
}

