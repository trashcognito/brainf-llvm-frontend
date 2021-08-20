#pragma once

#include <string>
#include <vector>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>

class BrainfItem {
    public:
    virtual void codegen() const = 0;
    virtual void debug_print() const = 0;
    virtual bool contains_taint() const = 0;
    virtual void optimize() = 0;
    virtual ~BrainfItem() = 0;
};
class BrainfProgram : public BrainfItem {
    public:
    std::vector<BrainfItem *> inner;
    BrainfProgram(std::string program);
    BrainfProgram();
    void codegen() const;
    void debug_print() const;
    bool contains_taint() const;
    void optimize();
    void optimize_outer();
    ~BrainfProgram();
};
class BrainfLoop : public BrainfItem {
    public:
    BrainfProgram *inner;
    void codegen() const;
    void debug_print() const;
    bool contains_taint() const;
    void optimize();
    ~BrainfLoop();
    BrainfLoop();
};
class BrainfInstruction : public BrainfItem {
    public:
    enum class InstrType {
        ADD,
        SUB,
        LEFT,
        RIGHT,
        PRINT,
        SCAN,
        NOP
    };
    InstrType type;
    BrainfInstruction(char inst);
    void codegen() const;
    void debug_print() const;
    bool contains_taint() const;
    void optimize();
    ~BrainfInstruction();
};
//Codegen stuff
extern std::unique_ptr<llvm::LLVMContext> context;
extern std::unique_ptr<llvm::IRBuilder<>> builder;
extern std::unique_ptr<llvm::Module> module;
void init_codegen_stuff();
void fini_codegen_stuff();