/* 
OS Assignment 1 - Linker
Aditya Pandey - ap6624

Note: 
- Input File Needs to have an empty line at the end of the file
- Use of "-std=c++11" necessary on linserv
- Unzip file and run "make"
- Execute linker by using this command - ./linker <file name>
*/

#include <iostream>
#include <fstream>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <string.h>

// Defining Limits
#define DEF_LIMIT 16
#define USE_LIMIT 16
#define INST_LIMIT 512
#define MACHINE_SIZE 512

using namespace std;

// Structure for each token - Updated dynamically
struct tokenStruct {
    char* dup;
    char* token;
    int linePosition, lineCount; 
};

// Symbol is a struct storing info about each symbol read by the linker
// Smbol Table is represented as a map between the "symbol text" and the struct
struct Symbol {
    int used;
    int module;
    int address;
    int redefine;
    int define;
};
map<string, Symbol> symbolTable;
vector<string> symbolInsertOrder;

// The modules variable stores the base address of each module
// The module is specified using an integer - starting from one
map<int, int> modules;

// The memory loc is represented by a map - Mapping the memory to the addressable location
map<int, int> memoryMap;
// The memory error codes are stored here
map<int, int> memoryErrorMap;
// The mapping of memory to the module it is used in is stored here
map<int, int> memoryModuleMap;
// The variable referred by the emory error is stored here
map<int, string> memoryErrorStrMap;

// The mapping of the uselist for each module is stored - along with a flag to indicate use of the variable
map<int, map<string, int> > useListRef;

// Store module information i.e. module number and base address of current module
int moduleNum = 0;
int moduleBase = 0; 

// Store errorCodes for Parse Errors
int errorCode = 0;
int operation=0;

// Flags for end of line/file
int lineEnd = 0;
int eofFlag = 0;

void createModule();
tokenStruct getNextToken(ifstream&infile, int &lineCount, tokenStruct &token, int &lineOffset);
string readSym(tokenStruct token);
char readIEAR(tokenStruct token);
int readInt(tokenStruct token);
void performOperation (char addressmode, int operand, vector<string> &useList, map<int, map<string, int> > &useListRef);
void updateSymbolTable (string sym);
void printSymbolTable (map<string, Symbol> symbolTable, vector<string> symbolInsertOrder);
void validateSymbolwithSize (map<string, Symbol> &symbolTable, vector<string> symbolInsertOrder);
void printNotUseList (map<int, map<string, int> > useListRef, int moduleNum);
void print_memory_map( map<int, int> memoryMap);
void printUnusedVars (map<string, Symbol> symbolTable, int moduleNum);
void __parseerror(int errcode, int linenum, int lineoffset);
void displayLogicalErrors(int err, string s);
void createSymbol(string sym, int val);
Symbol findSymbol(string sym);

int main (int argc, char* argv[]){
    if (argc != 2) {
        cout << "Submit an input file as argument" << endl;
        exit(0);
    }
    char *filename = argv[1];

    // Open File
    ifstream infile(filename);

    int instCountSum = 0;
    int lineCount = 0;
    int lineOffset = 1;
    tokenStruct token = { NULL, NULL, 0, 0 };
    
    // Pass 1 - Tokenizer
    try {
        while (!infile.eof()) {
            createModule();
            token = getNextToken(infile, lineCount, token, lineOffset);
            if (strcmp(token.token,"!EOF")==0)
                break;
            int defcount = readInt(token);
            
            // Check defcount limit
            if (defcount>DEF_LIMIT) {
                errorCode=4;
                throw(4);
            } 
            for (int i=0;i<defcount;i++) {
                token = getNextToken(infile, lineCount, token, lineOffset);
                string sym = readSym(token);
                token = getNextToken(infile, lineCount, token, lineOffset);
                int val = readInt(token);
                createSymbol(sym,val);
            }
            
            token = getNextToken(infile, lineCount, token, lineOffset);
            int usecount = readInt(token);
            // Check usecount limit
            if (usecount>USE_LIMIT) {
                errorCode=5;
                throw(5);
            } 
            for (int i=0;i<usecount;i++) {
                token = getNextToken(infile, lineCount, token, lineOffset);
                string sym = readSym(token);
            }            

            token = getNextToken(infile, lineCount, token, lineOffset);
            int instcount = readInt(token);
            
            instCountSum+=instcount;
            // Check instruction size
            if (instCountSum>INST_LIMIT) {
                errorCode=6;
                throw(6);
            } 
            for (int i=0;i<instcount;i++) {
                token = getNextToken(infile, lineCount, token, lineOffset);
                char addressmode = readIEAR(token);
                token = getNextToken(infile, lineCount, token, lineOffset);
                int operand = readInt(token);
            }
            moduleBase+=instcount;
        }
    } catch(int code) {
        
        __parseerror(errorCode, lineCount, lineOffset);
        exit(0);
    }
    createModule();
    
    // ========= Pass 1 Done =========
    validateSymbolwithSize(symbolTable, symbolInsertOrder);
    printSymbolTable(symbolTable, symbolInsertOrder);


    infile.clear();
    infile.close();

    // Pass 2
    lineCount = 0;
    lineOffset = 1;
    moduleNum=0;

    // ======= Pass 2 Starting =======
    
    ifstream infile2(filename);
    tokenStruct token_pass_2 = { NULL, NULL, 0, 0 };
    while (!infile2.eof()) {

        vector<string> useList;
        moduleNum++;

        token_pass_2 = getNextToken(infile2, lineCount, token_pass_2, lineOffset);
        
        if (strcmp(token_pass_2.token,"!EOF")==0)
            break;
        
        int defcount = readInt(token_pass_2);

        for (int i=0;i<defcount;i++) {
            token_pass_2 = getNextToken(infile2, lineCount, token_pass_2, lineOffset);
            string sym = readSym(token_pass_2);
            token_pass_2 = getNextToken(infile2, lineCount, token_pass_2, lineOffset);
            int val = readInt(token_pass_2);
        }
        token_pass_2 = getNextToken(infile2, lineCount, token_pass_2, lineOffset);
        int usecount = readInt(token_pass_2);

        for (int i=0;i<usecount;i++) {
            token_pass_2 = getNextToken(infile2, lineCount, token_pass_2, lineOffset);
            string sym = readSym(token_pass_2);
            useList.push_back(sym);
            useListRef[moduleNum][sym]=0;
            updateSymbolTable(sym);
        }            

        token_pass_2 = getNextToken(infile2, lineCount, token_pass_2, lineOffset);
        int instcount = readInt(token_pass_2);

        for (int i=0;i<instcount;i++) {
            token_pass_2 = getNextToken(infile2, lineCount, token_pass_2, lineOffset);
            char addressmode = readIEAR(token_pass_2);
            token_pass_2 = getNextToken(infile2, lineCount, token_pass_2, lineOffset);
            int operand = readInt(token_pass_2);
            performOperation(addressmode, operand, useList, useListRef);
        }

    }
    // ======= Pass 2 Done =======

    print_memory_map(memoryMap);

    printUnusedVars(symbolTable, moduleNum);
    
}

tokenStruct getNextToken(ifstream &infile, int &lineCount, tokenStruct &prevToken, int &lineOffset) {
    string temp;
    char *newDup;
    char *newToken = NULL;
    int position;
    if (prevToken.token == NULL) {
        //Reading a new line
        while (newToken == NULL) {
            // Line is over
            prevToken.linePosition=1;
            getline(infile, temp);
            lineOffset=1;
            if (infile.eof()) {
                
                prevToken.linePosition=prevToken.linePosition + strlen(prevToken.token);

                lineOffset=prevToken.linePosition;
                prevToken.token="!EOF";
                eofFlag=1;

                return prevToken;
            }
            lineCount++;
            newDup = strdup(temp.c_str());
            lineEnd = temp.size();
            newToken = strtok(newDup, "\n \t");
            position = newToken - newDup;
            prevToken.token="";
        }
    } else {
        // If continuing a line
        newToken = strtok(NULL, "\n \t");
        if (newToken == NULL){
            while (newToken == NULL) {
                // Line is over
                getline(infile, temp);
                if (infile.eof()) {
                    prevToken.linePosition=prevToken.linePosition + strlen(prevToken.token);
                    lineOffset=prevToken.linePosition;
                    prevToken.token="!EOF";
                    eofFlag=1;
                    return prevToken;
                }
                lineCount++;
                newDup = strdup(temp.c_str());
                lineEnd = temp.size();
                newToken = strtok(newDup, "\n \t");
                position = newToken - newDup;
                prevToken.token="";
            }
        } else {
            newDup = prevToken.dup;
            position = newToken - newDup;
        }
    }
    lineOffset=position+1;
    prevToken.linePosition=position+1;
    prevToken.lineCount=lineCount;
    prevToken.dup=newDup;
    prevToken.token=newToken;
    
    return prevToken;
}

int readInt(tokenStruct token) {
    // Read integer from given token
    char* value = token.token;
    int i=0;
    while (value[i] != '\0') {
        if (isdigit(value[i++])==false){
            errorCode=0;
            throw(0);
        }
    }
    return atoi(value);
}

string readSym(tokenStruct token) {
    // Read symbol from given token
    string newSymbol;
    char *value = token.token;
    
    if(strcmp(value, "!EOF")==0) {
        lineEnd++;
        errorCode=1;
        throw(1);
    }
    int i=0;
    if (isalpha(value[i++])==false) {
        errorCode=1;
        throw(1);
    }
    while (value[i] != '\0') {
        if (isalnum(value[i++])==false) {
            errorCode=1;
            throw(1);
        }
    }
    string symbolVal(value);
    if (symbolVal.length()>16) {
        errorCode=3;
        throw(3);
    }
    return symbolVal;
}

void createSymbol(string sym, int val) {
    // Initialize symbol with given value and add to table
    Symbol tableVal = {
        0, 
        moduleNum, 
        modules[moduleNum]+val, 
        0, 1
    };

    if (symbolTable.find(sym)==symbolTable.end()) {
        symbolTable[sym]=tableVal;
        symbolInsertOrder.push_back(sym);
    } else {
        symbolTable[sym].redefine=1;
    }
}

Symbol findSymbol(string sym) {
    // Search for symbol struct based on key
    for (int i = 0; i < symbolInsertOrder.size(); i++) {   
        const string &s = symbolInsertOrder[i];
        if ( sym.compare(s)==0 ) {
            return symbolTable[s];
        }
    }
    Symbol notPresent = { -1, -1, -1, -1,-1 };
    return notPresent;
}

void createModule(){
    moduleNum++;
    modules[moduleNum]=moduleBase;
}

char readIEAR(tokenStruct token) {
    // Validate Instruction
    char* value = token.token;
    if (strlen(value)>1 || !(value[0]!='I' || value[0]!='E' || value[0]!='A' || value[0]!='R')) {
        errorCode=2;
        throw(2);
    }
    else 
        return char(value[0]);
}

void updateSymbolTable (string sym) {
    // If symbol is used in E instr
    Symbol tableVal = symbolTable[sym];
    tableVal.used=1;
    symbolTable[sym] = tableVal;
}

void performOperation (char addressmode, int operand, vector<string> &useList, map<int, map<string, int> > &useListRef) {

    int op = operand/1000;
    int offset = operand%1000;
    switch (addressmode){
        case 'I' : {
            if (operand>9999) {
                memoryErrorMap[operation]=6;
                memoryModuleMap[operation]=moduleNum;
                memoryMap[operation++]=9999;
            } else {
                memoryModuleMap[operation]=moduleNum;
                memoryMap[operation++]=operand;
            }
            break;
        }
        case 'E' : {
            if (op>=10) {
                memoryErrorMap[operation] = 7;
                memoryModuleMap[operation]=moduleNum;
                memoryMap[operation++]=9999;
                break;    
            } else if (offset >= useList.size()) {
                memoryErrorMap[operation]=3;
                memoryModuleMap[operation]=moduleNum;
                memoryMap[operation++] = operand;

            } else {
                string referred_variable = useList[offset];
                
                useListRef[moduleNum][referred_variable] = 1;
                int variable_offset = 0;
                
                Symbol referred_symbol = findSymbol(referred_variable);
                if (referred_symbol.address == -1) {
                    memoryErrorMap[operation]=4;
                    memoryErrorStrMap[operation]=referred_variable;
                    variable_offset=0;
                } else {
                    variable_offset = referred_symbol.address;
                }
                memoryModuleMap[operation]=moduleNum;
                memoryMap[operation++]=op*1000+variable_offset;
                
            }
            break;
        }
        case 'A' : {
            if (op>=10) {
                memoryErrorMap[operation] = 7;
                memoryModuleMap[operation]=moduleNum;
                memoryMap[operation++]=9999;
                break;    
            } else if (offset>=MACHINE_SIZE) {
                memoryErrorMap[operation] = 1;
                memoryModuleMap[operation]=moduleNum;
                memoryMap[operation++] = (operand/1000)*1000;
            } else {
                memoryModuleMap[operation]=moduleNum;
                memoryMap[operation++]=operand; 
            }
            break;
        }
        case 'R' : {
            if (op>=10) {
                memoryErrorMap[operation] = 7;
                memoryModuleMap[operation]=moduleNum;
                memoryMap[operation++]=9999;
                break;    
            }

            int module_size = modules[moduleNum+1]-modules[moduleNum];

            if (offset>module_size) {
                operand=op*1000;
                memoryErrorMap[operation] = 2;
            }
            memoryModuleMap[operation]=moduleNum;
            memoryMap[operation++]=operand+modules[moduleNum];
            break;
        }
    }
}

void printSymbolTable (map<string, Symbol> symbolTable, vector<string> symbolInsertOrder) {
    cout << "Symbol Table" << endl;
    for (int i = 0; i < symbolInsertOrder.size(); i++) {   
        const string &s = symbolInsertOrder[i];
        Symbol symbol = symbolTable[s];
        cout << s << '=' << symbol.address;
        if (symbol.redefine==1) {
            cout << " Error: This variable is multiple times defined; first value used" << endl;;
        } else {
            cout << endl;
        }
    }
}

void validateSymbolwithSize (map<string, Symbol> &symbolTable, vector<string> symbolInsertOrder) {
    // Checking is symbol size/addr is allowed
    for (int i = 0; i < symbolInsertOrder.size(); i++) {   
        const string &s = symbolInsertOrder[i];
        Symbol symbol = symbolTable[s];
        
        if ((symbol.address - modules[symbol.module]) >= (modules[symbol.module+1] - modules[symbol.module])) {
            cout << "Warning: Module " << symbol.module << ": " << s << " too big " << (symbol.address - modules[symbol.module]) ;
            cout << " (max=" << (modules[symbol.module+1] - modules[symbol.module])-1 << ") assume zero relative" << endl;
            symbol.address = modules[symbol.module];
            symbolTable[s] = symbol;
        }
    }
}

void printUnusedVars (map<string, Symbol> symbolTable, int moduleNum) {
    // Defined but not used
    for (int i = 0; i < symbolInsertOrder.size(); i++) {   
        const string &s = symbolInsertOrder[i];
        Symbol symbol = symbolTable[s];
        if (symbol.used==0) {
            cout << "Warning: Module " << symbol.module << ": " << s << " was defined but never used" << endl;
        }
    }
}

void print_memory_map(map<int, int> memoryMap) {
    // Printing mem addresses with Errors/Warnings
    cout << endl << "Memory Map" << endl;
    int prevModule = 1;
    int currModule;
    map<int, int>::iterator it;
    for (it = memoryMap.begin(); it != memoryMap.end(); it++) {        
        currModule = memoryModuleMap[it->first];
        if( currModule != prevModule) {
            for (int i=prevModule; i<currModule; i++) {
                printNotUseList(useListRef, i);
            }
        }
        cout << fixed << setfill('0') << setw(3) << it->first << ": ";
        cout << fixed << setfill('0') << setw(4) << it->second ;

        displayLogicalErrors(memoryErrorMap[it->first], memoryErrorStrMap[it->first]);
        cout << endl;
        prevModule=currModule;
    }
    for (int i=prevModule; i<moduleNum; i++) {
        printNotUseList(useListRef, i);
    }
    cout<< endl;
}

void printNotUseList (map<int, map<string, int> > useListRef, int moduleNum) {
    // Print vars in uselist but not referred to
    map<string, int>::iterator it;
    map<string, int> useListModule = useListRef[moduleNum];
    for (it = useListModule.begin(); it != useListModule.end(); it++) {
        if (it->second == 0) {
            cout << "Warning: Module " << moduleNum << ": " << it->first << " appeared in the uselist but was not actually used" << endl;
        }
    }
}

void __parseerror(int errcode, int linenum, int lineoffset) {
    const char* errstr[] = {
        "NUM_EXPECTED", // Number expect, anything >= 2^30 is not a number either 
        "SYM_EXPECTED", // Symbol Expected
        "ADDR_EXPECTED", // Addressing Expected which is A/E/I/R
        "SYM_TOO_LONG", // Symbol Name is too long
        "TOO_MANY_DEF_IN_MODULE", // >16
        "TOO_MANY_USE_IN_MODULE", // >16
        "TOO_MANY_INSTR", // total num_instr exceeds memory size (512)
    };
    printf("Parse Error line %d offset %d: %s\n", linenum, lineoffset, errstr[errcode]); 
}

void displayLogicalErrors(int err, string s) {
    // Display errors before quitting program
    if (err == 1) 
        cout << " Error: Absolute address exceeds machine size; zero used";
    else if (err == 2) 
        cout << " Error: Relative address exceeds module size; zero used";
    else if (err == 3) 
        cout << " Error: External address exceeds length of uselist; treated as immediate";
    else if (err == 4) 
        cout << " Error: " << s << " is not defined; zero used" ;
    else if (err == 5) 
        cout << " Error: This variable is multiple times defined; first value used";
    else if (err == 6) 
        cout << " Error: Illegal immediate value; treated as 9999";
    else if (err == 7) 
        cout << " Error: Illegal opcode; treated as 9999";
}