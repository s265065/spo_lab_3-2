#define _LARGEFILE64_SOURCE

#include "storage.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define SIGNATURE ("\xDE\xAD\xBA\xBE")

struct storage * storage_init(int fd) {
    lseek64(fd, 0, SEEK_SET);

    write(fd, SIGNATURE, 4);

    uint64_t p = 0;
    write(fd, &p, sizeof(p));

    struct storage * storage = malloc(sizeof(*storage));

    storage->fd = fd;
    storage->first_table = 0;
    return storage;
}

struct storage * storage_open(int fd) {
    lseek64(fd, 0, SEEK_SET);

    char sign[4];
    if (read(fd, sign, 4) != 4) {
        errno = EINVAL;
        return NULL;
    }

    if (memcmp(sign, SIGNATURE, 4) != 0) {
        errno = EINVAL;
        return NULL;
    }

    struct storage * storage = malloc(sizeof(*storage));
    storage->fd = fd;

    read(fd, &storage->first_table, sizeof(storage->first_table));
    return storage;
}

void storage_delete(struct storage * storage) {
    free(storage);
}

static char * storage_read_string(int fd) {
    uint16_t length;

    read(fd, &length, sizeof(length));

    char * str = malloc(sizeof(int8_t) * (length + 1));
    read(fd, str, length);
    str[length] = '\0';

    return str;
}

struct storage_table * storage_find_table(struct storage * storage, const char * name) {
    uint64_t pointer = storage->first_table;

    while (pointer) {
        lseek64(storage->fd, (off64_t) pointer, SEEK_SET);

        uint64_t next, first_row;
        read(storage->fd, &next, sizeof(next));
        read(storage->fd, &first_row, sizeof(first_row));

        char * table_name = storage_read_string(storage->fd);
        if (strcmp(table_name, name) != 0) {
            free(table_name);
            pointer = next;
            continue;
        }

        struct storage_table * table = malloc(sizeof(*table));
        table->storage = storage;
        table->position = pointer;
        table->next = next;
        table->first_row = first_row;
        table->name = table_name;

        read(storage->fd, &table->columns.amount, sizeof(table->columns.amount));
        table->columns.columns = malloc(sizeof(*table->columns.columns) * table->columns.amount);

        for (uint16_t i = 0; i < table->columns.amount; ++i) {
            table->columns.columns[i].name = storage_read_string(storage->fd);

            uint8_t type;
            read(storage->fd, &type, sizeof(type));
            table->columns.columns[i].type = (enum storage_column_type) type;
        }

        return table;
    }

    return NULL;
}

void storage_table_delete(struct storage_table * table) {
    if (table) {
        free(table->name);

        for (uint16_t i = 0; i < table->columns.amount; ++i) {
            free(table->columns.columns[i].name);
        }

        free(table->columns.columns);
    }

    free(table);
}

static uint64_t storage_write(int fd, void * buf, size_t length) {
    uint64_t offset = lseek64(fd, 0, SEEK_END);

    write(fd, buf, length);
    return offset;
}

static uint64_t storage_write_string(int fd, const char * str) {
    uint16_t length = strlen(str);

    uint64_t ret = storage_write(fd, &length, sizeof(length));
    write(fd, str, length);
    return ret;
}

void storage_table_add(struct storage_table * table) {
    struct storage_table * another_table = storage_find_table(table->storage, table->name);

    if (another_table != NULL) {
        storage_table_delete(another_table);
        errno = EINVAL;
        return;
    }

    table->next = table->storage->first_table;
    table->position = storage_write(table->storage->fd, &table->next, sizeof(table->next));
    table->storage->first_table = table->position;

    write(table->storage->fd, &table->first_row, sizeof(table->first_row));
    storage_write_string(table->storage->fd, table->name);
    write(table->storage->fd, &table->columns.amount, sizeof(table->columns.amount));

    for (uint16_t i = 0; i < table->columns.amount; ++i) {
        storage_write_string(table->storage->fd, table->columns.columns[i].name);

        uint8_t type = table->columns.columns[i].type;
        write(table->storage->fd, &type, sizeof(type));
    }

    lseek64(table->storage->fd, 4, SEEK_SET);
    write(table->storage->fd, &table->position, sizeof(table->position));
}

void storage_table_remove(struct storage_table * table) {
    uint64_t pointer = table->storage->first_table;

    while (pointer) {
        lseek64(table->storage->fd, (off64_t) pointer, SEEK_SET);

        uint64_t next;
        read(table->storage->fd, &next, sizeof(next));

        if (next == table->position) {
            break;
        }

        pointer = next;
    }

    if (pointer == 0) {
        pointer = 4;
        table->storage->first_table = table->next;
    }

    lseek64(table->storage->fd, (off64_t) pointer, SEEK_SET);
    write(table->storage->fd, &table->next, sizeof(table->next));
}

struct storage_row * storage_table_get_first_row(struct storage_table * table) {
    if (table->first_row == 0) {
        return NULL;
    }

    struct storage_row * row = malloc(sizeof(*row));
    row->position = table->first_row;
    row->table = table;

    lseek64(table->storage->fd, (off64_t) row->position, SEEK_SET);
    read(table->storage->fd, &row->next, sizeof(row->next));

    return row;
}

struct storage_row * storage_table_add_row(struct storage_table * table) {
    struct storage_row * row = malloc(sizeof(*row));

    row->table = table;
    row->next = table->first_row;
    row->position = storage_write(table->storage->fd, &row->next, sizeof(row->next));
    table->first_row = row->position;

    uint64_t null = 0;
    for (uint16_t i = 0; i < table->columns.amount; ++i) {
        write(table->storage->fd, &null, sizeof(null));
    }

    lseek64(table->storage->fd, (off64_t) (table->position + sizeof(uint64_t)), SEEK_SET);
    write(table->storage->fd, &table->first_row, sizeof(table->first_row));
    return row;
}

void storage_row_delete(struct storage_row * row) {
    free(row);
}

struct storage_row * storage_row_next(struct storage_row * row) {
    row->position = row->next;

    if (row->next == 0) {
        free(row);
        return NULL;
    }

    lseek64(row->table->storage->fd, (off64_t) row->position, SEEK_SET);
    read(row->table->storage->fd, &row->next, sizeof(row->next));
    return row;
}

void storage_row_remove(struct storage_row * row) {
    uint64_t pointer = row->table->first_row;

    while (pointer) {
        lseek64(row->table->storage->fd, (off64_t) pointer, SEEK_SET);

        uint64_t next;
        read(row->table->storage->fd, &next, sizeof(next));

        if (next == row->position) {
            break;
        }

        pointer = next;
    }

    if (pointer == 0) {
        pointer = row->table->position + sizeof(uint64_t);
        row->table->first_row = row->next;
    }

    lseek64(row->table->storage->fd, (off64_t) pointer, SEEK_SET);
    write(row->table->storage->fd, &row->next, sizeof(row->next));
}

struct storage_value * storage_row_get_value(struct storage_row * row, uint16_t index) {
    if (index >= row->table->columns.amount) {
        errno = EINVAL;
        return NULL;
    }

    lseek64(row->table->storage->fd, (off64_t) (row->position + (1 + index) * sizeof(uint64_t)), SEEK_SET);

    uint64_t pointer;
    read(row->table->storage->fd, &pointer, sizeof(pointer));

    if (pointer == 0) {
        return NULL;
    }

    lseek64(row->table->storage->fd, (off64_t) pointer, SEEK_SET);

    struct storage_value * value = malloc(sizeof(*value));
    value->type = row->table->columns.columns[index].type;

    switch (value->type) {
        case STORAGE_COLUMN_TYPE_INT:
            read(row->table->storage->fd, &value->value._int, sizeof(value->value._int));
            break;

        case STORAGE_COLUMN_TYPE_UINT:
            read(row->table->storage->fd, &value->value.uint, sizeof(value->value.uint));
            break;

        case STORAGE_COLUMN_TYPE_NUM:
            read(row->table->storage->fd, &value->value.num, sizeof(value->value.num));
            break;

        case STORAGE_COLUMN_TYPE_STR:
            value->value.str = storage_read_string(row->table->storage->fd);
            break;
    }

    return value;
}

void storage_row_set_value(struct storage_row * row, uint16_t index, struct storage_value * value) {
    if (index >= row->table->columns.amount) {
        errno = EINVAL;
        return;
    }

    uint64_t pointer = 0;

    if (value) {
        if (row->table->columns.columns[index].type != value->type) {
            errno = EINVAL;
            return;
        }

        switch (value->type) {
            case STORAGE_COLUMN_TYPE_INT:
                pointer = storage_write(row->table->storage->fd, &value->value._int, sizeof(value->value._int));
                break;

            case STORAGE_COLUMN_TYPE_UINT:
                pointer = storage_write(row->table->storage->fd, &value->value.uint, sizeof(value->value.uint));
                break;

            case STORAGE_COLUMN_TYPE_NUM:
                pointer = storage_write(row->table->storage->fd, &value->value.num, sizeof(value->value.num));
                break;

            case STORAGE_COLUMN_TYPE_STR:
                pointer = storage_write_string(row->table->storage->fd, value->value.str);
                break;
        }
    }

    lseek64(row->table->storage->fd, (off64_t) (row->position + (1 + index) * sizeof(uint64_t)), SEEK_SET);
    write(row->table->storage->fd, &pointer, sizeof(pointer));
}

void storage_value_destroy(struct storage_value value) {
    switch (value.type) {
        case STORAGE_COLUMN_TYPE_STR:
            free(value.value.str);

        default:
            break;
    }
}

void storage_value_delete(struct storage_value * value) {
    if (value) {
        storage_value_destroy(*value);
    }

    free(value);
}

const char * storage_column_type_to_string(enum storage_column_type type) {
    switch (type) {
        case STORAGE_COLUMN_TYPE_INT:
            return "int";

        case STORAGE_COLUMN_TYPE_UINT:
            return "uint";

        case STORAGE_COLUMN_TYPE_NUM:
            return "num";

        case STORAGE_COLUMN_TYPE_STR:
            return "str";

        default:
            return NULL;
    }
}
