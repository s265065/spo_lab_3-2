%parse-param {struct json_object ** result} {char ** error}

%{
#include <string.h>

#include "../json_api.h"

int yylex(void);
void yyerror(struct json_object ** result, char ** error, const char * str);
%}

%define api.value.type {struct json_object *}

%token T_CREATE T_TABLE T_IDENTIFIER T_DBL_QUOTED T_INT T_UINT T_NUM T_STR T_DROP T_INSERT T_VALUES T_INTO
    T_INT_LITERAL T_UINT_LITERAL T_NUM_LITERAL T_STR_LITERAL T_NULL T_DELETE T_FROM T_WHERE T_JOIN T_ON
    T_EQ_OP T_NE_OP T_LT_OP T_GT_OP T_LE_OP T_GE_OP T_SELECT T_ASTERISK T_OFFSET T_LIMIT T_UPDATE T_SET

%left T_OR_OP
%left T_AND_OP

%%

command_line
    : command semi_non_req YYEOF    { *result = $1; }
    | YYEOF                         { *result = NULL; }
    ;

semi_non_req
    : /* empty */
    | ';'
    ;

command
    : create_table_command  { $$ = $1; }
    | drop_table_command    { $$ = $1; }
    | insert_command        { $$ = $1; }
    | delete_command        { $$ = $1; }
    | select_command        { $$ = $1; }
    | update_command        { $$ = $1; }
    ;

create_table_command
    : T_CREATE t_table_non_req name '(' columns_declaration_list ')'    {
        $$ = json_object_new_object();

        json_object_object_add($$, "action", json_object_new_int(0));
        json_object_object_add($$, "table", $3);
        json_object_object_add($$, "columns", $5);
    }
    ;

t_table_non_req
    : /* empty */
    | T_TABLE
    ;

name
    : T_IDENTIFIER  { $$ = $1; }
    | T_DBL_QUOTED  { $$ = $1; }
    ;

columns_declaration_list
    : /* empty */                   { $$ = NULL; }
    | columns_declaration_list_req  { $$ = $1; }
    ;

columns_declaration_list_req
    : column_declaration                                    { $$ = json_object_new_array(); json_object_array_add($$, $1); }
    | columns_declaration_list_req ',' column_declaration   { $$ = $1; json_object_array_add($$, $3); }
    ;

column_declaration
    : name type     {
        $$ = json_object_new_object();
        json_object_object_add($$, "name", $1);
        json_object_object_add($$, "type", $2);
    }
    ;

type
    : T_INT     { $$ = json_object_new_int(STORAGE_COLUMN_TYPE_INT); }
    | T_UINT    { $$ = json_object_new_int(STORAGE_COLUMN_TYPE_UINT); }
    | T_NUM     { $$ = json_object_new_int(STORAGE_COLUMN_TYPE_NUM); }
    | T_STR     { $$ = json_object_new_int(STORAGE_COLUMN_TYPE_STR); }
    ;

drop_table_command
    : T_DROP t_table_non_req name   {
        $$ = json_object_new_object();
        json_object_object_add($$, "action", json_object_new_int(1));
        json_object_object_add($$, "table", $3);
    }
    ;

insert_command
    : T_INSERT t_into_non_req name braced_names_list_non_req T_VALUES '(' values_list ')'   {
        $$ = json_object_new_object();

        json_object_object_add($$, "action", json_object_new_int(2));
        json_object_object_add($$, "table", $3);

        if ($4) {
            json_object_object_add($$, "columns", $4);
        }

        json_object_object_add($$, "values", $7);
    }
    ;

t_into_non_req
    : /* empty */
    | T_INTO
    ;

braced_names_list_non_req
    : /* empty */           { $$ = NULL; }
    | braced_names_list     { $$ = $1; }
    ;

braced_names_list
    : '(' names_list_req ')'    { $$ = $2; }
    ;

names_list_req
    : name                      { $$ = json_object_new_array(); json_object_array_add($$, $1); }
    | names_list_req ',' name   { $$ = $1; json_object_array_add($$, $3); }
    ;

values_list
    : /* empty */       { $$ = NULL; }
    | values_list_req   { $$ = $1; }
    ;

values_list_req
    : value                     { $$ = json_object_new_array(); json_object_array_add($$, $1); }
    | values_list_req ',' value { $$ = $1; json_object_array_add($$, $3); }
    ;

value
    : T_INT_LITERAL     { $$ = $1; }
    | T_UINT_LITERAL    { $$ = $1; }
    | T_NUM_LITERAL     { $$ = $1; }
    | T_STR_LITERAL     { $$ = $1; }
    | T_NULL            { $$ = NULL; }
    ;

delete_command
    : T_DELETE T_FROM name where_stmt_non_req   {
        $$ = json_object_new_object();

        json_object_object_add($$, "action", json_object_new_int(3));
        json_object_object_add($$, "table", $3);

        if ($4) {
            json_object_object_add($$, "where", $4);
        }
    }
    ;

where_stmt_non_req
    : /* empty */   { $$ = NULL; }
    | where_stmt    { $$ = $1; }
    ;

where_stmt
    : T_WHERE where_expr    { $$ = $2; }
    ;

where_expr
    : '(' where_expr ')'            { $$ = $2; }
    | name where_value_op value     {
        $$ = json_object_new_object();
        json_object_object_add($$, "op", $2);
        json_object_object_add($$, "column", $1);
        json_object_object_add($$, "value", $3);
    }
    | where_expr T_AND_OP where_expr    {
        $$ = json_object_new_object();
        json_object_object_add($$, "op", json_object_new_int(JSON_API_OPERATOR_AND));
        json_object_object_add($$, "left", $1);
        json_object_object_add($$, "right", $3);
    }
    | where_expr T_OR_OP where_expr     {
        $$ = json_object_new_object();
        json_object_object_add($$, "op", json_object_new_int(JSON_API_OPERATOR_OR));
        json_object_object_add($$, "left", $1);
        json_object_object_add($$, "right", $3);
    }
    ;

where_value_op
    : T_EQ_OP   { $$ = json_object_new_int(JSON_API_OPERATOR_EQ); }
    | T_NE_OP   { $$ = json_object_new_int(JSON_API_OPERATOR_NE); }
    | T_LT_OP   { $$ = json_object_new_int(JSON_API_OPERATOR_LT); }
    | T_GT_OP   { $$ = json_object_new_int(JSON_API_OPERATOR_GT); }
    | T_LE_OP   { $$ = json_object_new_int(JSON_API_OPERATOR_LE); }
    | T_GE_OP   { $$ = json_object_new_int(JSON_API_OPERATOR_GE); }
    ;

select_command
    : T_SELECT names_list_or_asterisk T_FROM name join_stmts where_stmt_non_req offset_stmt_non_req limit_stmt_non_req {
        $$ = json_object_new_object();

        json_object_object_add($$, "action", json_object_new_int(4));
        json_object_object_add($$, "table", $4);

        if ($2) {
            json_object_object_add($$, "columns", $2);
        }

        if ($5) {
            json_object_object_add($$, "joins", $5);
        }

        if ($6) {
            json_object_object_add($$, "where", $6);
        }

        if ($7) {
            json_object_object_add($$, "offset", $7);
        }

        if ($8) {
            json_object_object_add($$, "limit", $8);
        }
    }
    ;

names_list_or_asterisk
    : names_list_req    { $$ = $1; }
    | T_ASTERISK        { $$ = NULL; }
    ;

join_stmts
    : /* empty */           { $$ = NULL; }
    | join_stmts_non_null   { $$ = $1; }
    ;

join_stmts_non_null
    : join_stmt             { $$ = json_object_new_array(); json_object_array_add($$, $1); }
    | join_stmts join_stmt  { $$ = $1; json_object_array_add($$, $2); }
    ;

join_stmt
    : T_JOIN name T_ON name T_EQ_OP name    {
        $$ = json_object_new_object();

        json_object_object_add($$, "table", $2);
        json_object_object_add($$, "t_column", $4);
        json_object_object_add($$, "s_column", $6);
    }
    ;

offset_stmt_non_req
    : /* empty */   { $$ = NULL; }
    | offset_stmt   { $$ = $1; }
    ;

offset_stmt
    : T_OFFSET T_UINT_LITERAL   { $$ = $2; }
    ;

limit_stmt_non_req
    : /* empty */   { $$ = NULL; }
    | limit_stmt    { $$ = $1; }
    ;

limit_stmt
    : T_LIMIT T_UINT_LITERAL    { $$ = $2; }
    ;

update_command
    : T_UPDATE name T_SET update_values_list_req where_stmt_non_req {
        $$ = json_object_new_object();

        json_object_object_add($$, "action", json_object_new_int(5));
        json_object_object_add($$, "table", $2);

        int c = json_object_array_length($4);
        struct json_object * columns = json_object_new_array_ext(c);
        struct json_object * values = json_object_new_array_ext(c);
        for (int i = 0; i < c; ++i) {
            struct json_object * elem = json_object_array_get_idx($4, i);

            json_object_array_add(columns, json_object_array_get_idx(elem, 0));
            json_object_array_add(values, json_object_array_get_idx(elem, 1));
        }

        json_object_object_add($$, "columns", columns);
        json_object_object_add($$, "values", values);

        if ($5) {
            json_object_object_add($$, "where", $5);
        }
    }
    ;

update_values_list_req
    : update_value                              { $$ = json_object_new_array(); json_object_array_add($$, $1); }
    | update_values_list_req ',' update_value   { $$ = $1; json_object_array_add($$, $3); }
    ;

update_value
    : name T_EQ_OP value    { $$ = json_object_new_array(); json_object_array_add($$, $1); json_object_array_add($$, $3); }
    ;

%%

void yyerror(struct json_object ** result, char ** error, const char * str) {
    free(*error);

    *error = strdup(str);
}
