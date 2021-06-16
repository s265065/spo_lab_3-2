#pragma once

#include <json-c/json.h>

#include "storage.h"

// request object: { "action": <action: 0/1/2/3/4/5>, ... }
// response object: { ["success": ...,] ["error": <error message: string>,] }
//
// action "create table" (0):
// - request: {
//     "action": 0,
//     "table": <table name: string>,
//     "columns": [
//         {
//             "name": <column name: string>,
//             "type": <column type: 0/1/2/3>,
//         },
//     ],
// }
// - success response: {}
//
// action "drop table" (1):
// - request: {
//     "action": 1,
//     "table": <table name: string>,
// }
// - success response: {}
//
// action "insert" (2):
// - request: {
//     "action": 2,
//     "table": <table name: string>,
//     ["columns": <column names: string[]>,]
//     "values": <values list: <string/number/null>[]>,
// }
// - success response: {}
//
// action "delete" (3):
// - request: {
//     "action": 3,
//     "table": <table name: string>,
//     ["where": <where expression>,]
// }
// - success response: {
//     "amount": <amount of deleted rows: number>
// }
//
// action "select" (4):
// - request: {
//     "action": 4,
//     "table": <table name: string>,
//     ["columns": <columns list: string[]>,]
//     ["where": <where expression>,]
//     ["offset": <offset (default 0): number>,]
//     ["limit": <limit (from 0 to 1000, default 10): number>,]
//     ["joins": [
//         {
//             "table": <table name: string>,
//             "t_column": <column of joining table name: string>,
//             "s_column": <column of slice name: string>,
//         },
//     ],]
// }
// - success response: {
//     "columns": <columns list: string[]>,
//     "values": <values list: <string/number/null>[][]>
// }
//
// action "update" (5):
// - request: {
//     "action": 5,
//     "table": <table name: string>,
//     "columns": <column names: string[]>,
//     "values": <values list: <string/number/null>[]>,
//     ["where": <where expression>,]
// }
// - success response: {
//     "amount": <amount of updated rows: number>
// }
//
// where expression object: { "op": <operator: 0/1/2/3/4/5/6/7 - eq/ne/lt/gt/le/ge/and/or>, ... }
//
// where operators "eq"/"ne"/"lt"/"gt"/"le"/"ge" (0/1/2/3/4/5): {
//     "op": <0/1/2/3/4/5>
//     "column": <column name: string>,
//     "value": <value: <string/number/null>>,
// }
//
// where operators "and"/"or" (6/7): {
//     "op": <6/7>,
//     "left": <where expression>,
//     "right": <where expression>,
// }

enum json_api_action {
    JSON_API_TYPE_CREATE_TABLE = 0,
    JSON_API_TYPE_DROP_TABLE = 1,
    JSON_API_TYPE_INSERT = 2,
    JSON_API_TYPE_DELETE = 3,
    JSON_API_TYPE_SELECT = 4,
    JSON_API_TYPE_UPDATE = 5,
};

struct json_api_create_table_request {
    char * table_name;
    struct {
        unsigned int amount;
        struct {
            char * name;
            enum storage_column_type type;
        } * columns;
    } columns;
};

struct json_api_drop_table_request {
    char * table_name;
};

struct json_api_insert_request {
    char * table_name;
    struct {
        unsigned int amount;
        char ** columns;
    } columns;
    struct {
        unsigned int amount;
        struct storage_value ** values;
    } values;
};

enum json_api_operator {
    JSON_API_OPERATOR_EQ = 0,
    JSON_API_OPERATOR_NE = 1,
    JSON_API_OPERATOR_LT = 2,
    JSON_API_OPERATOR_GT = 3,
    JSON_API_OPERATOR_LE = 4,
    JSON_API_OPERATOR_GE = 5,
    JSON_API_OPERATOR_AND = 6,
    JSON_API_OPERATOR_OR = 7,
};

struct json_api_where {
    enum json_api_operator op;

    union {
        struct {
            char * column;
            struct storage_value * value;
        };

        struct {
            struct json_api_where * left;
            struct json_api_where * right;
        };
    };
};

struct json_api_delete_request {
    char * table_name;
    struct json_api_where * where;
};

struct json_api_select_request {
    char * table_name;
    struct {
        unsigned int amount;
        char ** columns;
    } columns;
    struct json_api_where * where;
    unsigned int offset;
    unsigned int limit;
    struct {
        unsigned int amount;
        struct {
            char * table;
            char * t_column;
            char * s_column;
        } * joins;
    } joins;
};

struct json_api_update_request {
    char * table_name;
    struct {
        unsigned int amount;
        char ** columns;
    } columns;
    struct {
        unsigned int amount;
        struct storage_value ** values;
    } values;
    struct json_api_where * where;
};

enum json_api_action json_api_get_action(struct json_object * object);

struct json_api_create_table_request json_api_to_create_table_request(struct json_object * object);
struct json_api_drop_table_request json_api_to_drop_table_request(struct json_object * object);
struct json_api_insert_request json_api_to_insert_request(struct json_object * object);
struct json_api_delete_request json_api_to_delete_request(struct json_object * object);
struct json_api_select_request json_api_to_select_request(struct json_object * object);
struct json_api_update_request json_api_to_update_request(struct json_object * object);

struct json_object * json_api_make_success(struct json_object * answer);
struct json_object * json_api_make_error(const char * msg);

struct json_object * json_api_from_value(struct storage_value * value);
