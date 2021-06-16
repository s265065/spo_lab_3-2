%option noyywrap case-insensitive

%{
#include <json-c/json.h>

#include "y.tab.h"

static struct json_object * quoted_str() {
    char * str = malloc(sizeof(*str) * yyleng);

    int j = 0;
    for (int i = 1; i < yyleng - 1; ++i, ++j) {
        if (yytext[i] == '\\') {
            ++i;
        }

        str[j] = yytext[i];
    }

    str[j] = '\0';

    struct json_object * result = json_object_new_string(str);
    free(str);

    return result;
}

static int64_t int_literal() {
    char * str = malloc(sizeof(*str) * (yyleng + 1));

    memcpy(str, yytext, yyleng);
    str[yyleng] = '\0';

    int64_t val;
    sscanf(str, "%ld", &val);
    free(str);

    return val;
}

static uint64_t uint_literal() {
    char * str = malloc(sizeof(*str) * (yyleng + 1));

    memcpy(str, yytext, yyleng);
    str[yyleng] = '\0';

    uint64_t val;
    sscanf(str, "%lu", &val);
    free(str);

    return val;
}

static double num_literal() {
    char * str = malloc(sizeof(*str) * (yyleng + 1));

    memcpy(str, yytext, yyleng);
    str[yyleng] = '\0';

    double val;
    sscanf(str, "%lf", &val);
    free(str);

    return val;
}
%}

S [ \n\b\t\f\r]
W [a-zA-Z_]
D [0-9]

I {W}({W}|{D})*

%%

{S}     ;

create      return T_CREATE;
table       return T_TABLE;
int         return T_INT;
uint        return T_UINT;
num         return T_NUM;
str         return T_STR;
drop        return T_DROP;
insert      return T_INSERT;
values      return T_VALUES;
null        return T_NULL;
into        return T_INTO;
delete      return T_DELETE;
from        return T_FROM;
where       return T_WHERE;
select      return T_SELECT;
from        return T_FROM;
offset      return T_OFFSET;
limit       return T_LIMIT;
update      return T_UPDATE;
set         return T_SET;
join        return T_JOIN;
on          return T_ON;
\*          return T_ASTERISK;
"="         return T_EQ_OP;
"<>"        return T_NE_OP;
"!="        return T_NE_OP;
"<"         return T_LT_OP;
">"         return T_GT_OP;
"<="        return T_LE_OP;
">="        return T_GE_OP;
and         return T_AND_OP;
"&&"        return T_AND_OP;
or          return T_OR_OP;
"||"        return T_OR_OP;

{I}     yylval = json_object_new_string_len(yytext, yyleng); return T_IDENTIFIER;

-{D}+               yylval = json_object_new_int64(int_literal()); return T_INT_LITERAL;
{D}+                yylval = json_object_new_uint64(uint_literal()); return T_UINT_LITERAL;
{D}*\.{D}+          yylval = json_object_new_double(num_literal()); return T_NUM_LITERAL;
\'(\\.|[^'\\])*\'   yylval = quoted_str(); return T_STR_LITERAL;
\"(\\.|[^"\\])*\"   yylval = quoted_str(); return T_DBL_QUOTED;

.       return yytext[0];

%%

void scan_string(const char * str) {
    yy_switch_to_buffer(yy_scan_string(str));
}
