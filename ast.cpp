#include "ast.hpp"
#include <cstdint>
#include <list>
#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/Support/Alignment.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <memory>
#include <set>
#include <vector>

std::unique_ptr<llvm::LLVMContext> context;
std::unique_ptr<llvm::IRBuilder<>> builder;
std::unique_ptr<llvm::Module> module;

llvm::Function *putchar_b;
llvm::Function *getchar_b;
llvm::AllocaInst *cursor;
llvm::AllocaInst *field;

static std::string extract_paren(std::string thing, int *p) {
    std::string result;
    int parens = 0;
    do {
        result += thing[*p];
        if (thing[*p] == '[') {
            parens++;
        } else if (thing[*p] == ']') {
            parens--;
        }
        (*p)++;
    } while (parens);
    (*p)--;
    return result;
};
BrainfProgram::BrainfProgram(std::string thing) {
    for (int i=0;i<thing.size();i++) {
        if (thing[i] == '[') {
            this->inner.push_back(new BrainfLoop(extract_paren(thing, &i)));
        } else {
            this->inner.push_back(new BrainfInstruction(thing[i]));
        }
    }
}
BrainfLoop::BrainfLoop(std::string thing) {
    auto actual = thing.substr(1, thing.size() - 2);
    this->inner = new BrainfProgram(actual);
}
BrainfInstruction::BrainfInstruction(char inst) {
    switch(inst) {
        case '+' :
            this->type = InstrType::ADD;
            break;
        case '-':
            this->type = InstrType::SUB;
            break;
        case '<':
            this->type = InstrType::LEFT;
            break;
        case '>':
            this->type = InstrType::RIGHT;
            break;
        case '.':
            this->type = InstrType::PRINT;
            break;
        case ',':
            this->type = InstrType::SCAN;
            break;
        default:
            this->type = InstrType::NOP;
    }
}

void codegen_optimizer_resume();

//CODEGEN STUFF
void init_codegen_stuff() {
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("brainf_llvm", *context);
    builder = std::make_unique<llvm::IRBuilder<>>(*context);

    getchar_b = llvm::Function::Create(
        llvm::FunctionType::get(
            llvm::Type::getInt32Ty(*context),
            std::vector<llvm::Type *>(),
            false
        ),
        llvm::GlobalValue::ExternalLinkage,
        "getchar",
        *module
    );
    putchar_b = llvm::Function::Create(
        llvm::FunctionType::get(
            llvm::Type::getInt32Ty(*context),
            std::vector<llvm::Type *>(
                {llvm::Type::getInt32Ty(*context)}
            ),
            false
        ),
        llvm::GlobalValue::ExternalLinkage,
        "putchar",
        *module
    );
    

    auto fun = llvm::Function::Create(
        llvm::FunctionType::get(
            llvm::Type::getVoidTy(*context),
            std::vector<llvm::Type *>(),
            false
        ),
        llvm::GlobalValue::ExternalLinkage,
        "main",
        *module
    );
    auto init_block = llvm::BasicBlock::Create(*context, "main_block", fun);
    builder->SetInsertPoint(init_block);
    auto field_type = llvm::ArrayType::get(builder->getInt8Ty(), 65536);
    field = builder->CreateAlloca(field_type, nullptr, "field");
    cursor = builder->CreateAlloca(llvm::Type::getInt16Ty(*context), nullptr, "cursor");

    //Idea to clear out allocas from https://github.com/eliben/code-for-blog/blob/master/2017/bfjit/llvmjit.cpp
    builder->CreateMemSet(field, builder->getInt8(0), builder->getInt32(65536), llvm::MaybeAlign());
    builder->CreateStore(builder->getInt16(0), cursor);

    codegen_optimizer_resume();
}
void fini_codegen_stuff() {
    builder->CreateRetVoid();
}
void BrainfProgram::codegen() const {
    for (auto thing : this->inner) {
        thing->codegen();
    }
}
void BrainfLoop::codegen() const {
    auto parent = builder->GetInsertBlock()->getParent();
    auto loop_body = llvm::BasicBlock::Create(*context, "loop_body", parent);
    auto loop_end = llvm::BasicBlock::Create(*context, "loop_end", parent);
    auto cursor_val_start = builder->CreateLoad(
        builder->CreatePointerCast(
            builder->CreateInBoundsGEP(
                field, 
                {builder->getInt16(0),builder->CreateLoad(cursor)}
            ),
            llvm::Type::getInt8PtrTy(*context)
        )
    );
    auto comparison = builder->CreateICmpEQ(cursor_val_start, builder->getInt8(0), "right_bracket");
    builder->CreateCondBr(comparison, loop_end, loop_body);

    builder->SetInsertPoint(loop_body);
    this->inner->codegen();
    auto cursor_val_end = builder->CreateLoad(
        builder->CreatePointerCast(
            builder->CreateInBoundsGEP(
                field, 
                {builder->getInt16(0),builder->CreateLoad(cursor)}
            ),
            llvm::Type::getInt8PtrTy(*context)
        )
    );
    auto comparison_end = builder->CreateICmpEQ(cursor_val_end, builder->getInt8(0), "left_bracket");
    builder->CreateCondBr(comparison_end, loop_end, loop_body);

    builder->SetInsertPoint(loop_end);
}
void BrainfInstruction::codegen() const {
    switch (this->type) {
        case InstrType::ADD:
            {
                auto cursor_loc = builder->CreatePointerCast(
                    builder->CreateInBoundsGEP(
                        field, 
                        {builder->getInt16(0),builder->CreateLoad(cursor)}
                    ),
                    llvm::Type::getInt8PtrTy(*context)
                );
                auto cursorval = builder->CreateLoad(cursor_loc);

                auto incr_val = builder->CreateAdd(cursorval, builder->getInt8(1), "inc");
                builder->CreateStore(incr_val, cursor_loc);
            }
            break;
        case InstrType::SUB:
            {
                auto cursor_loc = builder->CreatePointerCast(
                    builder->CreateInBoundsGEP(
                        field, 
                        {builder->getInt16(0),builder->CreateLoad(cursor)}
                    ),
                    llvm::Type::getInt8PtrTy(*context)
                );
                auto cursorval = builder->CreateLoad(cursor_loc);

                auto incr_val = builder->CreateSub(cursorval, builder->getInt8(1), "dec");
                builder->CreateStore(incr_val, cursor_loc);
            }
            break;
        case InstrType::LEFT:
            {
                auto cursorval = builder->CreateLoad(cursor);
                auto newcursorval = builder->CreateSub(cursorval, builder->getInt16(1), "left");
                builder->CreateStore(newcursorval, cursor);
            }
            break;
        case InstrType::RIGHT:
            {
                auto cursorval = builder->CreateLoad(cursor);
                auto newcursorval = builder->CreateAdd(cursorval, builder->getInt16(1), "right");
                builder->CreateStore(newcursorval, cursor);
            }
            break;
        case InstrType::PRINT:
            //call putchar()
            {
                auto cursor_loc = builder->CreatePointerCast(
                    builder->CreateInBoundsGEP(
                        field, 
                        {builder->getInt16(0),builder->CreateLoad(cursor)}
                    ),
                    llvm::Type::getInt8PtrTy(*context)
                );
                auto chrval = builder->CreateIntCast(builder->CreateLoad(cursor_loc), llvm::Type::getInt32Ty(*context), false);
                builder->CreateCall(putchar_b, std::vector<llvm::Value *>({chrval}));
            }
            break;
        case InstrType::SCAN:
            //call getchar()
            {
            auto character = builder->CreateIntCast(builder->CreateCall(getchar_b), llvm::Type::getInt8Ty(*context), false);
            auto cursor_loc = builder->CreatePointerCast(
                    builder->CreateInBoundsGEP(
                        field, 
                        {builder->getInt16(0),builder->CreateLoad(cursor)}
                    ),
                    llvm::Type::getInt8PtrTy(*context)
                );
            builder->CreateStore(character, cursor_loc);
            }
            break;
        case InstrType::NOP:
            //do nothing
            break;
        }
}

void BrainfProgram::debug_print() const {
    llvm::outs() << "[";
    for (auto element : this->inner) {
        element->debug_print();
        llvm::outs() << ",\n";
    }
    llvm::outs() << "]";
}
void BrainfLoop::debug_print() const {
    llvm::outs() << "{\"loop\":";
    this->inner->debug_print();
    llvm::outs() << "}";
}
void BrainfInstruction::debug_print() const {
    switch (this->type) {
    case InstrType::ADD:
        llvm::outs() << "\"ADD\"";
        break;
    case InstrType::SUB:
        llvm::outs() << "\"SUB\"";
        break;
    case InstrType::LEFT:
        llvm::outs() << "\"LEFT\"";
        break;
    case InstrType::RIGHT:
        llvm::outs() << "\"RIGHT\"";
        break;
    case InstrType::PRINT:
        llvm::outs() << "\"PRINT\"";
        break;
    case InstrType::SCAN:
        llvm::outs() << "\"SCAN\"";
        break;
    case InstrType::NOP:
        llvm::outs() << "\"NOP\"";
        break;
    }
}

bool BrainfProgram::contains_taint() const {
    for (auto entry : this->inner) {
        if (entry->contains_taint()) return true;
    }
    return false;
}
bool BrainfInstruction::contains_taint() const {
    return this->type == InstrType::SCAN;
}
bool BrainfLoop::contains_taint() const {
    return this->inner->contains_taint();
}

static int simulation[65536];
static int pointer = 0;
std::vector<char> optim_printed_result;
std::set<std::uint16_t> touched_addresses;
void BrainfInstruction::optimize() {
    //optimizer is running this instruction at compile time
    switch (this->type) {
        case InstrType::ADD:
            simulation[pointer]++;
            touched_addresses.insert(pointer);
            break;
        case InstrType::SUB:
            simulation[pointer]--;
            touched_addresses.insert(pointer);
            break;
        case InstrType::LEFT:
            pointer--;
            break;
        case InstrType::RIGHT:
            pointer++;
            break;
        case InstrType::PRINT:
            optim_printed_result.push_back(simulation[pointer]);
            break;
        case InstrType::SCAN:
            llvm::errs() << "Error while optimizing, hit tainted instruction while optimizing!";
        case InstrType::NOP:

          break;
        }
}
void BrainfLoop::optimize() {
    while (simulation[pointer]) {
        this->inner->optimize();
    }
}
void BrainfProgram::optimize() {
    //wont be called if tainted, no need to check
    for (auto thing : this->inner) {
        thing->optimize();
    }
}
void BrainfProgram::optimize_outer() {
    while (this->inner.size()) {
        if (this->inner[0]->contains_taint()) break;
        this->inner[0]->optimize();
        delete this->inner[0];
        this->inner.erase(this->inner.begin());
    }
}
BrainfItem::~BrainfItem() {
    //do nothing
}
BrainfInstruction::~BrainfInstruction() {
    //do nothing
}
BrainfProgram::~BrainfProgram() {
    for (auto thing : this->inner) {
        delete thing;
    }
}
BrainfLoop::~BrainfLoop() {
    delete this->inner;
}

void codegen_optimizer_resume() {
    //assumes optimize_outer has been run before codegen
    for (auto index : touched_addresses) {
        auto val = simulation[index];
        auto ptr = builder->CreatePointerCast(
                    builder->CreateInBoundsGEP(
                        field, 
                        {builder->getInt16(0),builder->getInt16(index)}
                    ),
                    llvm::Type::getInt8PtrTy(*context)
                );
        builder->CreateStore(builder->getInt8(val), ptr);
    }
    for (auto printed : optim_printed_result) {
        builder->CreateCall(putchar_b, {builder->getInt32(printed)});
    }
}