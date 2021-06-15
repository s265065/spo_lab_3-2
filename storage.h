#pragma once

#include <stdint.h>

// Pointer structure:
// - Offset from start of file: <uint64_t>
//
// String structure:
// - Length of string: <uint16_t>
// - Value: <int8_t[]>
//
// Storage file structure:
// - Storage file header
// - Table headers and rows
//
// Storage file header structure:
// - Signature: 0xdeadbabe
// - First table: <pointer>
//
// Table header structure:
// - Next table: <pointer>
// - First row: <pointer>
// - Table name: <string>
// - Amount of table columns: <uint16_t>
// - Table columns
//
// Table column structure:
// - Column name: <string>
// - Column type: <uint8_t>
//   - 0 - <int64_t>
//   - 1 - <uint64_t>
//   - 2 - <double>
//   - 3 - <string>
//
// Table row structure:
// - Next row: <pointer>
// - Cells: <pointer[]>
//
// Cell structure:
// - Value: value of type that noticed in table header column

enum storage_column_type {
    STORAGE_COLUMN_TYPE_INT = 0,
    STORAGE_COLUMN_TYPE_UINT = 1,
    STORAGE_COLUMN_TYPE_NUM = 2,
    STORAGE_COLUMN_TYPE_STR = 3,
};

struct storage {
    int fd;
    uint64_t first_table;
};

struct storage_column {
    char * name;
    enum storage_column_type type;
};

struct storage_table {
    struct storage * storage;

    uint64_t position;
    uint64_t next;

    uint64_t first_row;
    char * name;

    struct {
        uint16_t amount;
        struct storage_column * columns;
    } columns;
};

struct storage_row {
    struct storage_table * table;

    uint64_t position;
    uint64_t next;
};

struct storage_value {
    enum storage_column_type type;

    union {
        int64_t _int;
        uint64_t uint;
        double num;
        char * str;
    } value;
};

// storage

struct storage * storage_init(int fd);
struct storage * storage_open(int fd);
void storage_delete(struct storage * storage);

struct storage_table * storage_find_table(struct storage * storage, const char * name);

// storage_table

void storage_table_delete(struct storage_table * table);

void storage_table_add(struct storage_table * table);
void storage_table_remove(struct storage_table * table);
struct storage_row * storage_table_get_first_row(struct storage_table * table);
struct storage_row * storage_table_add_row(struct storage_table * table);

// storage_row

void storage_row_delete(struct storage_row * row);

struct storage_row * storage_row_next(struct storage_row * row);
void storage_row_remove(struct storage_row * row);
struct storage_value * storage_row_get_value(struct storage_row * row, uint16_t index);
void storage_row_set_value(struct storage_row * row, uint16_t index, struct storage_value * value);

// storage_value

void storage_value_destroy(struct storage_value value);
void storage_value_delete(struct storage_value * value);

// storage_column_type

const char * storage_column_type_to_string(enum storage_column_type type);
