#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/CodeGen/Passes.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stack>
#include <unordered_map>
#include <string>
#include <memory>
#include <regex>
#include <cstdlib>

enum class TokenType {
    EOF_TOK = 0, ID = 1, STR = 2, ARR = 3, NUM = 4, BND = 5, NL = 6,
    POP = 10, FLIP = 11, DUPE = 12,
    NOT = 20,
    BNT = 30, BAN = 31, BOR = 32, BXR = 33,
    ADD = 40, SUB = 41, MUL = 42, DIV = 43, MOD = 44, EQU = 45, RSH = 46, LSH = 47,
    RARR = 50, LARR = 51, RBRA = 52, LBRA = 53, RSQU = 54, LSQU = 55, RCUR = 56, LCUR = 57,
    DEF_START = 100, DEF_END = 101
};

struct Token {
    std::string value;
    TokenType type;
    std::vector<int> shape;  // For arrays
    std::vector<double> data; // For array data
    
    Token(const std::string& v, TokenType t) : value(v), type(t) {}
    Token(const std::string& v, TokenType t, const std::vector<int>& sh, const std::vector<double>& dt) 
        : value(v), type(t), shape(sh), data(dt) {}
};

class StackLangCompiler {
private:
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    
    // Stack simulation for compile-time
    std::vector<llvm::Value*> compileTimeStack;
    
    // Runtime stack pointer and operations
    llvm::Value* stackPtr;
    llvm::Value* stackTop;
    
    // Function definitions
    std::unordered_map<std::string, llvm::Function*> functions;
    
    // Types
    llvm::Type* doubleType;
    llvm::Type* int32Type;
    llvm::Type* int8PtrType;
    llvm::StructType* arrayType;  // {i32 size, double* data}
    
public:
    StackLangCompiler() : builder(std::make_unique<llvm::IRBuilder<>>(context)) {
        module = std::make_unique<llvm::Module>("StackLang", context);
        
        // Initialize types
        doubleType = llvm::Type::getDoubleTy(context);
        int32Type = llvm::Type::getInt32Ty(context);
        int8PtrType = llvm::Type::getInt8PtrTy(context);
        
        // Array type: {i32 size, double* data}
        std::vector<llvm::Type*> arrayFields = {int32Type, doubleType->getPointerTo()};
        arrayType = llvm::StructType::create(context, arrayFields, "Array");
        
        setupRuntimeFunctions();
    }
    
    void setupRuntimeFunctions() {
        // Stack operations
        auto voidType = llvm::Type::getVoidTy(context);
        
        // void push_double(double val)
        auto pushDoubleType = llvm::FunctionType::get(voidType, {doubleType}, false);
        llvm::Function::Create(pushDoubleType, llvm::Function::ExternalLinkage, "push_double", module.get());
        
        // double pop_double()
        auto popDoubleType = llvm::FunctionType::get(doubleType, {}, false);
        llvm::Function::Create(popDoubleType, llvm::Function::ExternalLinkage, "pop_double", module.get());
        
        // void push_array(Array arr)
        auto pushArrayType = llvm::FunctionType::get(voidType, {arrayType}, false);
        llvm::Function::Create(pushArrayType, llvm::Function::ExternalLinkage, "push_array", module.get());
        
        // Array pop_array()
        auto popArrayType = llvm::FunctionType::get(arrayType, {}, false);
        llvm::Function::Create(popArrayType, llvm::Function::ExternalLinkage, "pop_array", module.get());
        
        // void print_double(double val)
        auto printDoubleType = llvm::FunctionType::get(voidType, {doubleType}, false);
        llvm::Function::Create(printDoubleType, llvm::Function::ExternalLinkage, "print_double", module.get());
        
        // void print_array(Array arr)
        auto printArrayType = llvm::FunctionType::get(voidType, {arrayType}, false);
        llvm::Function::Create(printArrayType, llvm::Function::ExternalLinkage, "print_array", module.get());
    }
    
    llvm::Function* createMainFunction() {
        auto mainType = llvm::FunctionType::get(int32Type, {}, false);
        auto mainFunc = llvm::Function::Create(mainType, llvm::Function::ExternalLinkage, "main", module.get());
        
        auto entry = llvm::BasicBlock::Create(context, "entry", mainFunc);
        builder->SetInsertPoint(entry);
        
        return mainFunc;
    }
    
    void compileToken(const Token& token) {
        switch (token.type) {
            case TokenType::NUM: {
                double val = std::stod(token.value);
                auto constant = llvm::ConstantFP::get(doubleType, val);
                auto pushFunc = module->getFunction("push_double");
                builder->CreateCall(pushFunc, {constant});
                break;
            }
            
            case TokenType::ARR: {
                // Create array constant
                auto sizeConstant = llvm::ConstantInt::get(int32Type, token.data.size());
                
                // Allocate array data
                auto mallocFunc = module->getFunction("malloc");
                if (!mallocFunc) {
                    auto mallocType = llvm::FunctionType::get(int8PtrType, {int32Type}, false);
                    mallocFunc = llvm::Function::Create(mallocType, llvm::Function::ExternalLinkage, "malloc", module.get());
                }
                
                auto dataSize = builder->CreateMul(sizeConstant, 
                    llvm::ConstantInt::get(int32Type, sizeof(double)));
                auto dataPtr = builder->CreateCall(mallocFunc, {dataSize});
                auto typedDataPtr = builder->CreateBitCast(dataPtr, doubleType->getPointerTo());
                
                // Store array data
                for (size_t i = 0; i < token.data.size(); ++i) {
                    auto idx = llvm::ConstantInt::get(int32Type, i);
                    auto elemPtr = builder->CreateGEP(doubleType, typedDataPtr, {idx});
                    auto val = llvm::ConstantFP::get(doubleType, token.data[i]);
                    builder->CreateStore(val, elemPtr);
                }
                
                // Create array struct
                auto arrayAlloca = builder->CreateAlloca(arrayType);
                auto sizePtr = builder->CreateStructGEP(arrayType, arrayAlloca, 0);
                auto dataPtrPtr = builder->CreateStructGEP(arrayType, arrayAlloca, 1);
                
                builder->CreateStore(sizeConstant, sizePtr);
                builder->CreateStore(typedDataPtr, dataPtrPtr);
                
                auto arrayVal = builder->CreateLoad(arrayType, arrayAlloca);
                auto pushFunc = module->getFunction("push_array");
                builder->CreateCall(pushFunc, {arrayVal});
                break;
            }
            
            case TokenType::ADD: {
                auto popFunc = module->getFunction("pop_double");
                auto pushFunc = module->getFunction("push_double");
                
                auto b = builder->CreateCall(popFunc, {});
                auto a = builder->CreateCall(popFunc, {});
                auto result = builder->CreateFAdd(a, b);
                builder->CreateCall(pushFunc, {result});
                break;
            }
            
            case TokenType::SUB: {
                auto popFunc = module->getFunction("pop_double");
                auto pushFunc = module->getFunction("push_double");
                
                auto b = builder->CreateCall(popFunc, {});
                auto a = builder->CreateCall(popFunc, {});
                auto result = builder->CreateFSub(a, b);
                builder->CreateCall(pushFunc, {result});
                break;
            }
            
            case TokenType::MUL: {
                auto popFunc = module->getFunction("pop_double");
                auto pushFunc = module->getFunction("push_double");
                
                auto b = builder->CreateCall(popFunc, {});
                auto a = builder->CreateCall(popFunc, {});
                auto result = builder->CreateFMul(a, b);
                builder->CreateCall(pushFunc, {result});
                break;
            }
            
            case TokenType::DIV: {
                auto popFunc = module->getFunction("pop_double");
                auto pushFunc = module->getFunction("push_double");
                
                auto b = builder->CreateCall(popFunc, {});
                auto a = builder->CreateCall(popFunc, {});
                auto result = builder->CreateFDiv(a, b);
                builder->CreateCall(pushFunc, {result});
                break;
            }
            
            case TokenType::DUPE: {
                auto popFunc = module->getFunction("pop_double");
                auto pushFunc = module->getFunction("push_double");
                
                auto val = builder->CreateCall(popFunc, {});
                builder->CreateCall(pushFunc, {val});
                builder->CreateCall(pushFunc, {val});
                break;
            }
            
            case TokenType::POP: {
                auto popFunc = module->getFunction("pop_double");
                auto printFunc = module->getFunction("print_double");
                
                auto val = builder->CreateCall(popFunc, {});
                builder->CreateCall(printFunc, {val});
                break;
            }
            
            case TokenType::FLIP: {
                auto popFunc = module->getFunction("pop_double");
                auto pushFunc = module->getFunction("push_double");
                
                auto b = builder->CreateCall(popFunc, {});
                auto a = builder->CreateCall(popFunc, {});
                builder->CreateCall(pushFunc, {b});
                builder->CreateCall(pushFunc, {a});
                break;
            }
            
            case TokenType::ID: {
                // Function call
                auto func = functions.find(token.value);
                if (func != functions.end()) {
                    builder->CreateCall(func->second, {});
                }
                break;
            }
            
            case TokenType::NL:
            case TokenType::EOF_TOK:
                // Skip these
                break;
                
            default:
                std::cerr << "Warning: Unhandled token type: " << static_cast<int>(token.type) << std::endl;
                break;
        }
    }
    
    void compile(const std::vector<Token>& tokens) {
        auto mainFunc = createMainFunction();
        
        bool inDef = false;
        std::string currentDefName;
        std::vector<Token> currentDefBody;
        
        for (const auto& token : tokens) {
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
                    // Start of function body
                    currentDefBody.clear();
                } else if (token.type == TokenType::NL && !currentDefName.empty()) {
                    // End of function definition
                    createFunction(currentDefName, currentDefBody);
                    currentDefName.clear();
                    currentDefBody.clear();
                } else if (!currentDefName.empty() && token.type != TokenType::BND) {
                    currentDefBody.push_back(token);
                }
            } else {
                compileToken(token);
            }
        }
        
        // Return 0 from main
        builder->CreateRet(llvm::ConstantInt::get(int32Type, 0));
    }
    
    void createFunction(const std::string& name, const std::vector<Token>& body) {
        auto voidType = llvm::Type::getVoidTy(context);
        auto funcType = llvm::FunctionType::get(voidType, {}, false);
        auto func = llvm::Function::Create(funcType, llvm::Function::InternalLinkage, name, module.get());
        
        auto entry = llvm::BasicBlock::Create(context, "entry", func);
        auto oldInsertPoint = builder->GetInsertBlock();
        builder->SetInsertPoint(entry);
        
        for (const auto& token : body) {
            compileToken(token);
        }
        
        builder->CreateRetVoid();
        builder->SetInsertPoint(oldInsertPoint);
        
        functions[name] = func;
    }
    
    void generateLLVMIR(const std::string& filename) {
        std::error_code EC;
        llvm::raw_fd_ostream file(filename, EC);
        if (EC) {
            std::cerr << "Error opening file: " << EC.message() << std::endl;
            return;
        }
        module->print(file, nullptr);
    }
    
    void generateObjectFile(const std::string& filename) {
        // Initialize target
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
        auto targetMachine = target->createTargetMachine(targetTriple, CPU, features, opt, relocModel);

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
    
    void generateExecutable(const std::string& filename, const std::string& runtimeLibPath = "") {
        // First generate object file
        std::string objFile = filename + ".o";
        generateObjectFile(objFile);
        
        // Link with runtime library
        std::string linkCmd = "clang++ -o " + filename + " " + objFile;
        if (!runtimeLibPath.empty()) {
            linkCmd += " " + runtimeLibPath;
        } else {
            // Link with built-in runtime
            linkCmd += " -lm"; // Math library for basic ops
        }
        
        int result = std::system(linkCmd.c_str());
        if (result != 0) {
            std::cerr << "Linking failed!" << std::endl;
        } else {
            std::cout << "Executable generated: " << filename << std::endl;
            // Clean up object file
            std::remove(objFile.c_str());
        }
    }
    
    void optimize() {
        auto passManager = std::make_unique<llvm::legacy::FunctionPassManager>(module.get());
        passManager->add(llvm::createInstructionCombiningPass());
        passManager->add(llvm::createReassociatePass());
        passManager->add(llvm::createGVNPass());
        passManager->add(llvm::createCFGSimplificationPass());
        passManager->doInitialization();
        
        for (auto& func : *module) {
            passManager->run(func);
        }
    }
    
    bool verify() {
        return !llvm::verifyModule(*module, &llvm::errs());
    }
};

// Token parser from Python output
std::vector<Token> parseTokenFile(const std::string& filename) {
    std::vector<Token> tokens;
    std::ifstream file(filename);
    std::string line;
    
    std::regex tokenRegex(R"(Token\(value='([^']*)', type=<TokenType\.(\w+): \d+>\))");
    std::regex arrayRegex(R"(Token\(value=\(\[([^\]]*)\], \[([^\]]*)\]\), type=<TokenType\.ARR: 3>\))");
    
    while (std::getline(file, line)) {
        std::smatch match;
        
        if (std::regex_match(line, match, arrayRegex)) {
            // Parse array token
            std::string shapeStr = match[1].str();
            std::string dataStr = match[2].str();
            
            std::vector<int> shape;
            std::vector<double> data;
            
            // Parse shape
            std::stringstream ss(shapeStr);
            std::string item;
            while (std::getline(ss, item, ',')) {
                if (!item.empty()) {
                    shape.push_back(std::stoi(item));
                }
            }
            
            // Parse data
            ss = std::stringstream(dataStr);
            while (std::getline(ss, item, ',')) {
                if (!item.empty()) {
                    data.push_back(std::stod(item));
                }
            }
            
            tokens.emplace_back("array", TokenType::ARR, shape, data);
        } else if (std::regex_match(line, match, tokenRegex)) {
            std::string value = match[1].str();
            std::string typeStr = match[2].str();
            
            TokenType type = TokenType::EOF_TOK;
            if (typeStr == "NUM") type = TokenType::NUM;
            else if (typeStr == "ID") type = TokenType::ID;
            else if (typeStr == "ADD") type = TokenType::ADD;
            else if (typeStr == "SUB") type = TokenType::SUB;
            else if (typeStr == "MUL") type = TokenType::MUL;
            else if (typeStr == "DIV") type = TokenType::DIV;
            else if (typeStr == "POP") type = TokenType::POP;
            else if (typeStr == "DUPE") type = TokenType::DUPE;
            else if (typeStr == "FLIP") type = TokenType::FLIP;
            else if (typeStr == "BND") type = TokenType::BND;
            else if (typeStr == "NL") type = TokenType::NL;
            else if (typeStr == "DEF_START") type = TokenType::DEF_START;
            else if (typeStr == "DEF_END") type = TokenType::DEF_END;
            else if (typeStr == "EOF") type = TokenType::EOF_TOK;
            
            tokens.emplace_back(value, type);
        }
    }
    
    return tokens;
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 5) {
        std::cerr << "Usage: " << argv[0] << " <source_file> <output> [--ir|--obj|--exe] [lexer_path]" << std::endl;
        std::cerr << "  source_file: Stack language source file" << std::endl;
        std::cerr << "  output:      Output file name" << std::endl;
        std::cerr << "  --ir:        Generate LLVM IR (.ll file)" << std::endl;
        std::cerr << "  --obj:       Generate object file (.o)" << std::endl;
        std::cerr << "  --exe:       Generate executable (default)" << std::endl;
        std::cerr << "  lexer_path:  Path to Python lexer (default: ./lexer.py)" << std::endl;
        return 1;
    }
    
    std::string sourceFile = argv[1];
    std::string outputFile = argv[2];
    std::string outputMode = (argc >= 4) ? argv[3] : "--exe";
    std::string lexerPath = (argc >= 5) ? argv[4] : "./lexer.py";
    
    // Check if source file exists
    std::ifstream sourceCheck(sourceFile);
    if (!sourceCheck.good()) {
        std::cerr << "Error: Source file '" << sourceFile << "' not found!" << std::endl;
        return 1;
    }
    sourceCheck.close();
    
    // Check if lexer exists
    std::ifstream lexerCheck(lexerPath);
    if (!lexerCheck.good()) {
        std::cerr << "Error: Lexer file '" << lexerPath << "' not found!" << std::endl;
        std::cerr << "Make sure the Python lexer is in the current directory or specify its path." << std::endl;
        return 1;
    }
    lexerCheck.close();
    
    std::cout << "Compiling Stack Language source: " << sourceFile << std::endl;
    
    // Step 1: Run Python lexer on the source file
    std::string tmpTokenFile = "tmp_tokens.txt";
    std::string lexerCommand = "python3 \"" + lexerPath + "\" \"" + sourceFile + "\" \"" + tmpTokenFile + "\"";
    
    std::cout << "Running lexer: " << lexerCommand << std::endl;
    int lexerResult = std::system(lexerCommand.c_str());
    
    if (lexerResult != 0) {
        std::cerr << "Error: Lexer failed with exit code " << lexerResult << std::endl;
        std::cerr << "Make sure Python 3 is installed and the lexer script is correct." << std::endl;
        return 1;
    }
    
    // Check if token file was generated
    std::ifstream tokenCheck(tmpTokenFile);
    if (!tokenCheck.good()) {
        std::cerr << "Error: Lexer did not generate token file!" << std::endl;
        return 1;
    }
    tokenCheck.close();
    
    std::cout << "Lexing complete. Reading tokens..." << std::endl;
    
    // Step 2: Parse the generated token file
    auto tokens = parseTokenFile(tmpTokenFile);
    
    if (tokens.empty()) {
        std::cerr << "Error: No tokens parsed from " << tmpTokenFile << std::endl;
        return 1;
    }
    
    std::cout << "Parsed " << tokens.size() << " tokens. Compiling..." << std::endl;
    
    // Step 3: Initialize LLVM and compile
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    
    StackLangCompiler compiler;
    compiler.compile(tokens);
    
    if (!compiler.verify()) {
        std::cerr << "Module verification failed!" << std::endl;
        // Clean up temp file
        std::remove(tmpTokenFile.c_str());
        return 1;
    }
    
    std::cout << "Module verified. Optimizing..." << std::endl;
    compiler.optimize();
    
    // Step 4: Generate output based on mode
    if (outputMode == "--ir") {
        compiler.generateLLVMIR(outputFile + ".ll");
        std::cout << "LLVM IR generated: " << outputFile << ".ll" << std::endl;
    } else if (outputMode == "--obj") {
        compiler.generateObjectFile(outputFile + ".o");
        std::cout << "Object file generated: " << outputFile << ".o" << std::endl;
    } else if (outputMode == "--exe") {
        compiler.generateExecutable(outputFile);
        std::cout << "Executable generated: " << outputFile << std::endl;
    } else {
        std::cerr << "Unknown output mode: " << outputMode << std::endl;
        // Clean up temp file
        std::remove(tmpTokenFile.c_str());
        return 1;
    }
    
    // Step 5: Clean up temporary token file
    std::remove(tmpTokenFile.c_str());
    
    std::cout << "Compilation complete!" << std::endl;
    if (outputMode == "--exe") {
        std::cout << "Run with: ./" << outputFile << std::endl;
    }
    
    return 0;
}