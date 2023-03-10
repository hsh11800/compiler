#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdbool.h>
#include"yacc.tab.h"
#include"type.h"

#define LIT_MAX 8092
A_LITERAL literal_table[LIT_MAX];
int globalAddress = 12;
int isSemanticError = 0;
int literal_no=0;
int literal_size=0;


int semantic_analysis();
void setLiteralAddress();
void sem_program(A_NODE* const);
int sem_declaration_list(A_ID* const, int);
int sem_declaration(A_ID* const, int);
int sem_A_TYPE(A_TYPE* const);
A_TYPE * sem_expression(A_NODE* const);
int sem_statement(A_NODE* const node, const int addr, const A_TYPE* ret, bool sw,bool brk,bool cnt);
int sem_statement_list(A_NODE* const node, int addr, const A_TYPE* ret, bool sw, bool brk, bool cnt);
void sem_arg_expression_list(A_NODE* const,const A_ID* const);
A_LITERAL getTypeAndValueOfExpression(const A_NODE* const node);
A_LITERAL checkTypeAndConvertLiteral(A_LITERAL, A_TYPE*, int);
A_NODE* convertCastingConversion(A_NODE* const, A_TYPE* const);
A_NODE* convertScalarToInteger(A_NODE* const node);
A_NODE* convertUsualAssignmentConversion(A_TYPE* const,A_NODE* const);
A_TYPE* convertUsualBinaryConversion(A_NODE* const);
A_NODE* convertUsualUnaryConversion(A_NODE* const);

bool isArrayType(const A_TYPE* const);
bool isVoidType(const A_TYPE* const type);
bool isIntType(const A_TYPE* const type);
bool isFloatType(const A_TYPE* const type);
bool isFuncType(const A_TYPE* const type);
bool isStructType(const A_TYPE* const type);
bool isPointerOrArrayType(const A_TYPE* const type);
bool isPointerType(const A_TYPE* const type);
bool isIntegralType(const A_TYPE* const type);
bool isScalarType(const A_TYPE* const type);
bool isArithmeticType(const A_TYPE* const type);
bool isConstZeroExp(const A_NODE* const node);
bool isCompatiblePointerType(const A_TYPE* const type1, const A_TYPE* const type2);
bool isCompatibleType(const A_TYPE* const type1, const A_TYPE* const type2);

bool isAllowableCastingConversion(const A_TYPE* const type1, const A_TYPE* const type2);
bool isAllowableAssignmentConversion(const A_TYPE* const type1, const A_TYPE* const type2);
bool isModifiableLvalue(const A_NODE* const);

void semanticError(int error, int line);
void semanticWarning(int error, int line);
int put_literal(A_LITERAL lit, int ll);
A_ID* getStructFieldIdentifier_null(const A_TYPE* const,const char* const);
A_ID* getPointerFieldIdentifier_null(const A_TYPE* const, const char* const);



//extern function
int syntex_analize(A_NODE* const);
void print_sem_ast(A_NODE *node);
A_NODE* makeNode(NODE_NAME, A_NODE* const, A_NODE* const, A_NODE* const);
A_TYPE* makeType(T_KIND);
A_TYPE* setTypeElementType(A_TYPE* const, A_TYPE* const);
extern A_TYPE* char_type, *int_type, *void_type, *string_type, *float_type;
extern A_NODE* root;


int semantic_analysis()
{
    sem_program(root);
    if(!isSemanticError)
    {
	printf("no semantic error\n");
    }
    //????????? ?????? 
    setLiteralAddress();


    if(!isSemanticError)
    {
	    print_sem_ast(root);
	    return 1;
    }
    else
    {
	    return -1;
    }
}
void setLiteralAddress()
{
	for(int i=1;i<=literal_no;i++)
	{
		literal_table[i].addr+=root->value;
	}
	root->value+=literal_size;
}
/// <summary>
/// ?????? ????????? ??????literal_size, ????????? ????????? ?????? ??????, ??????????????? ?????? 
/// ????????? ?????? ????????? ?????? ??? ????????? ????????? ????????? ?????? ?????????????????? ??????
/// ??? ????????? ???????????? 12 ???????????? ??????
/// </summary>
/// <param name="program">root node</param>
void sem_program(A_NODE* const program)
{
    switch (program->name)
    {
    case N_PROGRAM:
        sem_declaration_list((A_ID*)program->clink, 12);
        program->value = globalAddress;
    }
    
}
/// <summary>
/// declaration_list??? ????????? ??????
/// 1. level 0?????? N_PROGRAM??? ??????
/// 2. T_FUNC??? ?????????????????? ??????????????? feild
/// 3. T_ENUM or T_STRUCT??? ??????????????? feild
/// 4. N_STMT_COMPOUND??? ??????
/// </summary>
/// <param name="id"></param>
/// <param name="addr"></param>
/// <returns>??? declaration ????????? ???</returns>
int sem_declaration_list(A_ID* const id, int addr)
{
    int lastAddr = addr;
    A_ID* linkId_null = id;
    while (linkId_null)
    {
        lastAddr += sem_declaration(linkId_null, lastAddr);
        linkId_null = linkId_null->link;
    }

    return(lastAddr - addr);
}
/// <summary>
/// ID_VAR, ID_FEILD: ????????? ?????? ??????, ?????? ????????? id size ??????
/// ID_FUNC: ?????? ????????? local id size ??????, ???????????? ??????, ???????????? ??????, BODY ????????? ??????(sem_A_TYPE()?????? ??????)
/// ID_PARM: ????????? ?????? ??????, ?????? ????????? id size ??????, ???????????? ???????????? ??????(size=4)
///         char??? int??? ??????, function?????? function pointer??? ??????
/// 
/// sementic:
///     ID_VAR: ????????? ???????????? ????????? ????????? ?????? ?????? error, ????????? ?????? const ?????? ??????(dynamic array ??????)
///     ID_FEILD: void, func ?????? ??????, struct ?????? ??????(struct pointer??? ??????)
///     ID_PARM: ????????? ?????? ?????? ??????(????????? pointer??? ??????)
/// </summary>
/// <param name="id"></param>
/// <param name="addr">????????? address</param>
/// <returns>size of declaration
///ID_VAR, ID_ENUM_LITERAL, ID_PARM, ID_TYPE: size of id, static?????? 0
///ID_FUNC: 0
///
/// </returns>
int sem_declaration(A_ID* const id, int addr)
{
    A_TYPE* type = id->type;
    int typeSize = 0, declarationSize = 0;
    switch (id->kind)
    {
    case ID_VAR:
        typeSize = sem_A_TYPE(type);
        if (isArrayType(type) && type->expr == NULL)
        {
            semanticError(86,id->line);
        }
        if (typeSize % 4)
        {
            typeSize = (typeSize / 4 + 1) * 4;
        }
        if (id->specifier == S_STATIC||id->level==0)
        {
            id->level = 0;
            id->address = globalAddress;
            globalAddress += typeSize;
        }
        else
        {
            id->address = addr;
            declarationSize = typeSize;
        }
        break;
    case ID_FIELD:
        typeSize = sem_A_TYPE(type);
        if (isVoidType(type) || isFuncType(type)||isStructType(type))
        {
            semanticError(84,id->line);
        }
        if (typeSize % 4)
        {
            typeSize = (typeSize / 4 + 1) * 4;
        }
        id->address = addr;
        declarationSize = typeSize;
        break;
    case ID_FUNC:
        typeSize = sem_A_TYPE(type);
        break;
    case ID_PARM:
        typeSize = sem_A_TYPE(type);
        //usual unary conversion
        if (type == char_type)
        {
            id->type = int_type;
        }
        else if (isArrayType(type))
        {
            id->type->kind = T_POINTER;
            id->type->size = 4;

        }
        else if (isFuncType(type))
        {
            A_TYPE* newType=makeType(T_POINTER);
            newType->element_type = type;
            newType->size = 4;
            id->type = newType;
        }
        declarationSize = id->type->size;

        if (declarationSize % 4)
        {
            (declarationSize / 4 + 1) * 4;
        }
        id->address = addr;

        break;
    case ID_TYPE:
        typeSize = sem_A_TYPE(type);
        break;
    
    }

    return declarationSize;
}
/// <summary>
/// T_ENUM: size=4. field??? ????????? ???????????? ????????? 0?????? ???????????? ??????. ????????? ????????? ???????????? ???????????? ??????
/// T_STRUCT: size = field??? ?????? ?????? ????????? ???.
/// T_POINTER: size=4
/// T_ARRAY: size=sizeof(element_type)*#arrays
/// T_FUNC: local_val_size=?????? ??? ?????? ???????????? ????????? ???. parameter declaration list ??????.
///         function body ??????
/// 
/// semantic:
///     T_ENUM: feild??? init??? int_type ????????? ???
///     T_STRUCT:
///     T_POINTER:
///     T_ARRAY: element_type??? T_FUNC, void_type?????? ??????
///     T_FUNC: return type??? T_FUNC, T_ARRAY??? ??????
/// 
/// </summary>
/// <param name="type"></param>
/// <returns>type->size</returns>
int sem_A_TYPE(A_TYPE* const type)
{
    int typeSize = 0, i = 0,*p;
    A_ID* id_null;
    A_LITERAL lit;
    if (type->check)
    {
        return type->size;
    }
    type->check = 1;
    switch (type->kind)
    {
    case T_ENUM:
        id_null = type->field;
        
        while (id_null)
        {
            if (id_null->init)
            {
                lit = getTypeAndValueOfExpression(id_null->init);
                if (!isIntType(lit.type))
                {
                    semanticError(81,id_null->line);
                }
                i = lit.value.i;
            }
            p =malloc(sizeof(int));
	    *p=i;
	    id_null->init=(A_NODE*)p;
            ++i;

            id_null = id_null->link;
        }
        typeSize = 4;
        break;
    case T_POINTER:
        i = sem_A_TYPE(type->element_type);
        typeSize = 4;
        
        break;
    case T_ARRAY:
        if (type->expr)//ID_PARM??? T_ARRAY ?????? ??????
        {
            lit=getTypeAndValueOfExpression(type->expr);
            if (!isIntType(lit.type) || lit.value.i <= 0)
            {
                semanticError(82, type->line);
                type->expr = 0;
            }
            else
            {
                p=malloc(sizeof(int));
		*p=lit.value.i;
		type->expr=(A_NODE*)p;
            }

        }
        i = sem_A_TYPE(type->element_type);
	if(type->expr)
	{
        	typeSize = i * (*(int*)(type->expr));
	}
	else
	{
		typeSize=i;
	}
        if (isVoidType(type->element_type) || isFuncType(type->element_type))
        {
            semanticError(83,type->line);
        }
        break;
    case T_FUNC:
        id_null = type->field;
        if(type->element_type)
        {
                sem_A_TYPE(type->element_type);
        }
	if (id_null)
        {
            i = sem_declaration_list(id_null, 12);
        }
	i+=12;
        if (type->expr)
        {
            i = i + sem_statement(type->expr, i,type->element_type,false,false,false);
        }
        type->local_var_size = i;
        break;
    case T_STRUCT:
        id_null = type->field;
        if (id_null)
        {
            typeSize = sem_declaration_list(id_null, 0);
            
        }
        break;
    case T_VOID:
    case T_NULL:
        break;
    }//end of "switch (type->kind)"
    type->size = typeSize;
    return typeSize;
}
/// <summary>
/// expression node??? ??????????????? ????????? ?????? ????????? ???????????? ??? ?????????????????? ?????????.
/// node ????????? ??? ????????? node??? ????????? ??????????????? ????????? ????????? ??????????????? warning??? ??????
///     ????????? ???????????? ?????? error
/// ????????? literal??? ????????? ?????? literal table??? ????????? literal table pointer??? ????????????. 
/// ????????? ?????? ????????? ????????? ?????? ????????? ????????? ????????? ?????? ?????????.
///  
/// </summary>
/// <param name="node">expression node</param>
/// <returns>expression??? ?????? type table</returns>
A_TYPE* sem_expression(A_NODE* const node)
{
    A_TYPE* result = NULL,*type, * expr1, * expr2, * expr3;
    A_ID* id_null,*id;
    A_LITERAL lit;
    int lvalue = 0;
    int typeSize = 0;
    int* p;
    if (node->type)
    {
        return node->type;
    }
    switch (node->name)
    {
        ////////////////////////
        ///primary_expression///
        //////////////////////// 
    case  N_EXP_IDENT:
        id = (A_ID*)node->clink;
      
        switch (id->kind)//????????? ?????? ????????? ???????????? ????????? 4?????? 
        {
        case ID_VAR:
        case ID_PARM:
            result = id->type;
            if (!isArrayType(result))
                lvalue = 1;
            break;
        case ID_FUNC:
            result = id->type;
            break;
        case ID_ENUM_LITERAL:
            result = int_type;
            break;
        }
        break;
    case N_EXP_CHAR_CONST:
        result = char_type;
        
        break;
    case N_EXP_INT_CONST:
        result = int_type;
        break;
    case N_EXP_FLOAT_CONST:
	result=float_type;
        lit.type = result;
	lit.value.f = *((float*)node->clink);
	//lit.value.i=(int)(*((float*)node->clink));
	p=malloc(sizeof(int));
        *p=put_literal(lit,node->line);
	node->clink=(A_NODE*)p;
        break;
    case N_EXP_STRING_LITERAL:
        result = string_type;
        lit.type = string_type;
        lit.value.s = (char*)node->clink;
	p=malloc(sizeof(int));
        *p=put_literal(lit,node->line);
        node->clink=(A_NODE*)p;
        break;
        ////////////////////////
        ///postfix_expression///
        ////////////////////////
    case N_EXP_ARRAY:
        // expr1 [ expr2 ]
        expr1 = sem_expression(node->llink);
        expr2 = sem_expression(node->rlink);
        //type=convertUsualBinaryConversion(node);
        //expr1 = node->llink->type;
        //expr2 = node->rlink->type;
        if(isPointerOrArrayType(expr1))
        {
            result = expr1->element_type;
        }
        else if(expr1 && expr2)
        {
            semanticError(32,node->line);
        }
        if (!isIntegralType(expr2)&&expr1 && expr2)
        {

            semanticError(29,node->line);
        }
        if (!isArrayType(result))
        {
            lvalue = 1;
        }
        break;
    case N_EXP_STRUCT:
        //expr1 . id
        expr1 = sem_expression(node->llink);
        id_null = getStructFieldIdentifier_null(expr1, (char*)node->rlink);
        if (id_null)
        {
            result = id_null->type;
            if (node->llink->value && !isArrayType(result))
            {
                lvalue=1;
            }
        }
        else
        {
            semanticError(37,node->line);
        }
        node->rlink = (A_NODE*)id_null;
        break;
    case N_EXP_ARROW:
        //expr1 -> id
        expr1 = sem_expression(node->llink);
        id_null = getPointerFieldIdentifier_null(expr1, (char*)node->rlink);
        if (id_null)
        {
            result = id_null->type;
            if (node->llink->value && !isArrayType(result))
            {
                lvalue = 1;
            }
        }
        else
        {
            semanticError(37,node->line);
        }
        node->rlink = (A_NODE*)id_null;
        break;
    case N_EXP_FUNCTION_CALL:
        //expr1 ( arg_list_opt )
        expr1 = sem_expression(node->llink);
        node->llink = convertUsualUnaryConversion(node->llink);
        type = node->llink->type;
        if (isPointerType(type) && isFuncType(type->element_type))
        {
            sem_arg_expression_list(node->rlink, type->element_type->field);
	    result=type->element_type->element_type;
        }
        else
        {
            semanticError(21,node->line);
        }
        break;
    case N_EXP_POST_INC:
    case N_EXP_POST_DEC:
        result = sem_expression(node->clink);
        if (!isScalarType(result))
        {
            semanticError(27,node->line);
        }
        if (!isModifiableLvalue(node->clink))
        {
            semanticError(60,node->line);
        }
        break;
        //////////////////////
        ///unary expression///
        /// //////////////////
    case N_EXP_CAST:
        //( typename ) expr1
        result = (A_TYPE*)node->llink;//?????? ????????? typename??? ?????????
        typeSize = sem_A_TYPE(result);
        type = sem_expression(node->rlink);
        if (!isAllowableCastingConversion(result, type))
        {
            semanticError(58, node->line);
        }
        
        break;
    case N_EXP_SIZE_TYPE:
        type = (A_TYPE*)node->clink;
        typeSize = sem_A_TYPE(type);
        if ((isArrayType(type) && typeSize == 0) || isFuncType(type) || isVoidType(type))
        {
            semanticError(39,node->line);
        }
        else
        {
	    p=malloc(sizeof(p));
	    *p=typeSize;
            node->clink = (A_NODE*)p;
        }
        result = int_type;
        break;
    case N_EXP_SIZE_EXP:
        type = sem_expression(node->clink);
        if ((node->clink->name != N_EXP_IDENT || ((A_ID*)node->clink->clink)->kind != ID_PARM) &&
            ((isArrayType(type) && type->size == 0) || isFuncType(type)))
        {
            semanticError(39,node->line);
        }
        else
        {
 	    p=malloc(sizeof(int));
	    *p=type->size;

            node->clink = (A_NODE*)p;
        }
	result = int_type;
        break;
    case N_EXP_PLUS:
    case N_EXP_MINUS:
        type = sem_expression(node->clink);
        if (isArithmeticType(type))
        {
            node->clink = convertUsualUnaryConversion(node->clink);
            result = node->clink->type;
        }
        else
        {
            semanticError(13,node->line);
        }
        break;
    case N_EXP_NOT:
        type = sem_expression(node->clink);
        if (isScalarType(type))
        {
            node->clink = convertUsualUnaryConversion(node->clink);
            result = int_type;
        }
        else
        {
            semanticError(27,node->line);
        }

        break;
    case N_EXP_AMP:
        type = sem_expression(node->clink);
        if (node->clink->value || isFuncType(type))
        {
            result = setTypeElementType(makeType(T_POINTER), type);
            result->size = 4;
        }

        break;
    case N_EXP_STAR:
        type = sem_expression(node->clink);
        node->clink = convertUsualUnaryConversion(node->clink);
        if (isPointerType(type))
        {
            result = type->element_type;
            if (isStructType(result) || isScalarType(result))
            {
                lvalue = 1;
            }
        }
        else
        {
            semanticError(31,node->line);
        }
        break;
    case N_EXP_PRE_DEC:
    case N_EXP_PRE_INC:
        result = sem_expression(node->clink);
        if (!isScalarType(result))
        {
            semanticError(27,node->line);
        }
        if (!isModifiableLvalue(node->clink))
        {
            semanticError(60, node->line);
        }
        break;
        ///////////////////////////////
        ///multiplicative expression///
        ///////////////////////////////
    case N_EXP_MUL:
        //expr1 * expr2
    case N_EXP_DIV:
        //expr1 / expr2
        expr1 = sem_expression(node->llink);
        expr2 = sem_expression(node->rlink);
        if (isArithmeticType(expr1)&& isArithmeticType(expr2))
        {
            result = convertUsualBinaryConversion(node);
        }
        else
        {
            semanticError(28, node->line);
        }
        break;
    case N_EXP_MOD:
        // expr1 % expr2
        expr1 = sem_expression(node->llink);
        expr2 = sem_expression(node->rlink);
        if (!isIntegralType(expr1) || !isIntegralType(expr2))
        {
            semanticError(29, node->line);
        }
        else
        {
            result = convertUsualBinaryConversion(node);

        }
        result = int_type;
        break;
        /////////////////////////
        ///additive expression///
        /////////////////////////
    case N_EXP_ADD:
        //expr1 + expr2
        expr1 = sem_expression(node->llink);
        expr2 = sem_expression(node->rlink);
        if (isArithmeticType(expr1) && isArithmeticType(expr2))
        {
            result = convertUsualBinaryConversion(node);
        }
        else if (isPointerType(expr1) && isIntegralType(expr2))
        {
            result = expr1;
        }
        else if (isIntegralType(expr1) && isPointerType(expr2))
        {
            result = expr2;
        }
        else
        {
            semanticError(24, node->line);
        }
        break;
    case N_EXP_SUB:
        //expr1 - expr2
        expr1 = sem_expression(node->llink);
        expr2 = sem_expression(node->rlink);
        if (isArithmeticType(expr1) && isArithmeticType(expr2))
        {
            result = convertUsualBinaryConversion(node);
        }
        else if (isPointerType(expr1) && (isIntegralType(expr2) ))
        {
            result = expr1;
        }
        else if (isCompatiblePointerType(expr1, expr2))
        {//??????????????? ?????? int???
            result = int_type;
        }
        else
        {
            semanticError(24, node->line);
        }
        break;
        ///////////////////////////
        ///relational expression///
        ///////////////////////////
    case N_EXP_LSS:
    case N_EXP_GTR:
    case N_EXP_LEQ:
    case N_EXP_GEQ:
        expr1 = sem_expression(node->llink); 
        expr2 = sem_expression(node->rlink); 
        if (isArithmeticType(expr1) && isArithmeticType(expr2)) 
        {
            type = convertUsualBinaryConversion(node);
        }
        else if (!isCompatiblePointerType(expr1, expr2)) 
        {
            semanticError(40, node->line);
        }
        result = int_type;
        break;
        /////////////////////////
        ///equality expression///
        /////////////////////////
    case N_EXP_EQL:
        //expr1 == expr2
    case N_EXP_NEQ:
        //expr1 != expr2
        expr1 = sem_expression(node->llink);
        expr2 = sem_expression(node->rlink);
        if (isArithmeticType(expr1) && isArithmeticType(expr2))
        {
            type = convertUsualBinaryConversion(node);
        }
	else if(isPointerType(expr1)&&isIntegralType(expr2)&&!isConstZeroExp(node->rlink))
        {
		semanticWarning(14,node->line);
		type = convertUsualBinaryConversion(node);
        }
        else if (!isCompatiblePointerType(expr1, expr2) &&
            (!isPointerType(expr1) || !isConstZeroExp(node->rlink)) &&
            (!isConstZeroExp(node->llink) || !isPointerType(expr2)))
        {
            semanticError(40, node->line);
        }
        result = int_type;

        break;
        ////////////////////////////
        ///logical AND expression///
        ////////////////////////////
    case N_EXP_AND:
        //expr1 && expr2
    case N_EXP_OR:
        //expr1 || expr2
        expr1 = sem_expression(node->llink);
        expr2 = sem_expression(node->rlink);
        node->llink = convertUsualUnaryConversion(node->llink);
        node->rlink = convertUsualUnaryConversion(node->rlink);
        expr1 = sem_expression(node->llink);
        expr2 = sem_expression(node->rlink);
        if (!isScalarType(expr1) || !isScalarType(expr2))
        {
            semanticError(27, node->line);
        }
        result = int_type;
        break;
        ////////////////
        ///expression///
        ////////////////
    case N_EXP_ASSIGN:
        //expr1 = expr2
        expr1 = sem_expression(node->llink);
        expr2 = sem_expression(node->rlink);
        if (!isModifiableLvalue(node->llink))
        {
            semanticError(60, node->line);
        }
        result = expr1;
        if (isAllowableAssignmentConversion(result, expr2))
        {
            if (isArithmeticType(result) && isArithmeticType(expr2))
            {
                node->rlink = convertUsualAssignmentConversion(result, node->rlink);
            }
	    else if(isPointerType(result)&&!isConstZeroExp(node->rlink))
	    {	
		    if(!isCompatiblePointerType(result,node->rlink->type))
		     	semanticWarning(11,node->line);
		    node->rlink = convertUsualAssignmentConversion(result, node->rlink);
	    }
	}

        else
        {
                semanticError(58, node->line);
        }
        
        
        break;
    case N_FOR_EXP:
        //expr_opt1 ; expr_opt ; expr_opt
        if (node->llink)
        {
            type = sem_expression(node->llink);
        }
        if (node->clink)
        {
            type = sem_expression(node->clink);
            if (isScalarType(type))
            {
                node->clink = convertScalarToInteger(node->clink);
            }
            else
            {
                semanticError(28, node->line);
            }
        }
        if (node->rlink)
        {
            type = sem_expression(node->rlink);
        }
        break;
    }
    node->type = result;
    node->value = lvalue;
    return result;
}

/// <summary>
/// arg list??? ????????????. 
/// </summary>
/// <param name="root"></param>
/// <param name="id"></param>
void sem_arg_expression_list(A_NODE* const rootNode, const A_ID* const rootId)
{
    A_TYPE* type;
    A_NODE* node = rootNode;
    const A_ID* id_null = rootId;
    int argSize=0;
    switch (node->name)
    {
    case N_ARG_LIST:
        type = sem_expression(rootNode->llink);
        if (!id_null)
        {
            semanticError(34, node->line);
        }
        else
        {
            if (id_null->type)
            {
                type = sem_expression(node->llink);
                node->llink = convertUsualUnaryConversion(node->llink);
                if (isAllowableCastingConversion(id_null->type, node->llink->type))
                {
                    node->llink = convertCastingConversion(node->llink, id_null->type);
                }
                else
                {
                    semanticError(59,node->line);

                }
                sem_arg_expression_list(node->rlink, id_null->link);
            }
            else
            {
                type = sem_expression(node->llink);
                sem_arg_expression_list(node->rlink, id_null);
            }
	    argSize=node->llink->type->size+node->rlink->value;
        }
	break;
    case N_ARG_LIST_NIL:
        if (id_null && id_null->type)
        {
            semanticError(35, node->line);
        }
        break;
    }
    node->value=argSize;
    
}

/// <summary>
/// 
/// ???????????? syntex tree??? ??????????????? ??????
/// ????????? ????????? ????????????(COMPOUND) ??????????????? ????????? ?????? ??????
/// ????????? ?????? ???????????? ?????? ??????
/// </summary>
/// <param name="node">syntext tree node</param>
/// <param name="addr">start address</param>
/// <param name="ret">return type</param>
/// <param name="sw">switch statement(case, default)</param>
/// <param name="brk">break</param>
/// <param name="cnt">continue</param>
/// <returns>size of all local variable</returns>
int sem_statement(A_NODE* const node, int addr, const A_TYPE* ret, bool sw, bool brk, bool cnt)
{
    int localSize = 0;
    int size;
    int*p;
    A_LITERAL lit;
    A_TYPE* type;
    switch (node->name)
    {
        /////////////////////
        ///lable statement///
        /////////////////////
    case N_STMT_LABEL_CASE:
        //case constant_expression : statement
	if(!sw)
	{
		semanticError(71,node->line);
	}
        lit = getTypeAndValueOfExpression(node->llink);
        if (isIntegralType(lit.type))
        {
         
	    p = malloc(sizeof(int));
	    *p=lit.value.i; 
	    node->llink=(A_NODE*)p;
        }
        else
        {
            semanticError(51, node->line);
        }
        localSize = sem_statement(node->rlink, addr, ret, true, true, cnt);

        break;
    case N_STMT_LABEL_DEFAULT:
        //default : statement
	if(!sw)
	{
		semanticError(72,node->line);
	}
        localSize = sem_statement(node->clink, addr, ret, false, true, cnt);

        break;
        ////////////////////////
        ///compound statement///
        ////////////////////////
    case N_STMT_COMPOUND:
        //declaration_list_opt statement_list_opt
        if (node->llink)
        {
            localSize = sem_declaration_list((A_ID*)node->llink, addr);
        }
        localSize += sem_statement_list(node->rlink, addr + localSize, ret, sw, brk, cnt);
        break;
        //////////////////////////
        ///expression statement///
        //////////////////////////
    case N_STMT_EXPRESSION:
        sem_expression(node->clink);
        break;
    case N_STMT_EMPTY:

        break;
        /////////////////////////
        ///selection statement///
        /////////////////////////
    case N_STMT_IF:
        //if ( expr ) statement
        type = sem_expression(node->llink);
        if (!isScalarType(type))
        {
            semanticError(50, node->line);
        }
        else
        {
            node->llink = convertScalarToInteger(node->llink);
        }
        localSize = sem_statement(node->rlink, addr, ret, sw, brk, cnt);
        break;
    case N_STMT_IF_ELSE:
        //if ( expr ) stmt1 else stmt2
        type = sem_expression(node->llink);
        if (!isScalarType(type))
        {
            semanticError(50, node->line);
        }
        else
        {
            node->llink = convertScalarToInteger(node->llink);
        }
        localSize = sem_statement(node->clink, addr, ret, sw, brk, cnt);
        size = sem_statement(node->rlink, addr, ret, sw, brk, cnt);
        if (size > localSize)
        {
            localSize = size;
        }

        break;
    case N_STMT_SWITCH:
        //switch ( expr ) stmt
        type = sem_expression(node->llink);
        if (!isIntegralType(type))
        {
            semanticError(50, node->line);
        }
        localSize = sem_statement(node->rlink, addr, ret, true, true, cnt);

        break;
        /////////////////////////
        ///iteration statement///
        /////////////////////////
    case N_STMT_WHILE:
        //while ( expr ) stmt
        type = sem_expression(node->llink);
        if (isScalarType(type))
        {
            node->llink = convertScalarToInteger(node->llink);
        }
        else
        {
            semanticError(50, node->line);
        }
        localSize = sem_statement(node->rlink, addr, ret, false, true, true);
        break;
    case N_STMT_FOR:
        //for ( for_expr ) stmt
        sem_expression(node->llink);
        localSize = sem_statement(node->rlink, addr, ret, false, true, true);
        break;
    
    case N_STMT_DO:
        //do stmt while ( expr ) ;
        localSize = sem_statement(node->llink, addr, ret, false, true, true);
        type = sem_expression(node->rlink);
        if (isScalarType(type))
        {
            node->llink = convertScalarToInteger(node->llink);
        }
        else
        {
            semanticError(50, node->line);
        }
        break;
        ///////////////////
        ///jum statement///
        ///////////////////
    case N_STMT_CONTINUE:
        if (!cnt)
        {
            semanticError(74, node->line);
        }
        break;
    case N_STMT_RETURN:
        if (node->clink)
        {
            type = sem_expression(node->clink);
            if (isAllowableCastingConversion(ret, type))
            {
                node->clink = convertCastingConversion(node->clink, type);
            }
            else
            {
                semanticError(59, node->line);
            }
        }
        break;
    case N_STMT_BREAK:
        if (!brk)
        {
            semanticError(73, node->line);
        }
        break;

    }//end of "switch(node->name)
    node->value = localSize;
    return localSize;
}
/// <summary>
/// statement list ??????
/// </summary>
/// <param name="node">syntext tree node</param>
/// <param name="addr">start address</param>
/// <param name="ret">return type</param>
/// <param name="sw">switch statement(case, default)</param>
/// <param name="brk">break</param>
/// <param name="cnt">continue</param>
/// <returns></returns>
int sem_statement_list(A_NODE* const node, int addr, const A_TYPE* ret, bool sw, bool brk, bool cnt)
{
    int size=0,result=0;
    switch (node->name)
    {
    case N_STMT_LIST:
        result = sem_statement(node->llink, addr, ret, sw, brk, cnt);
        size = sem_statement_list(node->rlink, addr, ret, sw, brk, cnt);
        if (result < size)
        {
            result = size;
        }
        break;
    case N_STMT_LIST_NIL:
        result = 0;
        break;
    }
    node->value = result;
    return result;
}
/// <summary>
/// node??? const expression?????? ?????? ????????? ?????? ????????? literal??? ????????? ??????
/// </summary>
/// <param name="node"></param>
/// <returns></returns>
A_LITERAL getTypeAndValueOfExpression(const A_NODE* const node)
{
    A_TYPE* type;
    A_ID* id;
    A_LITERAL result, lit;
    result.type = NULL;
    switch (node->name)
    {
    case  N_EXP_IDENT:
        id = (A_ID*)node->clink;
        if (id->kind == ID_ENUM_LITERAL)
        {
            result.type = int_type;
            result.value.i = *((int*)id->init);
        }
        else
        {
            semanticError(19, node->line);
        }
        break;
    case N_EXP_CHAR_CONST:
        result.type = int_type;
        result.value.i = *((char*)node->clink);
        break;
    case N_EXP_INT_CONST:
        result.type = int_type;
        result.value.i = *((int*)node->clink);
        break;
    case N_EXP_FLOAT_CONST:
        result.type = float_type;
        result.value.f = *((float*)node->clink);

        break;
    case N_EXP_STRING_LITERAL:
    case N_EXP_ARRAY:
    case N_EXP_STRUCT:
    case N_EXP_ARROW:
    case N_EXP_FUNCTION_CALL:
    case N_EXP_POST_INC:
    case N_EXP_POST_DEC:
    case N_EXP_PRE_DEC:
    case N_EXP_PRE_INC:
    case N_EXP_AMP:
    case N_EXP_STAR:
    case N_EXP_ASSIGN:
        semanticError(18, node->line);
        break;
    case N_EXP_SIZE_TYPE:
        result.type = int_type;
        result.value.i = sem_A_TYPE((A_TYPE*)node->clink);
        break;
    case N_EXP_SIZE_EXP:
        type = sem_expression(node->clink);
	result.type=int_type;
	result.value.i=type->size;
        break;
    case N_EXP_MINUS:
        result = getTypeAndValueOfExpression(node->clink);
        if (isIntType(result.type))
        {
            result.value.i = -result.value.i;
        }
        else if (isFloatType(result.type))
        {
            result.value.f = -result.value.f;
        }
        else
        {
            semanticError(18, node->line);
        }
        break;
    case N_EXP_NOT:
	result=getTypeAndValueOfExpression(node->clink);
	if (result.type == int_type)
	{
	    result.value.i = !(result.value.i);
	}    
	else if (result.type == float_type)
	{
             result.value.i = !((int)result.value.f);
	}

	result.type=int_type;
	
	break;
    case N_EXP_CAST:
        result = getTypeAndValueOfExpression(node->rlink);
        result = checkTypeAndConvertLiteral(result, (A_TYPE*)node->llink, node->line);
        break;
        ///////////////////////////////
        ///multiplicative expression///
        ///////////////////////////////
    case N_EXP_MUL:
        result = getTypeAndValueOfExpression(node->llink);
        lit = getTypeAndValueOfExpression(node->rlink);
        if (result.type == int_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = result.value.i * lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.f = (float)result.value.i * lit.value.f;
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else if (result.type == float_type)
        {
            if (lit.type == int_type)
            {
                result.value.f = result.value.f * (float)lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.f = result.value.i * lit.value.f;
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else
        {
            semanticError(18, node->line);
        }
        break;
    case N_EXP_DIV:
        //expr1 / expr2
        result = getTypeAndValueOfExpression(node->llink);
        lit = getTypeAndValueOfExpression(node->rlink);
        if (result.type == int_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = result.value.i / lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.f = (float)result.value.i / lit.value.f;
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else if (result.type == float_type)
        {
            if (lit.type == int_type)
            {
                result.value.f = result.value.f / (float)lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.f = result.value.i / lit.value.f;
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else
        {
            semanticError(18, node->line);
        }
        break;
    case N_EXP_MOD:
        // expr1 % expr2
        result = getTypeAndValueOfExpression(node->llink);
        lit = getTypeAndValueOfExpression(node->rlink);
        if ((result.type == int_type) && (lit.type == int_type))
        {
            result.value.i = result.value.i % lit.value.i;
        }
        else
        {
            semanticError(18, node->line);
        }
        break;
        /////////////////////////
        ///additive expression///
        /////////////////////////
    case N_EXP_ADD:
        //expr1 + expr2
        result = getTypeAndValueOfExpression(node->llink);
        lit = getTypeAndValueOfExpression(node->rlink);
        if (result.type == int_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = result.value.i + lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.f = (float)result.value.i + lit.value.f;
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else if (result.type == float_type)
        {
            if (lit.type == int_type)
            {
                result.value.f = result.value.f + (float)lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.f = result.value.i + lit.value.f;
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else
        {
            semanticError(18, node->line);
        }
        break;
    case N_EXP_SUB:
        //expr1 - expr2
        result = getTypeAndValueOfExpression(node->llink);
        lit = getTypeAndValueOfExpression(node->rlink);
        if (result.type == int_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = result.value.i - lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.f = (float)result.value.i - lit.value.f;
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else if (result.type == float_type)
        {
            if (lit.type == int_type)
            {
                result.value.f = result.value.f - (float)lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.f = result.value.i - lit.value.f;
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else
        {
            semanticError(18, node->line);
        }
        break;
        ///////////////////////////
        ///relational expression///
        ///////////////////////////
    case N_EXP_LSS:
        result = getTypeAndValueOfExpression(node->llink);
        lit = getTypeAndValueOfExpression(node->rlink);
        result.type = int_type;
        if (result.type == int_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = result.value.i < lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)((float)result.value.i < lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else if (result.type == float_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = (int)(result.value.f < (float)lit.value.i);
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)(result.value.i < lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else
        {
            semanticError(18, node->line);
        }
        break;
    case N_EXP_GTR:
        result = getTypeAndValueOfExpression(node->llink);
        lit = getTypeAndValueOfExpression(node->rlink);
        if (result.type == int_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = result.value.i > lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)((float)result.value.i > lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else if (result.type == float_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = (result.value.f > (float)lit.value.i);
            }
            else if (lit.type == float_type)
            {
                result.value.i = (result.value.f > lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else
        {
            semanticError(18, node->line);
        }
	result.type = int_type;
        break;
    case N_EXP_LEQ:
        result = getTypeAndValueOfExpression(node->llink);
        lit = getTypeAndValueOfExpression(node->rlink);
        result.type = int_type;
        if (result.type == int_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = result.value.i <= lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)((float)result.value.i <= lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else if (result.type == float_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = (int)(result.value.f <= (float)lit.value.i);
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)(result.value.i <= lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else
        {
            semanticError(18, node->line);
        }
        break;
    case N_EXP_GEQ:
        result = getTypeAndValueOfExpression(node->llink);
        lit = getTypeAndValueOfExpression(node->rlink);
        result.type = int_type;
        if (result.type == int_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = result.value.i >= lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)((float)result.value.i >= lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else if (result.type == float_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = (int)(result.value.f >= (float)lit.value.i);
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)(result.value.i >= lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else
        {
            semanticError(18, node->line);
        }
        break;
        /////////////////////////
        ///equality expression///
        /////////////////////////
    case N_EXP_EQL:
        //expr1 == expr2
        result = getTypeAndValueOfExpression(node->llink);
        lit = getTypeAndValueOfExpression(node->rlink);
        result.type = int_type;
        if (result.type == int_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = result.value.i == lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)((float)result.value.i == lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else if (result.type == float_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = (int)(result.value.f == (float)lit.value.i);
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)(result.value.i == lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else
        {
            semanticError(18, node->line);
        }
        break;
    case N_EXP_NEQ:
        //expr1 != expr2
        result = getTypeAndValueOfExpression(node->llink);
        lit = getTypeAndValueOfExpression(node->rlink);
        result.type = int_type;
        if (result.type == int_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = result.value.i != lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)((float)result.value.i != lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else if (result.type == float_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = (int)(result.value.f != (float)lit.value.i);
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)(result.value.i != lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else
        {
            semanticError(18, node->line);
        }
        break;
        ////////////////////////////
        ///logical AND expression///
        ////////////////////////////
    case N_EXP_AND:
        //expr1 && expr2
        result = getTypeAndValueOfExpression(node->llink);
        lit = getTypeAndValueOfExpression(node->rlink);
        result.type = int_type;
        if (result.type == int_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = result.value.i && lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)((float)result.value.i && lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else if (result.type == float_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = (int)(result.value.f && (float)lit.value.i);
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)(result.value.i && lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else
        {
            semanticError(18, node->line);
        }
    case N_EXP_OR:
        //expr1 || expr2
        result = getTypeAndValueOfExpression(node->llink);
        lit = getTypeAndValueOfExpression(node->rlink);
        result.type = int_type;
        if (result.type == int_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = result.value.i || lit.value.i;
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)((float)result.value.i || lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else if (result.type == float_type)
        {
            if (lit.type == int_type)
            {
                result.value.i = (int)(result.value.f || (float)lit.value.i);
            }
            else if (lit.type == float_type)
            {
                result.value.i = (int)(result.value.i || lit.value.f);
            }
            else
            {
                semanticError(18, node->line);
            }
        }
        else
        {
            semanticError(18, node->line);
        }
        break;
    default:
        semanticError(90, node->name);
    }//end of "switch(node->name)"
    return result;
}

int put_literal(A_LITERAL lit, int ll)
{
    float ff;
    if (literal_no >= LIT_MAX)
    {
        semanticError(93, ll);
    }
    else 
    { 
        literal_no++; 
    }
    literal_table[literal_no] = lit;
    literal_table[literal_no].addr = literal_size;
    if (lit.type->kind == T_ENUM)
    {
        literal_size += 4;
    }
    else if (lit.type == string_type)
    {
        literal_size += strlen(lit.value.s) + 1;
    }
    if (literal_size % 4) 
    { 
        literal_size = literal_size / 4 * 4 + 4; 
    }
    //return(literal_no-1);
    return (literal_no);
}


/// <summary>
/// node??? ????????? type?????? casting
/// warning: void type ?????? void pointer type?????? casting ????????? ????????? 
/// </summary>
/// <param name="node">casting ?????? node</param>
/// <param name="type">cast ?????? ??????</param>
/// <returns>casting??? node</returns>
A_NODE* convertCastingConversion(A_NODE* const node, A_TYPE* const type)
{
    A_TYPE* nodeType = node->type;
    A_NODE* result = node;
    if (!isCompatibleType(nodeType, type))
    {
	if(type->element_type&&!isVoidType(type->element_type))
	{
		semanticWarning(12, node->line);
	}
	else if(!type->element_type&&!isVoidType(type))
	{
		semanticWarning(12, node->line);
	}
        result = makeNode(N_EXP_CAST, (A_NODE*)type, NULL, node);
        result->type = type;
	result->type->prt = true;
    }
    return result;
}
/// <summary>
/// node??? ????????? int??? ?????????. float??? casting?????? warning
/// </summary>
/// <param name="node"></param>
/// <returns></returns>
A_NODE* convertScalarToInteger(A_NODE* const node)
{
    A_NODE* result = node;
    if (isFloatType(node->type))
    {
        semanticWarning(16, node->line);
        result = makeNode(N_EXP_CAST, (A_NODE*)int_type, NULL, node);
	result->type=int_type;
	result->type->prt = true;
    }
    result->type = int_type;
    return result;
}
/// <summary>
/// "type = node" expression?????? node??? type??? type?????? casting 
/// </summary>
/// <param name="type">castiong ?????? type</param>
/// <param name="node">casting ?????? node</param>
/// <returns>casting ????????? node</returns>
A_NODE* convertUsualAssignmentConversion(A_TYPE* const type, A_NODE* const node)
{
    A_NODE* result = node;
    if(node->type->kind==T_FUNC)
    {
         A_TYPE* newType = setTypeElementType(makeType(T_POINTER), type);
         newType->size = 4;
         result = makeNode(N_EXP_CAST, (A_NODE*)newType, NULL, node);
         result->type = newType;
	 result->type->prt = true;
    }
    else if (!isCompatibleType(type, node->type)&&(!isIntegralType(type)&&!isIntegralType(node->type)))
    {
        semanticWarning(11, node->line);
        result = makeNode(N_EXP_CAST, (A_NODE*)type, NULL, node);
    	result->type=type;
	result->type->prt = true;
    }

    result->type = type;
    return result;
}
/// <summary>
/// ?????? ?????? expression??? node??? ??? ?????? ????????? ?????????.
/// </summary>
/// <param name="node"></param>
/// <returns>????????? type</returns>
A_TYPE* convertUsualBinaryConversion(A_NODE* const node)
{
    A_TYPE* type1 = node->llink->type;
    A_TYPE* type2 = node->rlink->type;
    A_TYPE* result;
    if (isFloatType(type1) && !isFloatType(type2))
    {
        semanticWarning(14, node->line);
        node->rlink = makeNode(N_EXP_CAST, (A_NODE*)type1, NULL, node->rlink);
        node->rlink->type = type1;
	node->rlink->type->prt = true;
        result = type1;
    }
    else if (!isFloatType(type1) && isFloatType(type2))
    {
        semanticWarning(14, node->line);
        node->llink = makeNode(N_EXP_CAST, (A_NODE*)type2, NULL, node->llink);
        node->llink->type = type2;
	node->llink->type->prt = true;
        result = type2;
    }
    else if (type1 == type2)
    {
        result = type1;
    }
    else
    {
        result = int_type;
    }
    return result;
}
/// <summary>
/// 
/// </summary>
/// <param name="node"></param>
/// <returns></returns>
A_NODE* convertUsualUnaryConversion(A_NODE* const node)
{
    A_TYPE* type = node->type;
    A_NODE* result=node;

    if (type == char_type)
    {
        result = makeNode(N_EXP_CAST, (A_NODE*)int_type, NULL, node);
        result->type = int_type;
	result->type->prt = true;
    }
    else if (isArrayType(type))
    {
        A_TYPE* newType = setTypeElementType(makeType(T_POINTER), type->element_type);
        newType->size = 4;
        result = makeNode(N_EXP_CAST, (A_NODE*)newType, NULL, node);
        result->type = newType;
	result->type->prt = true;

    }
    else if (isFuncType(type))
    {
        A_TYPE* newType = setTypeElementType(makeType(T_POINTER), type);
        newType->size = 4;
        result = makeNode(N_EXP_CAST, (A_NODE*)newType, NULL, node);
        result->type = newType;
    	result->type->prt = true;
    }
    return result;
}




bool isArrayType(const A_TYPE* const type)
{
    return type && type->kind == T_ARRAY;
}
bool isVoidType(const A_TYPE* const type)
{
    return type && type == void_type;
}
bool isIntType(const A_TYPE* const type)
{
    return type && type == int_type;
}

bool isFloatType(const A_TYPE* const type)
{
    return type && type == float_type;
}
bool isFuncType(const A_TYPE* const type)
{
    return type && type->kind == T_FUNC;
}
bool isStructType(const A_TYPE* const type)
{
    return type && type->kind == T_STRUCT;
}
bool isPointerOrArrayType(const A_TYPE* const type)
{
    return type && (type->kind == T_POINTER)||(type->kind == T_ARRAY);
}
bool isPointerType(const A_TYPE* const type)
{
    return type && (type->kind == T_POINTER);
}

bool isIntegralType(const A_TYPE* const type)
{
    return (type && (type->kind == T_ENUM) && (type != float_type));
}
bool isArithmeticType(const A_TYPE* const type)
{
    return type &&(type->kind == T_ENUM);
}
bool isScalarType(const A_TYPE* const type)
{
    return type && ((type->kind == T_ENUM)||(type->kind == T_POINTER));
}

bool isConstZeroExp(const A_NODE* const node)
{
    if (node && node->name == N_EXP_INT_CONST && (*(int*)(node->clink) == 0))
    {
        return true;
    }
    else
    {
        return false;
    }
}
/// <summary>
/// pointer type??? ??????????????? ????????????.
/// pointer??? element type??? ??????????????? ???????????????.
/// </summary>
/// <param name="type1"></param>
/// <param name="type2"></param>
/// <returns></returns>
bool isCompatiblePointerType(const A_TYPE* const type1, const A_TYPE* const type2)
{
    if (isPointerType(type1) && isPointerType(type2))
    {
        return isCompatibleType(type1->element_type, type2->element_type);
    }
    else
    {
        return false;
    }
}
/// <summary>
/// ??? ????????? ??????????????? ????????????.
/// integral type??? ?????? ???????????????.
/// arithmetic type??? ????????? ??????????????? ??????.
/// struct type??? ????????? ??????????????? ??????.
/// array type??? element type??? ?????????????????? ??????.
/// function type??? ??? ????????? ??????????????? ??????????????? 
///     ??????????????? ?????? ????????? ????????? ??????????????? ???????????????.
/// </summary>
/// <param name="type1"></param>
/// <param name="type2"></param>
/// <returns></returns>
bool isCompatibleType(const A_TYPE* const type1, const A_TYPE* const type2)
{
    if (isArithmeticType(type1) && isArithmeticType(type2))
    {
	    /*if(isIntegralType(type1)&&isIntegralType(type2))
	    {
		    return true;
	    }
            else*/
	    {
	    	return type1 == type2;
	    }
    }
    else if (isStructType(type1) && isStructType(type2))
    {
        return type1 == type2;
    }
    else if (isArrayType(type1) && isArrayType(type2))
    {
        return isCompatibleType(type1->element_type, type2->element_type);
    }
    else if (isFuncType(type1) && isFuncType(type2))
    {
        A_ID* parm1_null = type1->field;
        A_ID* parm2_null = type2->field;
        while (parm1_null)
        {
            if (!isCompatibleType(parm1_null->type, parm2_null->type))
            {
                return false;
            }
            parm1_null = parm1_null->link;
            parm2_null = parm2_null->link;
        }
        if (parm2_null)
        {
            return false;
        }
        if (isCompatibleType(type1->element_type, type2->element_type))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else if(isPointerType(type1)&&isPointerType(type2))
    {
	return isCompatiblePointerType(type1, type2);
    }
}

/// <summary>
/// type2??? type1?????? casting ???????????? ??????
/// </summary>
/// <param name="type1">casting ?????? ??????</param>
/// <param name="type2">casting ?????? ??????</param>
/// <returns>true/false</returns>
bool isAllowableCastingConversion(const A_TYPE* const type1, const A_TYPE* const type2)
{
    if(type1==type2)
    {
	    return true;
    }
    else if (isIntType(type1) && isScalarType(type2))
    {
        return true;
    }
    else if (isFloatType(type1)&&isArithmeticType(type2))
    {
        return true;
    }
    else if (isPointerType(type1) && isIntegralType(type2))
    {
        return true;
    }
    else if(isPointerType(type1)&&isPointerType(type2))
    {
	
	return true;
    }
    else if (isVoidType(type1))
    {
        return true;
    }
    else
    {
        return false;
    }
}
/// <summary>
/// type2??? type1??? ?????? ???????????? ??????
/// </summary>
/// <param name="type1"></param>
/// <param name="type2"></param>
/// <returns></returns>
bool isAllowableAssignmentConversion(const A_TYPE* const type1, const A_TYPE* const type2)
{
    if (isArithmeticType(type1) && isArithmeticType(type2))
    {
        return true;
    }
    else if (isPointerType(type1) && (isCompatiblePointerType(type1, type2) || isIntegralType(type2)))
    {
        return true;
    }
    else if(isPointerType(type1)&&isPointerType(type2))
    {
	return  isAllowableCastingConversion(type1,type2);
    }
    else if (isStructType(type1) && isCompatibleType(type1, type2))
    {
        return true;
    }
    else
    {
        return false;
    }
}


void semanticError(int error, int line)
{
    ++isSemanticError;
    printf("semantic error line %d: ",line);
    switch (error)
    {
    case 13:
        printf("arithmetic type expression required in unary operation");
        break;
    case 18:
        printf("illegal constant expression ");
        break;
    case 19:
        printf("illegal identifier in constant expression");
        break;
    case 21:
        printf("illegal type in function call expression");
        break;
    case 24:
        printf("incompatible type in additive expression");
        break;
    case 27:

        printf("scalar type expression required in unary or logial-or operation");
        break;
    case 28:
        printf("arithmetic type expression required in binary operation");
        
       
        break;
    case 29:
        printf("integral type expression required in array subscript or binary operation"); 

        break;
    case 31:
        printf("pointer type expression required in pointer operation");
        break;
    case 32:
        printf("array type required in array expression");
        break;
    case 34:
        printf("too many arguments in function call");
        break;
    case 35:
        printf("too few arguments in function call");
        break;
    case 37:
        printf("illegal struct field identifier in struct reference expression");
        break;
    case 38:
        printf(" illegal kind of identifier in expression");
        break;
    case 39:
        printf("illegal type size in sizeof operation");
        break;
    case 40:
        printf("illegal expression type in relational exprssion");
        break;
    case 41:
        printf("incompatible type in literal");
        break;
    case 49:
        printf("scalar type expr required in middle of for-exp");
        break;
    case 50:
        printf("integral type expression required in statement");
        break;
    case 51:
        printf("illegal expression type in case label");
        break;
    case 57:
        printf("not permitted type conversion in return expression");
        break;
    case 58:
        printf("not permitted type casting in expression");
        break;
    case 59:
        printf("not permitted type conversion in argument");
        break;
    case 60:
        printf("expression is not an lvalue");
        break;
    case 71:
        printf("case label not within a switch statement");
        break;
    case 72:
        printf("default label not within a switch statement");
        break;
    case 73:
        printf("break statement not within loop or switch statement");
        break;
    case 74:
        printf("continue statement not within a loop statement");
        break;
    case 80:
        printf("undefined type");
        break;
    case 81:
        printf("integer type expression required in enumerator");
        break;
    case 82:
        printf("illegal array size or type");
        break;
    case 83:
        printf("illegal element type of array declaration");
        break;
    case 84:
        printf("illegal type in struct field");
        break;
    case 85:
        printf("invalid function return type");
        break;
    case 86:
        printf("illegal array size or empty array");
        break;
    case 89:
        printf("unknown identifier kind");
        break;
    case 90:
        printf("fatal compiler error in parse result");
            
        break;
    case 93:
        printf("too many literals in source program");
        break;
    default:
        printf("unknown");
        break;
    }//end of "switch(error)"
    printf("\n");

}

void semanticWarning(int warning,int line)
{
    printf("warning line %d: ", line);
    switch (warning)
    {
    case 11:
        printf("incompatible types in assignement expression");
        break;
    case 12:
        printf("incompatible types in argument or return expression");
        break;
    case 14:
        printf("incompatible types in binary expression");
	break;
    case 16:
        printf("integer type expression is required");
        break;
    default:
        printf("unknown");
        break;

    }//end of "switch(warning)"
    printf("\n");
}
/// <summary>
/// literal??? ????????? ??????
/// </summary>
/// <param name="lit">?????? ????????? literal</param>
/// <param name="type">?????? ?????? ??????</param>
/// <param name="line">error??? line</param>
/// <returns>????????? literal</returns>
A_LITERAL checkTypeAndConvertLiteral(A_LITERAL lit, A_TYPE*type, int line)
{
    if (lit.type == int_type)
    {
        if (type == int_type);
        else if (type == float_type)
        {
            lit.type = float_type;
            lit.value.f = (float)lit.value.i ;
        }
        else if (type == char_type)
        {
            lit.type =char_type;
            lit.value.c = (char)lit.value.i;
            
        }
        else
        {
            semanticError(41, line);
        }
    }
    else if (lit.type == float_type)
    {
        if (type == int_type)
        {
            lit.type = int_type;
            lit.value.i = (int)lit.value.f;
        }
        else if (type == float_type);
        else
        {
            semanticError(41, line);
        }
    }
    else if (lit.type == char_type)
    {
        if (type == int_type)
        {
            lit.type = int_type;
            lit.value.i = (int)lit.value.c;
        }
        else if (type == char_type);
        else
        {
            semanticError(41, line);
        }
    }
    else
    {
        semanticError(41, line);
    }
}

/// <summary>
/// struct "type"??? feild?????? s??? ?????????
/// </summary>
/// <param name="type">struct type table</param>
/// <param name="s">?????? id ??????</param>
/// <returns>?????? id table or null</returns>
A_ID* getStructFieldIdentifier_null(const A_TYPE* const type, const char* const s)
{
    A_ID* id_null = NULL;
    if (isStructType(type))
    {
        id_null = type->field;
        while (id_null)
        {
            if (strcmp(id_null->name, s) == 0)
            {
                return id_null;
            }
            id_null = id_null->link;
        }
    }
    return id_null;
}

/// <summary>
/// struct??? ???????????? pointer??? "type"??? struct feild?????? "s"??? ?????????.
/// </summary>
/// <param name="type"></param>
/// <param name="s"></param>
/// <returns></returns>
A_ID* getPointerFieldIdentifier_null(const A_TYPE* const type, const char* const s)
{
    A_ID* id_null = NULL;
    if (isPointerType(type) && isStructType(type->element_type))
    {
        id_null = type->element_type->field;
        while (id_null)
        {
            if (strcmp(id_null->name, s) == 0)
            {
                return id_null;
            }
            id_null = id_null->link;
        }
    }
    return id_null;
}

/// <summary>
/// "node"??? ?????? ????????? lvalue?????? ????????????. lvalue??? ?????? node????????? void?????? ????????? function ???????????? ???????????????.
/// </summary>
/// <param name="node"></param>
/// <returns></returns>
bool isModifiableLvalue(const A_NODE* const node)
{
    if (!(node->value) || isVoidType(node->type))
    {
        return false;
    }
    else if (isFuncType(node->type))
    {//????????? ?????? ???????????? ???????????? ????????? ?????? ??????
        return false;
    }
    else
    {
        return true;
    }
}
