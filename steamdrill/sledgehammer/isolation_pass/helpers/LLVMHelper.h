/*
 * Borrowed from David Devecsery's Optimistic Hybrid Analysis Code.
 * Copyright (C) 2015 David Devecsery
 */

#ifndef INCLUDE_LLVMHELPER_H_
#define INCLUDE_LLVMHELPER_H_

#include <unordered_set>

//#include "include/Debug.h"
//#include "include/ModInfo.h"
//#include "include/lib/UnusedFunctions.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Casting.h"

using llvm::cast;
using llvm::ConstantArray;
using llvm::ConstantDataArray;
using llvm::ConstantStruct;
using llvm::dyn_cast;
using llvm::Function;
using llvm::GlobalVariable;
using llvm::Module;
using llvm::Value;

using std::vector;

class LLVMHelper {
public:
  // No constructor -- static only
  LLVMHelper() = delete;

  static const llvm::Function *getConstFcnFromCall(const llvm::CallInst *ci) {
    llvm::ImmutableCallSite cs(ci);
    return getConstFcnFromCall(cs);
  }

  static llvm::Function *getFcnFromCall(llvm::CallInst *ci) {
    llvm::CallSite cs(ci);
    return getFcnFromCall(cs);
  }


  static const llvm::Function *getConstFcnFromCall(llvm::ImmutableCallSite &cs) {
    const llvm::Function *fcn = cs.getCalledFunction();
    if (fcn == nullptr) {
      auto callee = cs.getCalledValue();

      if (!llvm::isa<llvm::InlineAsm>(callee)) {
        auto ce = llvm::dyn_cast<llvm::ConstantExpr>(callee);

        if (ce) {
          if (ce->getOpcode() == llvm::Instruction::BitCast) {
            fcn = llvm::dyn_cast<llvm::Function>(ce->getOperand(0));
          }
        }
      }
    }
    return fcn;
  }

  static llvm::Function *getFcnFromCall(llvm::CallSite &cs) {
    llvm::Function *fcn = cs.getCalledFunction();
    if (fcn == nullptr) {
      auto callee = cs.getCalledValue();

      if (!llvm::isa<llvm::InlineAsm>(callee)) {
        auto ce = llvm::dyn_cast<llvm::ConstantExpr>(callee);
        if (ce) {
          if (ce->getOpcode() == llvm::Instruction::BitCast) {
            fcn = llvm::dyn_cast<llvm::Function>(ce->getOperand(0));
          }
        }
      }
    }
    return fcn;
  }

  // HERE (!) this is incompatable with annotated variables (I guess?)
  template<class T>
      static std::unordered_set<T*> getAnnotated(const char *pattern,
                                                 const Module &M)
  {
    std::unordered_set<T*> foos;
    GlobalVariable *annotations =
        M.getGlobalVariable("llvm.global.annotations");
    if (!annotations)
      return foos;

    for (Value *oper : annotations->operands())
    {
      ConstantArray *array = dyn_cast<ConstantArray>(oper);
      if (!array)
        continue;

      for (Value *caOper : array->operands())
      {
        ConstantStruct *annStruct = dyn_cast<ConstantStruct>(caOper);
        if (!annStruct)
          continue;

        // finally we're at the structure w/ the function annotations...
        if (annStruct->getNumOperands() >= 2)
        {
          T *foo = dyn_cast<T>(annStruct->getOperand(0)->getOperand(0));
          GlobalVariable *gAnn = dyn_cast<GlobalVariable>(annStruct->getOperand(1)->getOperand(0));
          if (!gAnn || !foo)
            continue;

          ConstantDataArray *fooAnn = dyn_cast<ConstantDataArray>(gAnn->getOperand(0));
          if (!fooAnn)
            continue;

          std::string name = fooAnn->getAsString().str();

          //holly cow, finally
          if (!strcmp(name.c_str(), pattern))
          {
            foos.insert(foo);
          }
        }
      }
    }
    return foos;
  }
};

#endif  // INCLUDE_LLVMHELPER_H_
