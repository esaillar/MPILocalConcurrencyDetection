#include "LocalConcurrencyDetection.h"


using namespace std;
using namespace llvm;


LocalConcurrencyDetection::LocalConcurrencyDetection() : ModulePass(ID) {}

void LocalConcurrencyDetection::getAnalysisUsage(AnalysisUsage &au) const
{
	au.addRequired<DominatorTreeWrapperPass>();
	au.addRequired<DominanceFrontierWrapperPass>();
	au.addRequired<PostDominatorTreeWrapperPass>();
	au.addRequired<LoopInfoWrapperPass>();
	au.addRequired<AAResultsWrapperPass>();
	au.setPreservesAll();
}



auto MagentaErr = []() { return WithColor(errs(), raw_ostream::Colors::MAGENTA); };
auto GreenErr = []() { return WithColor(errs(), raw_ostream::Colors::GREEN); };
auto RedErr = []() { return WithColor(errs(), raw_ostream::Colors::RED); };

// Count MPI operations in Fortran
void LocalConcurrencyDetection::CountMPIfuncFORTRAN(Instruction &I){
	if( I.getOperand(0)->getName() == ("mpi_put_")){
		count_PUT++;
	}else if( I.getOperand(0)->getName() == ("mpi_get_")){
		count_GET++;
	}else if( I.getOperand(0)->getName() == ("mpi_accumulate_")){
		count_ACC++;
	}else if( I.getOperand(0)->getName() == ("mpi_win_create_")){
		count_Win++;
	}else if( I.getOperand(0)->getName() == ("mpi_win_free_")){
		count_Free++;
	}else if( I.getOperand(0)->getName() == ("mpi_win_fence_")){
		count_FENCE++;
	}else if( I.getOperand(0)->getName() == ("mpi_win_unlock_all_")){
		count_UNLOCKALL++;
	}else if( I.getOperand(0)->getName() == ("mpi_win_unlock_")){
		count_UNLOCK++;
	}else if( I.getOperand(0)->getName() == ("mpi_win_lock_")){
		count_LOCK++;
	}else if( I.getOperand(0)->getName() == ("mpi_win_lock_all_")){
		count_LOCKALL++;
	}else if( I.getOperand(0)->getName() == ("mpi_win_flush_")){
		count_FLUSH++;
	}else if( I.getOperand(0)->getName() == ("mpi_barrier_")){
		count_BARRIER++;
	}
}

void LocalConcurrencyDetection::CountMPIfuncC(Instruction &I, Function *calledFunction){

	if(calledFunction->getName() == "MPI_Get"){
		count_GET++;
	}else if(calledFunction->getName() == "MPI_Put"){
		count_PUT++;
	}else if(calledFunction->getName() == "MPI_Win_create"){
		count_Win++;
	}else if(calledFunction->getName() == "MPI_Accumulate"){
		count_ACC++;
	}else if (calledFunction->getName() == "MPI_Win_fence"){
		count_FENCE++;
	}else if (calledFunction->getName() == "MPI_Win_flush"){
		count_FLUSH++;
	}else if(calledFunction->getName() == "MPI_Win_lock"){
		count_LOCK++;
	}else if(calledFunction->getName() == "MPI_Win_unlock"){
		count_UNLOCK++;
	}else if(calledFunction->getName() == "MPI_Win_unlock_all"){
		count_UNLOCKALL++;
	}else if(calledFunction->getName() == "MPI_Win_lock_all"){
		count_LOCKALL++;
	}else if(calledFunction->getName() == "MPI_Win_free"){
		count_Free++;
	}else if(calledFunction->getName() == "MPI_Barrier"){
		count_BARRIER++;
	}
}


void LocalConcurrencyDetection::GetRMAstatistics(Function *F){

	for(Instruction &I : instructions(F)) {
		DebugLoc dbg = I.getDebugLoc(); // get debug infos
                int line = 0;
                StringRef file = "";
                if(dbg){
                        line = dbg.getLine();
                        file = dbg->getFilename();
                }
		if(I.getOpcode()==Instruction::BitCast){
			if(I.getOperand(0)->getName().startswith ("mpi_")){
				count_MPI++;
				CountMPIfuncFORTRAN(I);

			}
		}else if (CallBase *cb = dyn_cast<CallBase>(&I)) {
			
			if (Function *calledFunction = cb->getCalledFunction())
			{
				if(calledFunction->getName().startswith ("MPI_")){
					count_MPI++;
					CountMPIfuncC(I, calledFunction);
				}
			}
		}
	}
}


// Get the memory access from the instruction
// We consider only local accesses here
enum LocalConcurrencyDetection::ACCESS LocalConcurrencyDetection::getInstructionType(Instruction *I){

	// For FORTRAN codes:
	if(I->getOpcode()==Instruction::BitCast){
		if( I->getOperand(0)->getName() == ("mpi_get_")){
			return WRITE;
		} else if( I->getOperand(0)->getName() == ("mpi_put_")){
			return READ;
		}
		// For C codes:
	} else if (CallBase *ci = dyn_cast<CallBase>(I)){
		if (Function *calledFunction = ci->getCalledFunction()){
			if(calledFunction->getName() == "MPI_Get"){
				return WRITE;
			} else if(calledFunction->getName() == "MPI_Put"){
				return READ;
			} 
		}
	} else if( StoreInst *SI = dyn_cast<StoreInst>(I)){
		return WRITE;
	} else if(LoadInst *LI = dyn_cast<LoadInst>(I)){	
		return READ;
	}	
	// Default return
	return READ;
}



bool LocalConcurrencyDetection::hasBeenReported(Function *F, Instruction *I1, Instruction *I2){
	for (auto &Instpair:ConcurrentAccesses[F]) {
		if((Instpair.first == I1 && Instpair.second == I2) || (Instpair.first == I2 && Instpair.second == I1)) //anyof
			return true;	
	}  
	return false;
}


// Store the memory accesses - we keep the memory which is a value and the last instruction accessing this memory address
void LocalConcurrencyDetection::analyzeBB(BasicBlock *bb, AAResults &AA)
{
	Function *F = bb->getParent();

	for (auto i = bb->begin(), e = bb->end(); i != e; ++i) 
	{
		Instruction *inst = &*i;	
		DebugLoc dbg = inst->getDebugLoc(); // get debug infos
                int line = 0;
                StringRef file = "";
                if(dbg){
                        line = dbg.getLine();
                        file = dbg->getFilename();
                }
		Value *mem = NULL;
		bool isLoadOrStore = false;

		// (1) Get memory access
		if (CallBase *ci = dyn_cast<CallBase>(inst)){
			if (Function *calledFunction = ci->getCalledFunction()){
				if( (calledFunction->getName() == "MPI_Get") || (calledFunction->getName() == "MPI_Put") ){
					mem = ci->getArgOperand(0);
				} else if( (calledFunction->getName() == "MPI_Win_flush") || (calledFunction->getName() == "MPI_Win_flush_all") || (calledFunction->getName() == "MPI_Win_fence") || (calledFunction->getName() == "MPI_Win_flush_local")){
					BBToMemAccess[bb].clear(); 
				}
			}
		} else if(inst->getOpcode()==Instruction::BitCast){
			if(inst->getOperand(0)->getName() == ("mpi_put_") || inst->getOperand(0)->getName() == ("mpi_get_")){

				for (auto user = inst->user_begin();user != inst->user_end(); ++user){
					if (isa<CallBase>(cast<Instruction>(*user))){
						CallBase *c = dyn_cast<CallBase>(*user);
						mem = c->getArgOperand(0);
						errs() << inst->getOpcodeName() << " with operands: " << c->getArgOperand(0) << ", " << c->getArgOperand(1)  << "\n";
					}
				}
			} else{
				StringRef bitcastName = inst->getOperand(0)->getName();
				 if(bitcastName == ("mpi_win_flush_") ||  bitcastName  == ("mpi_win_flush_all_") || bitcastName  == "MPI_Win_flush_all" || bitcastName  == "MPI_Win_flush" || bitcastName  == "MPI_Win_fence" || bitcastName  == ("mpi_win_fence_") || bitcastName  == ("MPI_Win_flush_local")){
				BBToMemAccess[bb].clear();
				}
			}
		}else if( StoreInst *SI = dyn_cast<StoreInst>(inst)){
			mem = SI->getPointerOperand();
			isLoadOrStore = true;
		}else if(LoadInst *LI = dyn_cast<LoadInst>(inst)){ 
			mem =  LI->getPointerOperand();
			isLoadOrStore = true;
		}

		if(mem){
			auto PrevAccess = BBToMemAccess[bb].find(mem);

			// (2) mem is stored in ValueMap
			if(BBToMemAccess[bb].count(mem) != 0){
				if(getInstructionType(PrevAccess->second) == WRITE || getInstructionType(inst) == WRITE){
					// Check if we already report this error
					if(!hasBeenReported(F, inst, PrevAccess->second)){
						RedErr() << "LocalConcurrency detected: conflit with the following instructions: \n";
						inst->print(errs());
						if(dbg)
							GreenErr() << " - LINE " << line << " in " << file;
						errs()  << "\n";
						RedErr() << " AND ";
						PrevAccess->second->print(errs());
						DebugLoc Prevdbg = PrevAccess->second->getDebugLoc(); // get debug infos for previous access
						if(Prevdbg)
							GreenErr() << " - LINE " << Prevdbg.getLine() << " in " << Prevdbg->getFilename();
						errs() << "\n";
						GreenErr() << "=> BEWARE: debug information (line+filename) may be wrong on Fortran codes\n";
						ConcurrentAccesses[F].push_back({inst,PrevAccess->second});
					}
				}else{
					if(!isLoadOrStore)
						PrevAccess->second = inst;
				}
			// (2) mem is not stored in ValueMap - check if a memory stored in ValueMap alias with mem
			}else{
				ValueMap<Value *, Instruction *>::iterator it;
				for(it=BBToMemAccess[bb].begin(); it!=BBToMemAccess[bb].end(); it++){
					if(AA.alias(it->first,mem) != NoAlias){
						if(getInstructionType(it->second) == WRITE || getInstructionType(inst) == WRITE){		
							// Check if we already reported this error
							if(!hasBeenReported(F, inst, it->second)){
								RedErr() << "LocalConcurrency detected: conflit with the following instructions: \n";
								inst->print(errs()); 
								if(dbg)
									GreenErr() << " - LINE " << line << " in " << file;
								errs()  << "\n";
								RedErr() << " AND ";
								it->second->print(errs()); 
								DebugLoc Prevdbg = it->second->getDebugLoc(); // get debug infos for previous access
								if(Prevdbg)
									GreenErr() << " -  LINE " << Prevdbg.getLine() << " in " << Prevdbg->getFilename();
								errs() << "\n";
								GreenErr() << "=> BEWARE: debug information (line+filename) may be wrong on Fortran codes\n";
								ConcurrentAccesses[F].push_back({inst,it->second});
							}
						}
					}
				}
				// store mem if the instruction is a MPI-RMA
				if(!isLoadOrStore){ 
					BBToMemAccess[bb].insert({mem,inst});
				}
			}

		}
	}
}


void LocalConcurrencyDetection::printBB(BasicBlock *bb){
	errs() << "DEBUG: BB " ;
	bb->printAsOperand(errs(), false);
	errs() << "\n" ;
}



// If all predecessors have not been set to black, return true otherwise return false
bool LocalConcurrencyDetection::mustWait(BasicBlock *BB){
	if(LheaderMap[BB]){
		return false; // ignore loop headers
		
	}
	pred_iterator PI = pred_begin(BB), E = pred_end(BB);
	for (; PI != E; ++PI) {
		BasicBlock *Pred = *PI;
		if(ColorMap[Pred] != BLACK)
			return true;
	}
	return false;
}


bool LocalConcurrencyDetection::mustWaitLoop(llvm::BasicBlock *BB, Loop *L) {
	pred_iterator PI = pred_begin(BB), E = pred_end(BB);
	for (; PI != E; ++PI) {
		BasicBlock *Pred = *PI;
		// BB is in the only bb in loop
		if((Pred == BB) || (LlatchMap[Pred])){
			continue;
		}
    		if (ColorMap[Pred] != BLACK && L->contains(Pred)){
     			 return true;
		}
  }
  return false;
}

void LocalConcurrencyDetection::BFS_Loop(Loop *L, AAResults &AA){
	std::deque<BasicBlock *> Unvisited;
	BasicBlock *Lheader = L->getHeader();	
	BasicBlock *Llatch = L->getLoopLatch();	

	for(Loop *ChildLoop : *L){
		BFS_Loop(ChildLoop, AA);
	}
	Unvisited.push_back(Lheader);
	LheaderMap[L->getHeader()] = true; 

	while (Unvisited.size() > 0) {
		BasicBlock *header = *Unvisited.begin();
		Unvisited.pop_front();

		if (ColorMap[header] == BLACK)
			continue;
		if (mustWaitLoop(header, L) && header != L->getHeader()) { // all predecessors have not been seen
			Unvisited.push_back(header);
			continue;
		}

		analyzeBB(header, AA);
		ColorMap[header] = GREY;

		succ_iterator SI = succ_begin(header), E = succ_end(header); 
		for (; SI != E; ++SI) {
			BasicBlock *Succ = *SI;
			// Ignore successor not in loop
			if (!(L->contains(Succ)))
				continue;
			// ignore back edge when the loop has already been checked
			if(LlatchMap[header] && LheaderMap[Succ]){
				continue;
			}
			
			// Succ not seen before 
			if (ColorMap[Succ] == WHITE) {
				BBToMemAccess[Succ].insert(BBToMemAccess[header].begin(), BBToMemAccess[header].end());	
				ColorMap[Succ] = GREY;
				Unvisited.push_back(Succ);
			// Succ already seen 
			} else{
				// merge the memory accesses from the previous paths - only local errors detection 
				BBToMemAccess[Succ].insert(BBToMemAccess[header].begin(), BBToMemAccess[header].end());	
			}
		}
		ColorMap[header] = BLACK;
	}
	// reset BB colors in loop and ignore backedge for the rest of the BFS
	for (BasicBlock *BB : L->blocks()) {
		ColorMap[BB] = WHITE;
	}
	LlatchMap[Llatch]=true;
}

// Local concurrency errors detection (BFS)
void LocalConcurrencyDetection::FindLocalConcurrency(Function *F, AAResults &AA, LoopInfo &LI){

	// All BB must be white at the beginning
	for(BasicBlock &BB : *F){
		ColorMap[&BB] = WHITE;
		LheaderMap[&BB] = false;
		LlatchMap[&BB] = false;
	}

	std::deque<BasicBlock *> Unvisited;	
	BasicBlock &entry = F->getEntryBlock();
	Unvisited.push_back(&entry);


	// BFS on loops 
	for (Loop *L : LI) {
		BFS_Loop(L, AA);
	}

	// BFS
	while (Unvisited.size() > 0) {
		BasicBlock *header = *Unvisited.begin();
		Unvisited.pop_front();

		if(ColorMap[header] == BLACK)
			continue;

		if (mustWait(header)) { // all predecessors have not been seen
			Unvisited.push_back(header);
			continue;
		}

		analyzeBB(header, AA);
		ColorMap[header] = GREY;

		succ_iterator SI = succ_begin(header), E = succ_end(header); 
		for (; SI != E; ++SI) {
			BasicBlock *Succ = *SI;
			// ignore back edge 
			if(LlatchMap[header] && LheaderMap[Succ]) 
				continue;
			// Succ not seen before 
			if (ColorMap[Succ] == WHITE) {
				BBToMemAccess[Succ].insert(BBToMemAccess[header].begin(), BBToMemAccess[header].end());	
				ColorMap[Succ] = GREY;
				Unvisited.push_back(Succ);
				// Succ already seen 
			} else{
				// merge the memory accesses from the previous paths - only local errors detection 
				BBToMemAccess[Succ].insert(BBToMemAccess[header].begin(), BBToMemAccess[header].end());	
			}
		}
		ColorMap[header] = BLACK;
	}	
}

void LocalConcurrencyDetection::resetCounters(){
	count_GET = 0;
	count_PUT = 0;
	count_ACC = 0;
	count_Win = 0;
	count_Free = 0;
	count_FENCE = 0;
	count_FLUSH = 0;
	count_LOCK = 0;
	count_LOCKALL = 0;
	count_UNLOCK = 0;
	count_UNLOCKALL = 0;
	count_BARRIER = 0;
	count_MPI =0;
	count_LOAD =0;
	count_STORE =0;
	count_inst_LOAD =0;
	count_inst_STORE =0;
}


// Main function of the pass
bool LocalConcurrencyDetection::runOnModule(Module &M) 
{
	DataLayout* datalayout = new DataLayout(&M);
	auto CyanErr = []() { return WithColor(errs(), raw_ostream::Colors::CYAN); };

	MagentaErr() << "===============================================\n";
	MagentaErr() << "===  LOCAL CONCURRENCY DETECTION  ANALYSIS  ===\n";
	MagentaErr() << "===============================================\n";



	for(Function &F : M){
		LLVMContext &Ctx = F.getContext();
		if (F.isDeclaration()) 
			continue;				
		
		resetCounters();

		// Get Alias Analysis infos
		DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
		PostDominatorTree &PDT = getAnalysis<PostDominatorTreeWrapperPass>(F).getPostDomTree();
		AAResults &AA = getAnalysis<AAResultsWrapperPass>(F).getAAResults();
		MemorySSA MSSA(F, &AA, &DT);
		LoopInfo LI(DT);
		
		CyanErr() << "===========================\n";
		CyanErr() << "ANALYZING function " << F.getName() << "...\n";


		// Get statistics
		CyanErr() << "(1) Get statistics ...";
		GetRMAstatistics(&F);	
		int count_RMA = count_Win + count_PUT + count_GET + count_FENCE + count_ACC + count_LOCK + count_LOCKALL + count_UNLOCK + count_UNLOCKALL + count_Free + count_FLUSH; 
		int count_oneSided = count_PUT + count_GET + count_ACC; 
		CyanErr() << "done \n";

		// Detection of local concurrency errors - BFS
		CyanErr() << "(2) Local concurrency errors detection ..."; 
		if(count_oneSided != 0)
			FindLocalConcurrency(&F, AA, LI);
		CyanErr() << "done \n";


		// Print statistics per function
		CyanErr() << "=== STATISTICS === \n"; 
			CyanErr() << count_MPI << " MPI functions including " << count_RMA << " RMA functions \n";
			CyanErr() << "= WINDOW CREATION/DESTRUCTION: "  
			<< count_Free << " MPI_Win_free, "
			<< count_Win << " MPI_Win_create \n";
			CyanErr() << "= EPOCH CREATION/DESTRUCTION: "  
			<< count_FENCE << " MPI_Win_fence, "
			<< count_LOCK << " MPI_Lock, "
			<< count_LOCKALL << " MPI_Lockall "
			<< count_UNLOCK << " MPI_Unlock, "
			<< count_UNLOCKALL << " MPI_Unlockall \n";
			CyanErr() << "= ONE-SIDED COMMUNICATIONS: " 
			<< count_GET << " MPI_Get, " 
			<< count_PUT << " MPI_Put, "
			<< count_ACC << " MPI_Accumulate \n";
			CyanErr() << "= SYNCHRONIZATION: " 
			<< count_FLUSH << " MPI_Win_Flush \n";

		CyanErr() << "LOAD/STORE STATISTICS: " << count_inst_LOAD << " (/" << count_LOAD << ") LOAD and " << count_inst_STORE << " (/" << count_STORE << ") STORE are instrumented\n"; 
	}
	MagentaErr() << "===========================\n";
	return true;
}




char LocalConcurrencyDetection::ID = 0;
int LocalConcurrencyDetection::count_GET = 0;
int LocalConcurrencyDetection::count_PUT = 0;
int LocalConcurrencyDetection::count_ACC = 0;
int LocalConcurrencyDetection::count_Win = 0;
int LocalConcurrencyDetection::count_Free = 0;
int LocalConcurrencyDetection::count_FENCE = 0;
int LocalConcurrencyDetection::count_FLUSH = 0;
int LocalConcurrencyDetection::count_LOCK = 0;
int LocalConcurrencyDetection::count_LOCKALL = 0;
int LocalConcurrencyDetection::count_UNLOCK = 0;
int LocalConcurrencyDetection::count_UNLOCKALL = 0;
int LocalConcurrencyDetection::count_BARRIER = 0;
int LocalConcurrencyDetection::count_MPI =0;
int LocalConcurrencyDetection::count_LOAD =0;
int LocalConcurrencyDetection::count_STORE =0;
int LocalConcurrencyDetection::count_inst_LOAD =0;
int LocalConcurrencyDetection::count_inst_STORE =0;


static RegisterPass<LocalConcurrencyDetection> X("lcd", "MPI Local Concurrency Detection Pass", true, false);

/* FIXME: This way of registering the pass provokes segfaults with
 * LLVM-9.  Commented for now, if this can be fixed to allow reuse the
 * clang -Xclang -load -Xclang command, it would be great. */
//static RegisterStandardPasses Y(
//    PassManagerBuilder::EP_EarlyAsPossibe,
//    [](const PassManagerBuilder &Builder,
//       legacy::PassManagerBase &PM) { PM.add(new LocalConcurrencyDetection()); });


