#!/bin/bash 

function test(){
  echo ../../../llvm_install/bin/clang++ -Xclang -ast-dump -fsyntax-only $1.cpp
  cat $1.cpp &> $1.ver_
  ../../../llvm_install/bin/clang++ -Xclang -ast-dump -fsyntax-only $1.cpp &>> $1.ver_
  sed 's/0x[0-9a-f]*//g' < $1.ver_ > $1.ver
  rm $1.ver_ -rf
}

test check