#ifndef LOCALCONCURRENCYDETECTION_H
#define LOCALCONCURRENCYDETECTION_H

#include "llvm/Pass.h"

#include "llvm/IRReader/IRReader.h"
#include "llvm/IRReader/IRReader.h"

#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Use.h"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PostOrderIterator.h"

#include "llvm/Support/WithColor.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/DominanceFrontier.h"

#include <string>
#include <vector>
#include <deque>


using namespace std;
using namespace llvm;


namespace {
	class LocalConcurrencyDetection : public ModulePass {


		public:
			static char ID;
			LocalConcurrencyDetection();
			virtual ~LocalConcurrencyDetection() {};
			virtual void getAnalysisUsage(AnalysisUsage &au) const;
			virtual bool runOnModule(Module &M);


		private:
			static int count_MPI, count_GET, count_PUT, count_ACC, count_FLUSH, count_FENCE, count_Win, count_Free, count_LOCK, count_UNLOCK, count_UNLOCKALL, count_LOCKALL, count_LOAD, count_STORE, count_BARRIER, count_inst_STORE, count_inst_LOAD;

			// Statistics
			void CountMPIfuncFORTRAN(Instruction &I);
			void CountMPIfuncC(Instruction &I, Function *calledFunction);
			void GetRMAstatistics(Function *F);
			// Local concurrency errors detection
			bool hasBeenReported(Function *F, Instruction *I1, Instruction *I2);
			void FindLocalConcurrency(Function *F, AAResults &AA, LoopInfo &LI);
			void analyzeBB(BasicBlock *B, AAResults &AA);
			void BFS_Loop(Loop *L, AAResults &AA);
			bool mustWait(BasicBlock *BB);
			bool mustWaitLoop(BasicBlock *BB, Loop *L);
			// Debug 
			void printBB(BasicBlock *BB);
			// Utils
			void resetCounters();
			enum Color { WHITE, GREY, BLACK };
			enum ACCESS { READ, WRITE };
			enum ACCESS	getInstructionType(Instruction *I);
			typedef DenseMap<const BasicBlock *, Color> BBColorMap;
			typedef DenseMap<const BasicBlock *, bool> BBMap;
			using MemMap = ValueMap<Value *, Instruction *>;
			typedef std::map<BasicBlock *, ValueMap<Value *, Instruction *> > BBMemMap;
			typedef std::map<Function *, SmallVector<std::pair<Instruction *, Instruction *>, 8> > FMemMap;
			BBColorMap ColorMap;
			BBMap LheaderMap;
			BBMap LlatchMap;
			BBMemMap BBToMemAccess;
			FMemMap	ConcurrentAccesses;
			MemMap ValueToMemAccess; 

	};

}




#endif

