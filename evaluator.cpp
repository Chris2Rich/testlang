#include <iostream>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include "./lib/stack.cpp"
#include "./lib/ndarray.cpp"

/*input file, output file*/
int main(int argc, char** argv){
    if(argc != 3){
        std::cout << "Error: expected 3 arguements, got: " << argc;
        return 1;
    }

    std::string cmd = "py ./lexer.py";
    cmd = cmd.append(" ");
    cmd = cmd.append(argv[1]);
    cmd = cmd.append(" ");
    cmd = cmd.append(argv[2]);
    
    int lex_success = system(cmd.c_str());
    std::ifstream file(argv[2]);

    std::vector<std::string> tokens = {};
    while(not file.eof()){
        char tmp[256];
        file.getline(tmp, 256);
        tokens.push_back(tmp);
    }

    file.close();
    remove(argv[2]);

    for(auto x: tokens){
        std::cout << x << "\n";
    }

    //Actual stack, contains pointers to other stacks in the order they appear in program, eg could point to int stack twice then fn stack causing evaluation of dyadic function.
    return 0;
}