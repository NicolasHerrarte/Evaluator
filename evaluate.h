#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "dynarray.h"
#include "symbols.h"

bool type_equals(Type a, Type b);
AnyType evaluate(SymbolsManager* manager, ASTNode* node);