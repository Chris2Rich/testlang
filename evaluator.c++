#include <iostream>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>



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

    remove(argv[2]);

    for(int i = 0; i < tokens.size(); i++){
        std::cout << tokens[i] << "\n";
    }

    return 0;
}