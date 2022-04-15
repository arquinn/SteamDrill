#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/AliasSetTracker.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/CommandLine.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include "../helpers/LLVMHelper.h"

#include "../../../utils/tracer_configuration.hpp"
#include "../../log/log.hpp"

#include "../../../utils/replay_bin_info.hpp" // weird that I'm including steamdrill stuff, I know

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>

using namespace llvm;

using std::deque;
using std::string;
using std::unordered_set;

static cl::opt<std::string>
tpFile("tps", cl::desc("The tracepoints file"), cl::value_desc("filename"));

static cl::opt<bool>
debugFlag("d", cl::desc("log debug "), cl::value_desc("debug"));

static cl::opt<bool>
isolateFlag("iso", cl::desc("Whether to run isolation, or just cont."), cl::value_desc("isolate"));


struct Hello : public ModulePass {
  static char ID;
  Hello() : ModulePass(ID) {
    stores = 0;
    storesElided = 0;
    loads = 0;
    loadsElided = 0;
    debug = true;
    undoLogAdd = NULL;
    undoLogAddMemcpy = NULL;

    assignPtr = NULL;
  }

 bool debug;
#define DEBUG(x) if (debug) {do {errs() << x;} while(0);}

 private:
  Function *undoLogAdd;
  Function *undoLogAddMemcpy;
  Function *accessLogAdd;
  Function *accessLogAddMemcpy;

  Function *assignPtr;

  std::unordered_map<std::string, std::string> warningFunctions;
  std::unordered_set<Function *> ptraceFoos;
  std::unordered_set<Instruction *> instsExternalGlobal;

  // globals (for ease of use)
  DataLayout *DL;


  // statsistics
  int stores;
  int storesElided;
  int loads;
  int loadsElided;

  void printStats(void) {
    errs() << "stores: " << stores << "\n";
    errs() << "stores elided: " << storesElided << "\n";
    errs() << "loads: " << loads << "\n";
    errs() << "loads elided: " << loadsElided << "\n";
  }

 public:
  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AAResultsWrapperPass>();
  }

  virtual bool runOnModule(Module &M) {

    //errs() << "runOnModule\n";
    deque<Function*> worklist;
    unordered_set<Function*> seen;

    deque<Function*> supportCont;
    bool changed = false;
    //std::string tracepointsFile;
    initialize(M);

    unordered_set<Function*> tracerLib = LLVMHelper::getAnnotated<Function>("tracer_library",M);
    Function *dbg = M.getFunction("llvm.dbg.declare");
    seen.insert(dbg);

    for (auto item : tracerLib) {
      seen.insert(item);
      if (!item->isDeclaration()) {
        if (isolateFlag)
          worklist.push_back(item);
       }
    }

    // tracepointsFile = tpsFile;
    debug = debugFlag;
    if (isolateFlag) {
      errs() << "isolating this set of tracers\n";
    }

    auto tps = Configuration::parseTPoints(tpFile);
    // auto ips = Configuration::parseInstPoints(instFile);
    //errs() << "Pass on ";
    //errs().write_escaped(M.getName());
    //errs() << " with " << tpFile << "\n";


    for (auto tp : tps) {
      // TODO: anything we can do with selection for cont tracers?
      DEBUG("tp is " << tp->toString() << "\n");
      Function *f = M.getFunction(tp->getOutputFunction());
      if (!f) {
        errs() << "Can't find tp! " << tp->getOutputFunction() << "\n";
        return changed;
      }
      if (isolateFlag) {
        worklist.push_back(f);
        seen.insert(f);
      }

      if (auto &filterConfig = tp->getFilter()) {
        f = M.getFunction(filterConfig->getOutputFunction());
        if (!f) {
          errs() << "Can't find filter? " << filterConfig->getOutputFunction() << "\n";
          return changed;
        }
        if (isolateFlag) {
          worklist.push_back(f);
          seen.insert(f);
        }

        // cont tracers get no output help, but they might get filtering help:
        if (tp->isContinuous()) {
          DEBUG("continuous function " << f->getName() << "\n");
          supportCont.push_back(f);
        }
      }
    }

    while (!worklist.empty()) {
      Function *F = worklist.back();
      worklist.pop_back();
      DEBUG("checking on " << F->getName() << "\n");

      if (F->hasPersonalityFn()) {
        Constant *c = F->getPersonalityFn()->stripPointerCasts();
        Function *f = dyn_cast<Function>(c);
        if (!f)
          errs() << *c << " not a function\n";
        assert(f);

        seen.insert(f);
        if (!f->isDeclaration())
          worklist.push_front(f);
      }
      bool instrument = tracerLib.find(F) == tracerLib.end();
      AliasAnalysis &AA = getAnalysis<AAResultsWrapperPass>(*F).getAAResults();
      AliasSetTracker ast(AA);
      if (instrument) {
        for (auto bbit = F->begin(), bbend = F->end(); bbit != bbend; ++bbit)
          ast.add(*bbit);
      }

      for (Function::iterator it = F->begin(); it != F->end(); ++it) {
        for (auto inst = it->begin(); inst != it->end(); ++inst) {

          StoreInst *SI = dyn_cast<StoreInst>(&(*inst));
          // LoadInst *LI = dyn_cast<LoadInst>(&(*inst));

          CallInst *CI = dyn_cast<CallInst>(&(*inst));
          InvokeInst *II = dyn_cast<InvokeInst>(&(*inst));
          if (SI && instrument) {
            if(onlyStackAlias(SI, ast)) {
              storesElided++;
            }
            else {
              stores ++;
              addValToUndoLog(SI);
              changed = true;
            }
          }
          /*
            else if (WatchPoint && (LI != NULL)) {
            handleLoadInst(LI);
            changed = true;

           */
          else if (CI || II) {
            CallSite cs;
            if (CI) {
              cs = CallSite(CI);
            }
            else {
              cs = CallSite(II);
            }

            Function *called = getFunction(cs);
            if (called != NULL && seen.find(called) == seen.end()) {
              DEBUG("seen " << called->getName() << "\n");
              seen.insert(called);
              if (!called->isDeclaration())
                worklist.push_front(called);
            }
            if (CI)
              handleCallInst(CI, F->getName());
          }
        }
      }
    }

    if (isolateFlag) {
      unordered_set<Function*> remove;
      for (auto F = M.begin(), E = M.end(); F != E; ++F) {
        if (seen.find(&(*F)) == seen.end()) {
          remove.insert(&(*F));
        }
        else {
          seen.erase(&(*F));
        }
      }
      while (!remove.empty()) {
        auto f = remove.begin();
        (*f)->replaceAllUsesWith(UndefValue::get((*f)->getType()));
        (*f)->eraseFromParent();
        remove.erase(f);
      }
    }


    unordered_set<Function*> contSeen; // once a function is instrumented, we're good :D
    while (!supportCont.empty()) {
      Function *F = supportCont.back();
      supportCont.pop_back();

      // TODO: figure out whether we can ignore registers (SH just assumes :/)
      DEBUG("continuous for " << F->getName() << "\n");

      // magic code from above
      if (F->hasPersonalityFn()) {
        Constant *c = F->getPersonalityFn()->stripPointerCasts();
        Function *f = dyn_cast<Function>(c);
        if (!f)
          errs() << *c << " not a function\n";
        assert(f);

        contSeen.insert(f);
        if (!f->isDeclaration())
          supportCont.push_front(f);
      }

      // build up the alias tracker (not sure this works for globals ??
      bool instrument = tracerLib.find(F) == tracerLib.end();
      AliasAnalysis &AA = getAnalysis<AAResultsWrapperPass>(*F).getAAResults();
      AliasSetTracker ast(AA);
      if (instrument) {
        for (auto bbit = F->begin(), bbend = F->end(); bbit != bbend; ++bbit)
          ast.add(*bbit);
      }

      // find everything we give a fuck about
      for (Function::iterator it = F->begin(); it != F->end(); ++it) {
        for (auto inst = it->begin(); inst != it->end(); ++inst) {

          LoadInst *LI = dyn_cast<LoadInst>(&(*inst));
          CallInst *CI = dyn_cast<CallInst>(&(*inst));
          InvokeInst *II = dyn_cast<InvokeInst>(&(*inst));
          if (LI && instrument) {
            if(onlyStackAlias(LI, ast)) {
              loadsElided++;
            }
            else {
              loads++;
              addValToWatchLog(LI);
              changed = true;
            }
          }
          else if (CI || II) {
            // effectively don't allow memcpy YIKES (!)
            // TODO: We oughta copy the functions that we use (just in cases)

            CallSite cs;
            if (CI) {
              cs = CallSite(CI);
            }
            else {
              cs = CallSite(II);
            }

            Function *called = getFunction(cs);
            if (called != NULL && contSeen.find(called) == contSeen.end()) {
              DEBUG("seen " << called->getName() << "\n");
              contSeen.insert(called);
              if (!called->isDeclaration())
                supportCont.push_front(called);
            }
          }
        }
      }
    }

    // now lets deal with globals
    /*
    unordered_set<GlobalVariable*> vars =
        LLVMHelper::getAnnotated<GlobalVariable>("tracer_state", M);
    vars.insert(M.getGlobalVariable("llvm.global.annotations"));
        unordered_set<GlobalVariable*> unused_gvs;
    unordered_set<GlobalVariable*> convert_gvs;
    for (auto g = M.global_begin(), e = M.global_end(); g != e; ++g) {
      if (vars.find(&*g) == vars.end() && g->users().begin() == g->users().end()) {
        unused_gvs.insert(&*g);
      }

      else if (vars.find(&*g) == vars.end() && !g->isConstant()) {
        DEBUG("using " << *g << "\n");
        for (auto u : g->users()) {
          DEBUG("at " << *u << "\n");
        }
        convert_gvs.insert(&*g);
      }
    }
    for (auto ugv : unused_gvs) {
      ugv->eraseFromParent();
    }

    // TODO: Setup a manual linker for all of these vars
    ReplayBinInfo hank(replay);  // the rbi leader is hank arron

    // it's time to get FUNKY
    auto globalFoo = Function::Create(FunctionType::get(Type::getVoidTy(M.getContext()), false),
                                      GlobalValue::InternalLinkage, "manuelLinkage", M);
    DEBUG("new function: " << *globalFoo);
    auto endbb = BasicBlock::Create(M.getContext(), "globalFooBB", globalFoo);

    // I think I want this to always run last (idk though)
    appendToGlobalCtors(M, globalFoo, 0);
    DEBUG("created function, now let's build it\n");
    for (auto cgv : convert_gvs) {
      //if (cgv->getLinkage() != GlobalValue::ExternalLinkage) {

      // might need a trampoline thing to get the value from a different object?
      PointerType *ty = dyn_cast<PointerType>(cgv->getType());
      assert(ty && "globalVariable isn't a pointer!?");
      string name = cgv->getName();
      uintptr_t loc = hank.getGlobal(name);
      DEBUG("old " << *cgv << " rp loc " << (void*)loc << "\n");
      if (loc) {
        // create the update in the globalFoo:
        Constant *c = ConstantInt::get(Type::getInt32Ty(M.getContext()), loc);
        Value *gvCast = CastInst::CreatePointerCast(cgv, Type::getInt32PtrTy(M.getContext()),
                                                    "cgvCast", endbb);
        // DEBUG("should (hopefully) store " << *c << " to " << *gvCast <<"\n");

        std::vector<Value*> args = {c, gvCast};
        CallInst::Create (assignPtr, args, "",  endbb);

        //auto s = new StoreInst(c, gvCast, endbb);
        // DEBUG("store is built " << *s);
        // it is very much unclear if this is correct
          DEBUG("creates constnat (IF YOU SEE THIS, YOU NEED A TRAMP");
          auto newGV = new GlobalVariable(M,
          ty->getElementType(),
          false,
          cgv->getLinkage(),
          c,
          cgv->getName());

          cgv->replaceAllUsesWith(newGV);
          cgv->eraseFromParent();
          // set this after b/c llvm adds an int to differentiate syms.
          newGV->setName(name);
          DEBUG("new " << *newGV << "\n");
      }
      else {
        DEBUG("cannot get location of " << name << "\n");
      }
      //}
    }

    ReturnInst::Create(M.getContext(), endbb);
    */


    //printStats();
    return changed;
  }

  bool onlyStackAlias(const Instruction *I, AliasSetTracker &ast) {

    auto ml = MemoryLocation::get(I);
    auto &set = ast.getAliasSetFor(ml);
    for (auto s : set) {
      if (!dyn_cast<AllocaInst>(s.getValue()))
        return false;
    }
    return true;
  }

  Function* handleCallInst(CallInst *CI, std::string fooName) {
    CallSite cs(CI);
    Function *f =  getFunction(cs);
    if (f == NULL)
      return f;

    Module *M = CI->getParent()->getParent()->getParent();
    handleMemcpy(f, CI, M);
    return f;
  }

  Function* handleInvokeInst(InvokeInst *II, std::string fooName) {
    //is CI a callSite??
    CallSite cs(II);
    return getFunction(cs);
  }

  void handleMemcpy(Function *f, CallInst *CI, Module *M) {
    if (f->getName().startswith("llvm.memcpy")) {
      //TODO: handle watchpoint memcpy as well.
      assert(undoLogAddMemcpy);

      Value *dest = CI->getArgOperand(0);
      Value *size = CI->getArgOperand(2);

      dest = CastInst::CreatePointerCast(dest, Type::getInt32PtrTy(M->getContext()),
                                         "memcpyDest", CI);

      size = CastInst::CreateIntegerCast(size, Type::getInt32Ty(M->getContext()),
                                         false, "memcpySize", CI);

      std::vector<Value *> args = {dest, size};
      CallInst::Create (undoLogAddMemcpy, args, "",  CI);
      //auto ci
      //ci->setDebugLoc(DILocation::get(M->getContext(), 0, 1, ci->getFunction()->getSubprogram()));
    }
  }

  Function* getFunction(CallSite &cs) {
    Function *foo = LLVMHelper::getFcnFromCall(cs);
    if (foo == NULL) {
      //errs() << "ERROR! I'm super worried about this: ";
      //cs->print(errs());
      //errs() << "\n";
      return NULL;
    }
    if (foo->getName().startswith("llvm.dbg.declare")) {
      return NULL;
    }
    return foo;
  }

  void addValToWatchLog(LoadInst *LI) {
    Module *M = LI->getParent()->getParent()->getParent();
    Value *ea = LI->getPointerOperand();
    Type *valType = ea->getType()->getPointerElementType();

    if (DL->getTypeStoreSize(valType) > 4) {
      //we need to iterate through the bytes of this type.
      Value *eaIter = CastInst::CreatePointerCast(ea, Type::getInt32PtrTy(M->getContext()),
                                                  "eaIter", LI);
      int remaining = DL->getTypeStoreSize(valType);

      while (remaining > 0) {
        writeWord(eaIter, accessLogAdd, LI, M);
        eaIter = addWord(eaIter,  LI, M);
        remaining -= 4;
      }
    }
    else
      writeWord(ea, accessLogAdd, LI, M);
  }

  void addValToUndoLog(StoreInst *SI) {
    Module *M = SI->getParent()->getParent()->getParent();
    Value *ea = SI->getPointerOperand();
    Type *valType = ea->getType()->getPointerElementType();

    if (DL->getTypeStoreSize(valType) > 4) {
      //we need to iterate through the bytes of this type.
      Value *eaIter = CastInst::CreatePointerCast(ea, Type::getInt32PtrTy(M->getContext()),
                                                  "eaIter", SI);

      int remaining = DL->getTypeStoreSize(valType);
      while (remaining > 0) {
        writeWord(eaIter, undoLogAdd, SI, M);
        eaIter = addWord(eaIter,  SI, M);
        remaining -= 4;
      }
    }
    else
      writeWord(ea, undoLogAdd, SI, M);
  }

  Value* addWord(Value *ea, Instruction *before, Module *M) {
    Value* one = ConstantInt::get(Type::getInt32Ty(M->getContext()), 1);
    GetElementPtrInst *incEA = GetElementPtrInst::Create(Type::getInt32Ty(M->getContext()),
                                                         ea, one, "IncEA", before);
    return incEA;
  }

  void writeWord(Value *ea, Function *foo, Instruction *before, Module *M) {
    Value *eaCast = CastInst::CreatePointerCast(ea, Type::getInt32PtrTy(M->getContext()),
                                                "eaIter", before);

    std::vector<Value *> args = {eaCast};
    if (foo == accessLogAdd) {
      Value *l = ConstantInt::get(Type::getInt32Ty(M->getContext()), loads, false);
      args = {eaCast, l};
    }
    CallInst::Create (foo, args, "",  before);
    // auto ci
    //ci->setDebugLoc(DILocation::get(M->getContext(), 0, 1, ci->getFunction()->getSubprogram()));
  }

  void initialize(Module &M) {
    //initialize globals
    DL = new DataLayout(&M);
    // find fxns
    //undoLogAdd = M.getFunction("undologAdd");
    //undoLogAddMemcpy = M.getFunction("undologMemcpy");
    accessLogAdd = M.getFunction("accesslogAdd");
    accessLogAddMemcpy = M.getFunction("accesslogMemcpy");

    assignPtr = M.getFunction("tracerAssignPtr");

    /*if (undoLogAdd == NULL || undoLogAddMemcpy == NULL) {
      errs() << "I didn't find something \n";
      errs() << "undoLogAdd " << undoLogAdd << "\n";
      errs() << "undoLogAddMemcpy " << undoLogAddMemcpy << "\n";
      assert (0);
    }
    */
    if ((accessLogAdd == NULL || accessLogAddMemcpy == NULL)) {
      errs() << "I didn't find something \n";
      errs() << "accessLogAdd " << accessLogAdd << "\n";
      errs() << "accessAddMemcpy " << accessLogAddMemcpy << "\n";
      assert (0);
    }
  }
};


char Hello::ID = 0;
static RegisterPass<Hello> X("isolate", "LLVM Isolation Pass", false, false);

static void registerMyPass(const PassManagerBuilder &, legacy::PassManagerBase &PM) {
  PM.add(new Hello());
}

static RegisterStandardPasses RegisterMyPass(PassManagerBuilder::EP_EnabledOnOptLevel0,
                                             registerMyPass);


