#!/bin/bash 

function test(){
  echo ../../../llvm_install/bin/clang++ $1.cpp
  cat $1.cpp &> $1.ver
  ../../../llvm_install/bin/clang++ $1.cpp &>> $1.ver
}


test test0
test test1
test test2
test test3

rm ../code/tools -rf
mkdir ../code/tools/
mkdir ../code/tools/clang/
mkdir ../code/tools/clang/lib/
mkdir ../code/tools/clang/lib/Analysis
cp /home/art/Projects/llvm/tools/clang/lib/Analysis/NullPtrCheckAfterDereference.cpp ../code/tools/clang/lib/Analysis/.

mkdir ../code/tools/clang/include/
mkdir ../code/tools/clang/include/clang/
mkdir ../code/tools/clang/include/clang/Analysis/
mkdir ../code/tools/clang/include/clang/Analysis/Analyses
cp  /home/art/Projects/llvm/tools/clang/include/clang/Analysis/Analyses/NullPtrCheckAfterDereference.h ../code/tools/clang/include/clang/Analysis/Analyses/.

mkdir ../code/tools/clang/lib/Sema
cp /home/art/Projects/llvm/tools/clang/lib/Sema/AnalysisBasedWarnings.cpp ../code/tools/clang/lib/Sema/.