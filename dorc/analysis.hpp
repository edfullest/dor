#pragma once

#include <unordered_map>
#include <vector>
#include <initializer_list>
#include "ast.hpp"
#include "types.hpp"

using ast::Ptr;
using ast::Atom;
using ast::List;

enum scope_t {
    EXTERN,
    GLOBAL, 
    PARAMETER, 
    LOCAL, 
    CLOSURE
};


// NOTE consider distinguishing between const and final
enum mutability_t {
    CONST, // constant. for extern & global, this means known at compile/link/load time.
    VAR    // mutable
};

/*enum storage_t { // completely determined by scope!
    AUTO, // stack variables
    STATIC // 
};*/

struct Type;
struct Expression;

// DO NOT Ptr<Binding> !!!
struct Binding {
    scope_t scope;
    mutability_t mut;  
    
    Ptr<Type> type;
    Sym name;
    Sym qualified_name;
    
    Ptr<Expression> value;
    Binding *parent;
    
    bool defined;
    bool constructor;
    bool isconstexpr;
    
    int id;
    
    Binding() 
        : id(-1)
        , defined(false)
        , constructor(false)
        , isconstexpr(false) {}
    Binding(const Binding &b)
        : scope(b.scope)
        , parent(b.parent)
        , type(b.type)
        , name(b.name)
        , qualified_name(b.qualified_name)
        , defined(b.defined)
        , value(b.value)
        , constructor(false)
        , isconstexpr(false)
        , id(-1) {}
    Binding(Binding &parent, scope_t scope) 
        : scope(scope)
        , parent(&parent)
        , type(parent.type)
        , name(parent.name)
        , defined(parent.defined)
        , qualified_name(parent.qualified_name)
        , value(parent.value)
        , constructor(false)
        , isconstexpr(false)
        , id(-1) {}
        
    
};

struct Env : TypeEnv {
    Ptr<Env> parent;
    std::unordered_map<Sym, Binding> dictionary;
    
    virtual Binding *find(Sym name);
    
    Env(Ptr<Env> parent = nullptr) : parent(parent), TypeEnv(parent) {}
    
    virtual bool isGlobal() {return false;}
    
    virtual void setLocalsSize(size_t size) {
        assert(parent != nullptr && !isGlobal());
        parent->setLocalsSize(size);
    }
    virtual size_t getLocalsSize() {
        assert(parent != nullptr && !isGlobal());
        return parent->getLocalsSize();
    }
};

struct Globals : Env {
    std::vector<Binding *> externs; 
    std::vector<Binding *> globals; 
    
    Globals() : Env() {}
    // handle externs
    virtual Binding *find(Sym name);
    
    virtual bool isGlobal() {return true;}
    
    void assignIds();
};


struct Function;
struct Literal;
struct Reference;
struct Conditional;
struct Sequence;
struct Application;
struct Assignment;
struct Match;
struct While;
struct Mutation;

struct ExpressionVisitor {
    virtual void visit(Function &) = 0;
    virtual void visit(Literal &) = 0;
    virtual void visit(Reference &) = 0;
    virtual void visit(Conditional &) = 0;
    virtual void visit(Sequence &) = 0;
    virtual void visit(Application &) = 0;
    virtual void visit(Assignment &) = 0;
    virtual void visit(Match &) = 0;
    virtual void visit(While &) = 0;
    virtual void visit(Mutation &) = 0;
};

struct Expression {
    int line, column;
    Ptr<Type> type;
    
    Expression(int line = -1, int column = -1) 
        : line(line), column(column), type(newPtr<TypeVar>(K1)) {}
    
    virtual void dump(int level) = 0;
    virtual void accept(ExpressionVisitor &v) = 0;
};

struct Function : Expression, Env {
    std::vector<Binding *> closure;
    std::vector<Binding *> parameters; 
    
    Ptr<Expression> body;
    size_t locals_size;
    size_t max_locals;
    
    int id;
    
    virtual Binding *find(Sym name);
    
    Function(Ptr<Env> parent) : 
        Env(parent), locals_size(0), max_locals(0), id(-1) {} 
    
    virtual void dump(int level) {
        ast::print_indent(level);
        std::cout << "FUNCTION : ";
        type->dump();
        std::cout << std::endl;
        
        ast::print_indent(level + 1);
        std::cout << "CLOSURE ";
        for(Binding *b : closure) {
            std::cout << b->name.str() << " ";
        }
        std::cout << std::endl;
        
        ast::print_indent(level + 1);
        std::cout << "PARAMS ";
        for(Binding *b : parameters) {
            std::cout << b->name.str() << " ";
        }
        std::cout << std::endl;
        
        ast::print_indent(level + 1);
        std::cout << "LOCALS " << max_locals << std::endl;
        
        body->dump(level + 1);
    }
    
    void assignIds() {
        size_t id = 0;
        
        for(Binding *b : closure) {
            b->id = id++;
        }
        
        for(Binding *b : parameters) {
            b->id = id++;
        }
    }
    
    virtual void setLocalsSize(size_t size) {
        locals_size = size;
        max_locals = std::max(max_locals, locals_size);
    }
    virtual size_t getLocalsSize() {
        return locals_size;
    }
    
    virtual void accept(ExpressionVisitor &v) {
        v.visit(*this);
    }
};

struct Literal : Expression {
    Ptr<Atom> value;
    
    Literal(Ptr<Atom> value, Ptr<Type> type) : value(value) {
        line = value->line;
        column = value->column;
        this->type = type;
    }
    
    virtual void dump(int level) {
        value->dump(level);
        std::cout << " : ";
        type->dump();
        std::cout << std::endl;
    }
    
    virtual void accept(ExpressionVisitor &v) {
        v.visit(*this);
    }
};

struct Reference : Expression { // lvalue/rvalue distinciton!!!
    Binding *binding;
    
    Reference(Binding *b) : binding(b) {
        type = b->type;
    }
    
    
    virtual void dump(int level) {
        ast::print_indent(level);
        
        switch(binding->scope) {
        case PARAMETER: std::cout << "PARAM("; break;
        case CLOSURE: std::cout << "CLOSURE("; break;
        case LOCAL: std::cout << "LOCAL("; break;    
        case GLOBAL: std::cout << "GLOBAL("; break;  
        case EXTERN: std::cout << "EXTERN("; break;    
        }

        std::cout << binding->id << "?";
        
        std::cout << binding->name.str() << ") : ";
        binding->type->dump();
        std::cout << std::endl;
    }
    
    virtual void accept(ExpressionVisitor &v) {
        v.visit(*this);
    }
};

struct Application : Expression {
    Ptr<Expression> left, right;
    
    Application(Ptr<Expression> left, Ptr<Expression> right)
        : left(left), right(right) {
        assert(this->left!=nullptr);
        assert(this->right!=nullptr);
        //type = newPtr<TypeVar>(K1);
        type = applyTypes(left->type, right->type);
    }
    
    virtual void dump(int level) {
        ast::print_indent(level);
        std::cout << "APPLY : ";
        type->dump();
        std::cout << std::endl;
        
        left->dump(level + 1);
        right->dump(level + 1);
    }
    
    virtual void accept(ExpressionVisitor &v) {
        v.visit(*this);
    }
};

struct Assignment : Expression {
    Ptr<Reference> lhs; // replace by generic lvalue reference??
    Ptr<Expression> rhs;
    
    bool is_const_expr; // 
    
    Assignment(Ptr<Reference> lhs, Ptr<Expression> rhs)
        : lhs(lhs), rhs(rhs), Expression() {
        assert(this->lhs!=nullptr);
        assert(this->rhs!=nullptr);
        
        type = newPtr<TypeVar>(K1);
        assert(unifyTypes(type, this->lhs->type));
        assert(unifyTypes(type, this->rhs->type));
    }
    
    virtual void dump(int level) {
        ast::print_indent(level);
        std::cout << "ASSIGN" << std::endl;
        
        lhs->dump(level + 1);
        rhs->dump(level + 1);
    }
    
    virtual void accept(ExpressionVisitor &v) {
        v.visit(*this);
    }
};

struct Mutation : Expression { // impure! 
    Ptr<Reference> lhs; // replace by generic lvalue reference??
    Ptr<Expression> rhs;
    
    bool is_const_expr; // 
    
    Mutation(Ptr<Reference> lhs, Ptr<Expression> rhs)
        : lhs(lhs), rhs(rhs), Expression() {
        assert(this->lhs!=nullptr);
        assert(this->rhs!=nullptr);
        
        type = newPtr<TypeVar>(K1);
        assert(unifyTypes(type, this->lhs->type));
        assert(unifyTypes(type, this->rhs->type));
    }
    
    virtual void dump(int level) {
        ast::print_indent(level);
        std::cout << "MUTATE" << std::endl;
        
        lhs->dump(level + 1);
        rhs->dump(level + 1);
    }
    
    virtual void accept(ExpressionVisitor &v) {
        v.visit(*this);
    }
};

struct Conditional : Expression {
    Ptr<Expression> condition, on_false, on_true;
    
    Conditional(Ptr<Expression> condition, Ptr<Expression> on_true, Ptr<Expression> on_false)
        : condition(condition), on_false(on_false), on_true(on_true) {
        type = newPtr<TypeVar>(K1);
        assert(unifyTypes(condition->type, Bool));
        assert(unifyTypes(type, on_true->type));
        assert(unifyTypes(type, on_false->type));
    }
    Conditional() {}
    
    virtual void dump(int level) {
        ast::print_indent(level);
        std::cout << "CONDITIONAL" << std::endl;
        
        condition->dump(level + 1);
        on_true->dump(level + 1);
        on_false->dump(level + 1);
    }
    
    virtual void accept(ExpressionVisitor &v) {
        v.visit(*this);
    }
}; 

struct Sequence : Expression {
    Ptr<Env> locals;
    std::vector<Ptr<Expression> > steps;
    
    Sequence(Ptr<Env> parent) : locals(newPtr<Env>(parent)) {}
    Sequence() {}
    
    virtual void dump(int level) {
        ast::print_indent(level);
        std::cout << "SEQUENCE" << std::endl;
        
        for(Ptr<Expression> exp : steps) {
            exp->dump(level + 1);
        }
    }
    
    virtual void accept(ExpressionVisitor &v) {
        v.visit(*this);
    }
};

struct While : Expression {
    Ptr<Expression> condition;
    Ptr<Sequence> body;
    
    While(Ptr<Expression> condition, Ptr<Sequence> body)
        : condition(condition), body(body) {
        type = Void;//newPtr<TypeVar>(K1);
        assert(unifyTypes(condition->type, Bool));
        // no restrictions on the type of the body, return values are simply ignored. 
    }
    While() {}
    
    virtual void dump(int level) {
        ast::print_indent(level);
        std::cout << "WHILE" << std::endl;
        
        condition->dump(level + 1);
        body->dump(level + 1);
    }
    
    virtual void accept(ExpressionVisitor &v) {
        v.visit(*this);
    }
}; 

struct MatchClause : Env {
    Ptr<Expression> pattern, condition, body;
    bool is_pattern;
    
    virtual Binding *find(Sym name);
    
    MatchClause (Ptr<Env> parent) : Env(parent), is_pattern(false) {
        
    }
};

struct Match : Expression {
    Ptr<Expression> exp;
    std::vector<Ptr<MatchClause> > clauses;
    
    // void dump(int level) {}
    
    virtual void accept(ExpressionVisitor &v) {
        v.visit(*this);
    }
    
    virtual void dump(int level) {
        ast::print_indent(level);
        std::cout << "MATCH : ";
        type->dump();
        std::cout << std::endl;
        
        ast::print_indent(level + 1);
        std::cout << "EXP " << std::endl;
        exp->dump(level + 1);
        // std::cout << std::endl;

        for(Ptr<MatchClause> mc : clauses) {
            ast::print_indent(level + 1);
            std::cout << "PATTERN " << std::endl;
            if(mc->pattern != nullptr) mc->pattern->dump(level + 2);
            else {
                ast::print_indent(level + 2);
                std::cout << "ELSE" << std::endl;
            }
            ast::print_indent(level + 1);
            std::cout << "BODY " << std::endl;
            mc->body->dump(level + 2);
            // std::cout << std::endl;

        }
        
    }

};

Ptr<Expression> topLevel(Ptr<Globals> globals, Ptr<Atom> exp);
Ptr<Sequence> program(Ptr<Globals> env, Ptr<List> body);

void initGlobals(Ptr<Globals> g);

extern std::unordered_set<Sym> builtins;

inline bool isConstExpr(Ptr<Expression> expr) {
    if(Ptr<Literal> lit = castPtr<Literal, Expression>(expr)) {
        return true;
    } else if(Ptr<Function> fun = castPtr<Function, Expression>(expr)) {
        return fun->closure.size() == 0;
    }
    
    return false;
}
