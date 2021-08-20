#include <iostream>
#include "ast.hpp"

#include <llvm/IR/Verifier.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetLoweringObjectFile.h>
int main(int argc, char *kwargs[]) {
    /*
    if (argc < 3) {
        std::cout << "Rerun with 3 args!\nUsage: " << kwargs[0] << " (infile) (outfile)\n";
        return -1;
    }
    */

    //Hello World program

    auto program = std::string("++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++.");

    auto prog_ast = *(single_pass_parse(program)); //BrainfProgram(program);

    prog_ast.debug_print();

    prog_ast.optimize_outer();

    llvm::outs() << "OPTIMIZED DUMP:\n";
    prog_ast.debug_print();
    llvm::outs() << "Module:\n";

    auto triple_name_str = llvm::sys::getDefaultTargetTriple();
    auto arch_name = std::string();
    auto target_triple = llvm::Triple(triple_name_str);

    init_codegen_stuff();
    prog_ast.codegen();
    fini_codegen_stuff();

    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    //llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    std::string Error;
    auto target = llvm::TargetRegistry::lookupTarget(arch_name,target_triple, Error);
    if (!target) {
        llvm::errs() << Error;
        return -1;
    }

    module->setTargetTriple(triple_name_str);
    //Boilerplate ELF emitter code from https://www.llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl08.html
    auto CPU = "generic";
    auto Features = "";
    llvm::TargetOptions opt;
    auto RM = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::Model::PIC_);
    auto TheTargetMachine = target->createTargetMachine(triple_name_str, CPU, Features, opt, RM);
    module->setDataLayout(TheTargetMachine->createDataLayout());
    llvm::verifyModule(*module, &llvm::errs());
    //OPTIMIZATION PASSES
    //pre-opt print
    module->print(llvm::errs(), nullptr);
        llvm::PassBuilder pb;
    llvm::LoopAnalysisManager lam(true);
    llvm::FunctionAnalysisManager fam(true);
    llvm::CGSCCAnalysisManager cgsccam(true);
    llvm::ModuleAnalysisManager mam(true);

    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgsccam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);

    pb.crossRegisterProxies(lam, fam, cgsccam, mam);

    llvm::ModulePassManager module_pass_manager = pb.buildPerModuleDefaultPipeline(llvm::PassBuilder::OptimizationLevel::O2);

    module_pass_manager.run(*module, mam);
    //re-verify post optimization
    llvm::verifyModule(*module, &llvm::errs());
    //TODO: get output file name
    auto outfile = "output.o";
    std::error_code EC;
    llvm::raw_fd_ostream dest(outfile, EC, llvm::sys::fs::OF_None);
    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message();
        return 1;
    }

    llvm::legacy::PassManager pass;
    auto FileType = llvm::CGFT_ObjectFile;
    //check module
        module->print(llvm::errs(), nullptr);

    if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
        llvm::errs() << "TheTargetMachine can't emit a file of this type";
        return 1;
    }
    pass.run(*module);
    dest.flush();
    llvm::outs() << "Wrote output to " << outfile << "\n";
    return 0;
}