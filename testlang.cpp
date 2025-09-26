#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/NewGVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include <llvm/CodeGen/Passes.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Scalar.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

enum class TokenType {
  EOF_TOK = 0,
  ID = 1,
  STR = 2,
  ARR = 3,
  NUM = 4,
  BND = 5,
  NL = 6,
  EXP = 7,
  POP = 10,
  FLIP = 11,
  DUPE = 12,
  NOT = 20,
  BNT = 30,
  BAN = 31,
  BOR = 32,
  BXR = 33,
  ADD = 40,
  SUB = 41,
  MUL = 42,
  DIV = 43,
  MOD = 44,
  EQU = 45,
  RSH = 46,
  LSH = 47,
  RARR = 50,
  LARR = 51,
  RBRA = 52,
  LBRA = 53,
  RSQU = 54,
  LSQU = 55,
  RCUR = 56,
  LCUR = 57,
  DEF_START = 100,
  DEF_END = 101
};

struct Token {
  std::string value;
  TokenType type;
  std::vector<long> shape;
  std::vector<double> data;

  Token(const std::string &v, TokenType t) : value(v), type(t) {}
  Token(const std::string &v, TokenType t, const std::vector<long> &sh,
        const std::vector<double> &dt)
      : value(v), type(t), shape(sh), data(dt) {}
};

class StackLangCompiler {
private:
  std::vector<llvm::Value *> compileTimeStack;

  llvm::Value *stackPtr;
  llvm::Value *stackTop;
  llvm::LLVMContext context;
  std::unique_ptr<llvm::Module> module;
  std::unique_ptr<llvm::IRBuilder<>> builder;

  llvm::Type *doubleType;
  llvm::Type *int32Type;
  llvm::Type *int8PtrType;
  llvm::Type *int64Type;

  std::unordered_map<std::string, llvm::Function *> functions;

public:
  StackLangCompiler() : builder(std::make_unique<llvm::IRBuilder<>>(context)) {
    module = std::make_unique<llvm::Module>("StackLang", context);

    doubleType = llvm::Type::getDoubleTy(context);
    int32Type = llvm::Type::getInt32Ty(context);
    int8PtrType = llvm::Type::getInt8PtrTy(context);
    int64Type = llvm::Type::getInt64Ty(context);

    setupRuntimeFunctions();
  }

  void setupRuntimeFunctions() {
    auto voidType = llvm::Type::getVoidTy(context);

    auto pushMultidimType = llvm::FunctionType::get(
        voidType,
        {int32Type, int32Type->getPointerTo(), doubleType->getPointerTo()},
        false);
    llvm::Function::Create(pushMultidimType, llvm::Function::ExternalLinkage,
                           "push_multidim_array", module.get());

    auto pushArrayDataType = llvm::FunctionType::get(
        voidType, {int32Type, doubleType->getPointerTo()}, false);
    llvm::Function::Create(pushArrayDataType, llvm::Function::ExternalLinkage,
                           "push_array_data", module.get());

    auto matmulType = llvm::FunctionType::get(voidType, {}, false);
    llvm::Function::Create(matmulType, llvm::Function::ExternalLinkage,
                           "matrix_multiply", module.get());

    auto reshapeType = llvm::FunctionType::get(
        voidType, {int32Type, int32Type->getPointerTo()}, false);

    llvm::Function::Create(reshapeType, llvm::Function::ExternalLinkage,
                           "reshape_top", module.get());

    auto transposeType = llvm::FunctionType::get(voidType, {}, false);
    llvm::Function::Create(transposeType, llvm::Function::ExternalLinkage,
                           "transpose_top", module.get());

    auto pushDoubleType =
        llvm::FunctionType::get(voidType, {doubleType}, false);
    llvm::Function::Create(pushDoubleType, llvm::Function::ExternalLinkage,
                           "push_double", module.get());

    auto popDoubleType = llvm::FunctionType::get(doubleType, {}, false);
    llvm::Function::Create(popDoubleType, llvm::Function::ExternalLinkage,
                           "pop_double", module.get());

    auto simpleVoidType = llvm::FunctionType::get(voidType, {}, false);
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "push_shape", module.get());
    
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_band", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_bor", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_bxor", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_bnot", module.get());

    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_lshift", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_rshift", module.get());

    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_add", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_sub", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_mul", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_div", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_mod", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_pow", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_log", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_exp", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_ln", module.get());

    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_sin", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_cos", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_tan", module.get());

    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_asin", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_acos", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_atan", module.get());

    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "push_pi", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "push_e", module.get());

    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "do_neg", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "pop_and_print", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "duplicate_top", module.get());
    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "swap_top", module.get());

    llvm::Function::Create(simpleVoidType, llvm::Function::ExternalLinkage,
                           "expand_top", module.get());

    auto mallocType = llvm::FunctionType::get(int8PtrType, {int64Type}, false);
    llvm::Function::Create(mallocType, llvm::Function::ExternalLinkage,
                           "runtime_malloc", module.get());

    auto freeType = llvm::FunctionType::get(voidType, {int8PtrType}, false);
    llvm::Function::Create(freeType, llvm::Function::ExternalLinkage,
                           "runtime_free", module.get());
  }

  void compileToken(const Token &token) {
    switch (token.type) {
    case TokenType::NUM: {
      double val = std::stod(token.value);
      auto constant = llvm::ConstantFP::get(doubleType, val);
      auto pushFunc = module->getFunction("push_double");
      builder->CreateCall(pushFunc, {constant});
      break;
    }

    case TokenType::ARR: {
      if (token.shape.empty()) {
        std::cerr << "Warning: Array token has no shape information"
                  << std::endl;
        break;
      }

      bool is_multidim =
          token.shape.size() > 1 ||
          (token.shape.size() == 1 && token.shape[0] != token.data.size());

      auto mallocFunc = module->getFunction("runtime_malloc");
      auto dataSizeBytes =
          llvm::ConstantInt::get(int64Type, sizeof(double) * token.data.size());
      auto dataPtr = builder->CreateCall(mallocFunc, {dataSizeBytes});
      auto typedDataPtr =
          builder->CreateBitCast(dataPtr, doubleType->getPointerTo());

      for (size_t i = 0; i < token.data.size(); ++i) {
        auto idx = llvm::ConstantInt::get(int32Type, i);
        auto elemPtr = builder->CreateGEP(doubleType, typedDataPtr, idx);
        auto val = llvm::ConstantFP::get(doubleType, token.data[i]);
        builder->CreateStore(val, elemPtr);
      }

      if (is_multidim) {

        auto shapeSizeBytes = llvm::ConstantInt::get(
            int64Type, sizeof(int32_t) * token.shape.size());
        auto shapePtr = builder->CreateCall(mallocFunc, {shapeSizeBytes});
        auto typedShapePtr =
            builder->CreateBitCast(shapePtr, int32Type->getPointerTo());

        for (size_t i = 0; i < token.shape.size(); ++i) {
          auto idx = llvm::ConstantInt::get(int32Type, i);
          auto shapeElemPtr = builder->CreateGEP(int32Type, typedShapePtr, idx);
          auto shapeVal = llvm::ConstantInt::get(int32Type, token.shape[i]);
          builder->CreateStore(shapeVal, shapeElemPtr);
        }

        auto ndimConstant =
            llvm::ConstantInt::get(int32Type, token.shape.size());
        auto pushMultidimFunc = module->getFunction("push_multidim_array");
        builder->CreateCall(pushMultidimFunc,
                            {ndimConstant, typedShapePtr, typedDataPtr});

        auto freeFunc = module->getFunction("runtime_free");
        builder->CreateCall(freeFunc, {shapePtr});
      } else {
        auto sizeConstant =
            llvm::ConstantInt::get(int32Type, token.data.size());
        auto pushArrayFunc = module->getFunction("push_array_data");
        builder->CreateCall(pushArrayFunc, {sizeConstant, typedDataPtr});
      }
      break;
    }

    case TokenType::BAN: {
      auto Func = module->getFunction("do_band");
      builder->CreateCall(Func, {});
      break;
    }

    case TokenType::BOR: {
      auto Func = module->getFunction("do_bor");
      builder->CreateCall(Func, {});
      break;
    }

    case TokenType::BXR: {
      auto Func = module->getFunction("do_bxor");
      builder->CreateCall(Func, {});
      break;
    }

    case TokenType::BNT: {
      auto Func = module->getFunction("do_bnot");
      builder->CreateCall(Func, {});
      break;
    }

    case TokenType::LSH: {
      auto Func = module->getFunction("do_lshift");
      builder->CreateCall(Func, {});
      break;
    }

    case TokenType::RSH: {
      auto Func = module->getFunction("do_rshift");
      builder->CreateCall(Func, {});
      break;
    }

    case TokenType::MUL: {
      auto mulFunc = module->getFunction("do_mul");
      builder->CreateCall(mulFunc, {});
      break;
    }

    case TokenType::ADD: {
      auto addFunc = module->getFunction("do_add");
      builder->CreateCall(addFunc, {});
      break;
    }

    case TokenType::SUB: {
      auto subFunc = module->getFunction("do_sub");
      builder->CreateCall(subFunc, {});
      break;
    }

    case TokenType::DIV: {
      auto divFunc = module->getFunction("do_div");
      builder->CreateCall(divFunc, {});
      break;
    }

    case TokenType::MOD: {
      auto modFunc = module->getFunction("do_mod");
      builder->CreateCall(modFunc, {});
      break;
    }

    case TokenType::NOT: {
      auto negFunc = module->getFunction("do_neg");
      builder->CreateCall(negFunc, {});
      break;
    }

    case TokenType::DUPE: {
      auto dupeFunc = module->getFunction("duplicate_top");
      builder->CreateCall(dupeFunc, {});
      break;
    }

    case TokenType::FLIP: {
      auto flipFunc = module->getFunction("swap_top");
      builder->CreateCall(flipFunc, {});
      break;
    }

    case TokenType::POP: {
      auto printFunc = module->getFunction("pop_and_print");
      builder->CreateCall(printFunc, {});
      break;
    }

    case TokenType::EXP: {
      auto Func = module->getFunction("expand_top");
      builder->CreateCall(Func, {});
      break;
    }

    case TokenType::ID: {
      if (token.value == "matmul") {
        auto matmulFunc = module->getFunction("matrix_multiply");
        builder->CreateCall(matmulFunc, {});
      } else if (token.value == "shape") {
        auto shapeFunc = module->getFunction("push_shape");
        builder->CreateCall(shapeFunc, {});
      } else if (token.value == "pow") {
        auto powFunc = module->getFunction("do_pow");
        builder->CreateCall(powFunc, {});
      } else if (token.value == "log") {
        auto logFunc = module->getFunction("do_log");
        builder->CreateCall(logFunc, {});
      } else if (token.value == "exp") {
        auto expFunc = module->getFunction("do_exp");
        builder->CreateCall(expFunc, {});
      } else if (token.value == "ln") {
        auto lnFunc = module->getFunction("do_ln");
        builder->CreateCall(lnFunc, {});
      } else if (token.value == "sin") {
        auto trigFunc = module->getFunction("do_sin");
        builder->CreateCall(trigFunc, {});
      } else if (token.value == "cos") {
        auto trigFunc = module->getFunction("do_cos");
        builder->CreateCall(trigFunc, {});
      } else if (token.value == "tan") {
        auto trigFunc = module->getFunction("do_tan");
        builder->CreateCall(trigFunc, {});
      } else if (token.value == "asin") {
        auto trigFunc = module->getFunction("do_asin");
        builder->CreateCall(trigFunc, {});
      } else if (token.value == "acos") {
        auto trigFunc = module->getFunction("do_acos");
        builder->CreateCall(trigFunc, {});
      } else if (token.value == "atan") {
        auto trigFunc = module->getFunction("do_atan");
        builder->CreateCall(trigFunc, {});
      } else if (token.value == "pi") {
        auto transendentalFunc = module->getFunction("push_pi");
        builder->CreateCall(transendentalFunc, {});
      } else if (token.value == "e") {
        auto transendentalFunc = module->getFunction("push_e");
        builder->CreateCall(transendentalFunc, {});
      } else if (token.value == "transpose") {
        auto transposeFunc = module->getFunction("transpose_top");
        builder->CreateCall(transposeFunc, {});
      } else {
        auto func = functions.find(token.value);
        if (func != functions.end()) {
          builder->CreateCall(func->second, {});
        } else {
          std::cerr << "Warning: Undefined function: " << token.value
                    << std::endl;
        }
      }
      break;
    }

    case TokenType::NL:
    case TokenType::EOF_TOK:
      break;

    default:
      std::cerr << "Warning: Unhandled token type: "
                << static_cast<int>(token.type) << std::endl;
      break;
    }
  }

  void addReshapeOperation(const std::vector<long> &newShape) {
    auto mallocFunc = module->getFunction("runtime_malloc");
    auto shapeSizeBytes =
        llvm::ConstantInt::get(int64Type, sizeof(int32_t) * newShape.size());
    auto shapePtr = builder->CreateCall(mallocFunc, {shapeSizeBytes});
    auto typedShapePtr =
        builder->CreateBitCast(shapePtr, int32Type->getPointerTo());

    for (size_t i = 0; i < newShape.size(); ++i) {
      auto idx = llvm::ConstantInt::get(int32Type, i);
      auto shapeElemPtr = builder->CreateGEP(int32Type, typedShapePtr, idx);
      auto shapeVal = llvm::ConstantInt::get(int32Type, newShape[i]);
      builder->CreateStore(shapeVal, shapeElemPtr);
    }

    auto ndimConstant = llvm::ConstantInt::get(int32Type, newShape.size());
    auto reshapeFunc = module->getFunction("reshape_top");
    builder->CreateCall(reshapeFunc, {ndimConstant, typedShapePtr});

    auto freeFunc = module->getFunction("runtime_free");
    builder->CreateCall(freeFunc, {shapePtr});
  }

  llvm::Function *createMainFunction() {
    auto mainType = llvm::FunctionType::get(int32Type, {}, false);
    auto mainFunc = llvm::Function::Create(
        mainType, llvm::Function::ExternalLinkage, "main", module.get());

    auto entry = llvm::BasicBlock::Create(context, "entry", mainFunc);
    builder->SetInsertPoint(entry);

    return mainFunc;
  }

  void compile(const std::vector<Token> &tokens) {
    auto mainFunc = createMainFunction();

    bool inDef = false;
    std::string currentDefName;
    std::vector<Token> currentDefBody;
    std::vector<Token> proceduralTokens;

    for (const auto &token : tokens) {
      if (token.type == TokenType::DEF_START) {
        inDef = true;
        continue;
      } else if (token.type == TokenType::DEF_END) {
        inDef = false;
        continue;
      }

      if (inDef) {
        if (token.type == TokenType::ID && currentDefName.empty()) {
          currentDefName = token.value;
        } else if (token.type == TokenType::BND) {
          currentDefBody.clear();
        } else if (token.type == TokenType::NL && !currentDefName.empty()) {
          createFunction(currentDefName, currentDefBody);
          currentDefName.clear();
          currentDefBody.clear();
        } else if (!currentDefName.empty() && token.type != TokenType::BND) {
          currentDefBody.push_back(token);
        }
      } else {
        proceduralTokens.push_back(token);
      }
    }

    std::vector<std::vector<Token>> proceduralLines;
    if (!proceduralTokens.empty()) {
      proceduralLines.emplace_back();
      for (const auto &token : proceduralTokens) {
        if (token.type == TokenType::NL || token.type == TokenType::EOF_TOK) {
          if (!proceduralLines.back().empty()) {
            proceduralLines.emplace_back();
          }
        } else {
          proceduralLines.back().push_back(token);
        }
      }
    }

    for (auto &line : proceduralLines) {
      if (line.empty())
        continue;

      std::reverse(line.begin(), line.end());
      for (const auto &token : line) {
        compileToken(token);
      }
    }

    builder->CreateRet(llvm::ConstantInt::get(int32Type, 0));
  }

  void createFunction(const std::string &name, const std::vector<Token> &body) {
    auto voidType = llvm::Type::getVoidTy(context);
    auto funcType = llvm::FunctionType::get(voidType, {}, false);
    auto func = llvm::Function::Create(
        funcType, llvm::Function::InternalLinkage, name, module.get());

    auto entry = llvm::BasicBlock::Create(context, "entry", func);
    auto oldInsertPoint = builder->GetInsertBlock();
    builder->SetInsertPoint(entry);

    auto reversed_body = body;
    std::reverse(reversed_body.begin(), reversed_body.end());

    for (const auto &token : reversed_body) {
      compileToken(token);
    }

    builder->CreateRetVoid();
    builder->SetInsertPoint(oldInsertPoint);

    functions[name] = func;
  }

  void generateLLVMIR(const std::string &filename) {
    std::error_code EC;
    llvm::raw_fd_ostream file(filename, EC);
    if (EC) {
      std::cerr << "Error opening file: " << EC.message() << std::endl;
      return;
    }
    module->print(file, nullptr);
  }

  void generateObjectFile(const std::string &filename) {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    auto targetTriple = llvm::sys::getDefaultTargetTriple();
    module->setTargetTriple(targetTriple);

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
    if (!target) {
      std::cerr << "Error: " << error << std::endl;
      return;
    }

    auto CPU = "generic";
    auto features = "";
    llvm::TargetOptions opt;
    auto relocModel = llvm::Reloc::PIC_;
    auto targetMachine = target->createTargetMachine(targetTriple, CPU,
                                                     features, opt, relocModel);

    module->setDataLayout(targetMachine->createDataLayout());

    std::error_code EC;
    llvm::raw_fd_ostream dest(filename, EC, llvm::sys::fs::OF_None);
    if (EC) {
      std::cerr << "Could not open file: " << EC.message() << std::endl;
      return;
    }

    llvm::legacy::PassManager pass;
    auto fileType = llvm::CodeGenFileType::CGFT_ObjectFile;

    if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
      std::cerr << "TargetMachine can't emit a file of this type" << std::endl;
      return;
    }

    pass.run(*module);
    dest.flush();
  }

  void generateExecutable(const std::string &filename,
                          const std::string &runtimeLibPath = "") {
    std::string objFile = filename + ".o";
    generateObjectFile(objFile);

    std::string linkCmd = "clang++ -o " + filename + " " + objFile +
                          " -Wl,--whole-archive " + runtimeLibPath +
                          " -Wl,--no-whole-archive";
    if (!runtimeLibPath.empty()) {
      linkCmd += " " + runtimeLibPath;
    }

    int result = std::system(linkCmd.c_str());
    if (result != 0) {
      std::cerr << "Linking failed!" << std::endl;
    } else {
      std::cout << "Executable generated: " << filename << std::endl;
      std::remove(objFile.c_str());
    }
  }

  void optimize() {
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    llvm::PassBuilder PB;

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    llvm::FunctionPassManager FPM;

    FPM.addPass(llvm::InstCombinePass());
    FPM.addPass(llvm::ReassociatePass());
    FPM.addPass(llvm::NewGVNPass());
    FPM.addPass(llvm::SimplifyCFGPass());

    for (auto &func : *module) {
      if (!func.isDeclaration()) {
        FPM.run(func, FAM);
      }
    }
  }

  bool verify() { return !llvm::verifyModule(*module, &llvm::errs()); }
};

std::string trim_and_clean(const std::string &str) {
  const std::string whitespace = " \t\n\r\f\v";
  size_t first = str.find_first_not_of(whitespace);
  if (std::string::npos == first) {
    return str;
  }
  size_t last = str.find_last_not_of(whitespace);
  std::string trimmed = str.substr(first, (last - first + 1));

  if ((trimmed.front() == '\'' && trimmed.back() == '\'') ||
      (trimmed.front() == '"' && trimmed.back() == '"')) {
    return trimmed.substr(1, trimmed.length() - 2);
  }
  return trimmed;
}

std::vector<Token> parseTokenFile(const std::string &filename) {
  std::vector<Token> tokens;
  std::ifstream file(filename);
  std::string line;

  std::regex tokenRegex(
      R"(Token\(value='([^']*)', type=<TokenType\.(\w+): \d+>\))");
  std::regex arrayRegex(
      R"(Token\(value=\(\[([^\]]*)\], \[([^\]]*)\]\), type=<TokenType\.ARR: 3>\))");

  while (std::getline(file, line)) {
    std::smatch match;

    if (std::regex_match(line, match, arrayRegex)) {
      std::string shapeStr = match[1].str();
      std::string dataStr = match[2].str();

      std::vector<long> shape;
      std::vector<double> data;

      std::stringstream ss(shapeStr);
      std::string item;
      while (std::getline(ss, item, ',')) {
        if (!item.empty()) {
          std::string trimmed = trim_and_clean(item);
          if (!trimmed.empty()) {
            shape.push_back(std::stoi(trimmed));
          }
        }
      }

      ss = std::stringstream(dataStr);
      while (std::getline(ss, item, ',')) {
        if (!item.empty()) {
          std::string trimmed = trim_and_clean(item);
          if (!trimmed.empty()) {
            data.push_back(std::stod(trimmed));
          }
        }
      }

      tokens.emplace_back("array", TokenType::ARR, shape, data);
    } else if (std::regex_match(line, match, tokenRegex)) {
      std::string value = match[1].str();
      std::string typeStr = match[2].str();

      TokenType type = TokenType::EOF_TOK;
      if (typeStr == "NUM")
        type = TokenType::NUM;
      else if (typeStr == "ID")
        type = TokenType::ID;
      else if (typeStr == "ADD")
        type = TokenType::ADD;
      else if (typeStr == "SUB")
        type = TokenType::SUB;
      else if (typeStr == "MUL")
        type = TokenType::MUL;
      else if (typeStr == "DIV")
        type = TokenType::DIV;
      else if (typeStr == "POP")
        type = TokenType::POP;
      else if (typeStr == "EXP")
        type = TokenType::EXP;
      else if (typeStr == "DUPE")
        type = TokenType::DUPE;
      else if (typeStr == "FLIP")
        type = TokenType::FLIP;
      else if (typeStr == "BND")
        type = TokenType::BND;
      else if (typeStr == "NL")
        type = TokenType::NL;
      else if (typeStr == "DEF_START")
        type = TokenType::DEF_START;
      else if (typeStr == "DEF_END")
        type = TokenType::DEF_END;
      else if (typeStr == "EOF")
        type = TokenType::EOF_TOK;

      tokens.emplace_back(value, type);
    }
  }

  return tokens;
}

int main(int argc, char *argv[]) {
  if (argc < 3 || argc > 5) {
    std::cerr << "Usage: " << argv[0]
              << " <source_file> <output> [--ir|--obj|--exe] [lexer_path]"
              << std::endl;
    std::cerr << "  source_file: Stack language source file" << std::endl;
    std::cerr << "  output:      Output file name" << std::endl;
    std::cerr << "  --ir:        Generate LLVM IR (.ll file)" << std::endl;
    std::cerr << "  --obj:       Generate object file (.o)" << std::endl;
    std::cerr << "  --exe:       Generate executable (default)" << std::endl;
    std::cerr << "  lexer_path:  Path to Python lexer (default: ./lexer.py)"
              << std::endl;
    return 1;
  }

  std::string sourceFile = argv[1];
  std::string outputFile = argv[2];
  std::string outputMode = (argc >= 4) ? argv[3] : "--exe";
  std::string lexerPath = (argc >= 5) ? argv[4] : "./lexer.py";

  std::ifstream sourceCheck(sourceFile);
  if (!sourceCheck.good()) {
    std::cerr << "Error: Source file '" << sourceFile << "' not found!"
              << std::endl;
    return 1;
  }
  sourceCheck.close();

  std::ifstream lexerCheck(lexerPath);
  if (!lexerCheck.good()) {
    std::cerr << "Error: Lexer file '" << lexerPath << "' not found!"
              << std::endl;
    std::cerr << "Make sure the Python lexer is in the current directory or "
                 "specify its path."
              << std::endl;
    return 1;
  }
  lexerCheck.close();

  std::cout << "Compiling Stack Language source: " << sourceFile << std::endl;

  std::string tmpTokenFile = "tmp_tokens.txt";
  std::string lexerCommand = "python3 \"" + lexerPath + "\" \"" + sourceFile +
                             "\" \"" + tmpTokenFile + "\"";

  std::cout << "Running lexer: " << lexerCommand << std::endl;
  int lexerResult = std::system(lexerCommand.c_str());

  if (lexerResult != 0) {
    std::cerr << "Error: Lexer failed with exit code " << lexerResult
              << std::endl;
    std::cerr
        << "Make sure Python 3 is installed and the lexer script is correct."
        << std::endl;
    return 1;
  }

  std::ifstream tokenCheck(tmpTokenFile);
  if (!tokenCheck.good()) {
    std::cerr << "Error: Lexer did not generate token file!" << std::endl;
    return 1;
  }
  tokenCheck.close();

  std::cout << "Lexing complete. Reading tokens..." << std::endl;

  auto tokens = parseTokenFile(tmpTokenFile);

  if (tokens.empty()) {
    std::cerr << "Error: No tokens parsed from " << tmpTokenFile << std::endl;
    return 1;
  }

  std::cout << "Parsed " << tokens.size() << " tokens. Compiling..."
            << std::endl;

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  StackLangCompiler compiler;
  compiler.compile(tokens);

  if (!compiler.verify()) {
    std::cerr << "Module verification failed!" << std::endl;
    std::remove(tmpTokenFile.c_str());
    return 1;
  }

  std::cout << "Module verified. Optimizing..." << std::endl;
  compiler.optimize();

  if (outputMode == "--ir") {
    compiler.generateLLVMIR(outputFile + ".ll");
    std::cout << "LLVM IR generated: " << outputFile << ".ll" << std::endl;
  } else if (outputMode == "--obj") {
    compiler.generateObjectFile(outputFile + ".o");
    std::cout << "Object file generated: " << outputFile << ".o" << std::endl;
  } else if (outputMode == "--exe") {
    compiler.generateExecutable(outputFile, "./libstack_runtime.a");
    std::cout << "Executable generated: " << outputFile << std::endl;
  } else {
    std::cerr << "Unknown output mode: " << outputMode << std::endl;
    std::remove(tmpTokenFile.c_str());
    return 1;
  }

  std::remove(tmpTokenFile.c_str());

  std::cout << "Compilation complete!" << std::endl;
  if (outputMode == "--exe") {
    std::cout << "Run with: ./" << outputFile << std::endl;
  }

  return 0;
}