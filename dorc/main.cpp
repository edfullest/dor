#include "parser.hpp"
#include "analysis.hpp"
#include "tokens.hpp"
#include "types.hpp"


int main() {
    initLexer();
	parser::initParser();
    
    initTypes();
    
    
	
	Ptr<List> tree = asList(parser::parseGroup(true));
    
    //program->dump(0);
    
    Ptr<Globals> globals = newPtr<Globals>();
    Ptr<Sequence> unit = program(globals, tree);
    
    unit->dump(0);
    std::cout << endl;
}
