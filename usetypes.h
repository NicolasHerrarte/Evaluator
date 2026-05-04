#include "hash.h"
#include "allocator.h"

#define MAX_DIMS 4
#define STORAGE_NAME_CAPACITY 10

#define LOCAL_ARENA_SIZE 1024
#define GLOBAL_ARENA_SIZE 4136

enum NodeClass{
    STATLIST,
    IFNODE,
    WHILENODE,
    FORNODE,
    VARDECL,
    ASSIGNSTORAGE,
    ARRINST,
    FUNCDECL,
    VARASSIGN,
    PRIMITIVETYPE,
    ARRTYPE,
    BREAK,
    CONTINUE,
    RETURN,
    ARGUMENTS,
    PARAMETERS,
    UNIPARAM,
    LOGICOR,
    LOGICAND,
    COMPNODE,
    SUM,
    SUBTRACT,
    MULT,
    DIVISION,
    ARRACCESS,
    FUNCCALL,
    PROPERTYACCESS,
    ACCESS,
};

typedef enum {
    SIGNAL_BREAK,
    SIGNAL_CONTINUE,
    SIGNAL_RETURN,
} SignalTag;

typedef enum {
    VAR_TYPE_INT, 
    VAR_TYPE_BOOL, 
    VAR_TYPE_FLOAT,
    VAR_TYPE_STRING,
    VAR_TYPE_VOID,
} TypeTag;

typedef enum {
    PARAMETER_MODE, 
    ARGUMENT_MODE, 
} ArgMode;

typedef enum {
    KIND_PRIMITIVE,
    KIND_ARRAY,
    KIND_POINTER,
} TypeKind;

struct ArrType{
    TypeTag elem_type;
    int sizes[MAX_DIMS];
    int ndims;
};

struct PtrType{
    struct Type* pointee_type;
    bool unpack;
};

typedef struct Type {
    TypeKind kind;
    union {
        TypeTag primitive_tag;
        struct ArrType array;
        struct PtrType pointer;
    };
} Type;

enum ValTag{
    VV_UNDEFINED,
    VV_INT,
    VV_FLOAT,
    VV_BOOL,
    VV_STRING,
    VV_PTR,
};

typedef struct VValue {
    enum ValTag vv_tag;
    union {
        int value_int;
        float value_float;
        bool value_bool;
        char *value_string;
        void *value_ptr;
    };
} VValue;

typedef struct Variable {
    Type vtype;
    VValue storage;
} Variable;

typedef struct Signal {
    SignalTag signal;
    struct Variable variable;
} Signal;

typedef struct Argument {
    ArgMode mode;
    Type argtype;
    union {
        char* name;
        VValue value;
    };
} Argument;

typedef struct Args {
    ArgMode mode;
    Argument* list;
    int amount;
} Args;

typedef struct CentralAccess {
    VValue* link;
    bool view;
} CentralAccess;

enum AnyEnum{
    UNDEFINED_TYPE,
    VALUE_TYPE,
    VART_TYPE,
    VARIABLE_TYPE,
    SPAR_TYPE,
    ARGS_TYPE,
    SIGNAL_TYPE,
    ACCESS_TYPE,
};

typedef struct EvalPass{
    enum AnyEnum tag;
    union {
        VValue as_value;
        Type as_vart;
        Variable as_variable;
        Argument as_spar;
        Args as_args;
        Signal as_signal;
        CentralAccess as_access;
    };
    enum NodeClass nclass;
} EvalPass;

//Don't Touch

typedef struct SymbolsTable{
    Hash* hash;
    struct SymbolsTable* parent;
    struct SymbolsTable* sibling;
    struct SymbolsTable* child;
} SymbolsTable;

typedef struct SymbolsManager{
    Arena* arena;
    Arena* global_arena;
    Hash* index_hash;
    SymbolsTable* root;
    SymbolsTable** stack;
    int attribute_capacity;
    bool production;
} SymbolsManager;


