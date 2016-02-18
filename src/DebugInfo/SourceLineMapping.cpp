//
// This class provides LLVM passes that provide a mapping from LLVM
// instruction to Binary  information.
//

//#define DEBUG_TYPE "giriutil"

#include "SourceLineMapping.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/DebugInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_os_ostream.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

using namespace llvm;

//===----------------------------------------------------------------------===//
//                          Command line options
//===----------------------------------------------------------------------===//
static cl::opt<std::string>
FunctionName("mapping-function",
             cl::desc("The function name to be mapped"),
             cl::init(""));

static cl::opt<std::string>
MappingFileName("mapping-output",
                cl::desc("The output filename of the source line mapping"),
                cl::init("-"));

//===----------------------------------------------------------------------===//
//                          Pass Statistics
//===----------------------------------------------------------------------===//
STATISTIC(NumFoundSrc, "Number of source information locations found");
STATISTIC(NumNotFoundSrc, "Number of source information locations not found");
STATISTIC(NumQueriedSrc, "Number of queried including ignored LLVM insts");

//===----------------------------------------------------------------------===//
//                      Source Line Mapping Pass
//===----------------------------------------------------------------------===//

// ID Variable to identify the pass
char SourceLineMappingPass::ID = 0;

// Pass registration
static RegisterPass<SourceLineMappingPass>
X("srcline-mapping", "Mapping LLVM inst to source line number");

std::string SourceLineMappingPass::locateSrcInfo(Instruction *I) {
  // Update the number of source locations queried.
  ++NumQueriedSrc;

  // Get the ID number for debug metadata.
  if (MDNode *N = I->getMetadata("mcsema_real_eip")) {
    ++NumFoundSrc;

    Value *val = N->getOperand(0);
    assert(val != NULL && "mcsema_real_eip metadata not populated"); 

    std::stringstream ss;
    if (ConstantInt *ci = dyn_cast<ConstantInt>(val)) {
        uint64_t val = ci->getLimitedValue();
        ss  << std::hex << val ;
    }
    return ss.str();
  } else {
    NumNotFoundSrc++;
    return "NIL";
  }
}

void SourceLineMappingPass::mapCompleteFile(Module &M, raw_ostream &Output) {
  for (Module::iterator F = M.begin(); F != M.end(); ++F)
    if (!F->isDeclaration() && F->hasName())
      mapOneFunction(&*F, Output);
}

void SourceLineMappingPass::mapOneFunction(Function *F, raw_ostream &Output) {
  assert(F && !F->isDeclaration() && F->hasName());
  Output << "========================================================\n";
  Output << "Source line mapping for function: " << F->getName() << "\n";
  Output << "========================================================\n";

  int instCount = 0;
  for (inst_iterator I = inst_begin(F); I != inst_end(F); ++I) {
    Output << ++instCount << " :  ";
    I->print(Output);
    Output << ": ";
    Output << locateSrcInfo(&*I);
    Output << "\n";
  }
}

bool SourceLineMappingPass::runOnModule(Module &M) {
  std::string errinfo;
  raw_fd_ostream MappingFile(MappingFileName.c_str(), errinfo);

  if (!FunctionName.empty())
    mapOneFunction(M.getFunction(FunctionName), MappingFile);
  else 
    mapCompleteFile(M, MappingFile);

  // This is an analysis pass, so always return false.
  return false;
}
