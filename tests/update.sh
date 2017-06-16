#!/bin/bash 

function test(){
  echo ../../../llvm_install/bin/clang++ $1.cpp
  cat $1.cpp &> $1.ver
  ../../../llvm_install/bin/clang++ $1.cpp &>> $1.ver
}


test test0
test test1
test test2


