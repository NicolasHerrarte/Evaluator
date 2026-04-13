#include "hash.h"
#include "allocator.h"

#define MAX_DIMS 4
#define STORAGE_NAME_CAPACITY 10

enum NodeClass{
    STATLIST,
    IFNODE,
    WHILENODE,
    FORNODE,
    VARDECL,
    FUNCDECL,
    VARASSIGN,
    ARRTYPE,
    BREAK,
    CONTINUE,
    RETURN,
    GOTO,
    ARGUMENTS,
    PARAMETERS,
    UNIPARAM,
    EXPRESSION,
    SUM,
    SUBTRACT,
    MULT,
    DIVISION,
    ARRACCESS,
    FUNCCALL,
    PROPERTYACCESS
};

typedef enum {
    VAR_TYPE_INT, 
    VAR_TYPE_FLOAT,
    VAR_TYPE_BOOL, 
    VAR_TYPE_STRING,
} TypeTag;

typedef enum {
    KIND_PRIMITIVE,
    KIND_ARRAY,
} TypeKind;

typedef struct Type {
    TypeKind kind;
    int size;
    union {
        TypeTag primitive_tag;
        struct {
            TypeTag elem_type;
            int sizes[MAX_DIMS];
            int ndims;
        } ArrType;
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
        int    value_int;
        double value_float;
        bool   value_bool;
        char  *value_string;
        void  *value_ptr;
    };
} VValue;

typedef struct Variable {
    Type vtype;
    VValue storage;
} Variable;

enum AnyEnum{
    UNDEFINED_TYPE,
    VALUE_TYPE,
    VART_TYPE,
    VARIABLE_TYPE,
};

typedef struct AnyType{
    enum AnyEnum tag;
    union {
        VValue as_value;
        Type as_vart;
        Variable as_variable;
    };
} AnyType;

//Don't Touch

typedef struct SymbolsTable{
    Hash* hash;
    struct SymbolsTable* parent;
    struct SymbolsTable* sibling;
    struct SymbolsTable* child;
    Arena* local_arena;
} SymbolsTable;

typedef struct SymbolsManager{
    Arena* arena;
    Hash* index_hash;
    SymbolsTable* root;
    SymbolsTable** stack;
    int attribute_capacity;
} SymbolsManager;


