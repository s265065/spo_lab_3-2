#include "json_api.h"

#include <string.h>
#include <errno.h>

enum json_api_action json_api_get_action(struct json_object * object) {
    json_object_object_foreach(object, key, val) {
        if (strcmp("action", key) == 0) {
            return (enum json_api_action) json_object_get_int(val);
        }
    }

    return -1;
}

struct json_api_create_table_request json_api_to_create_table_request(struct json_object * object) {
    struct json_api_create_table_request request;

    json_object_object_foreach(object, key, val) {
        if (strcmp("table", key) == 0) {
            request.table_name = strdup(json_object_get_string(val));
            continue;
        }

        if (strcmp("columns", key) == 0) {
            request.columns.amount = json_object_array_length(val);
            request.columns.columns = malloc(sizeof(*request.columns.columns) * request.columns.amount);

            for (int i = 0; i < request.columns.amount; ++i) {
                struct json_object * elem = json_object_array_get_idx(val, i);

                json_object_object_foreach(elem, elem_key, elem_val) {
                    if (strcmp("name", elem_key) == 0) {
                        request.columns.columns[i].name = strdup(json_object_get_string(elem_val));
                        continue;
                    }

                    if (strcmp("type", elem_key) == 0) {
                        request.columns.columns[i].type = (enum storage_column_type) json_object_get_int(elem_val);
                        continue;
                    }
                }
            }

            continue;
        }
    }

    return request;
}

struct json_api_drop_table_request json_api_to_drop_table_request(struct json_object * object) {
    struct json_api_drop_table_request request;
    request.table_name = NULL;

    json_object_object_foreach(object, key, val) {
        if (strcmp("table", key) == 0) {
            request.table_name = strdup(json_object_get_string(val));
            break;
        }
    }

    return request;
}

static struct storage_value * json_to_storage_value(struct json_object * object) {
    struct storage_value * value;

    switch (json_object_get_type(object)) {
        case json_type_boolean:
        case json_type_object:
        case json_type_array:
            errno = EINVAL;

        case json_type_null:
            return NULL;

        case json_type_double:
            value = malloc(sizeof(*value));
            value->type = STORAGE_COLUMN_TYPE_NUM;
            value->value.num = json_object_get_double(object);
            break;

        case json_type_int:
            value = malloc(sizeof(*value));
            value->value._int = json_object_get_int64(object);

            if (value->value._int < 0) {
                value->type = STORAGE_COLUMN_TYPE_INT;
                break;
            }

            value->type = STORAGE_COLUMN_TYPE_UINT;
            value->value.uint = json_object_get_uint64(object);
            break;

        case json_type_string:
            value = malloc(sizeof(*value));
            value->type = STORAGE_COLUMN_TYPE_STR;
            value->value.str = strdup(json_object_get_string(object));
            break;
    }

    return value;
}

struct json_api_insert_request json_api_to_insert_request(struct json_object * object) {
    struct json_api_insert_request request;

    request.columns.amount = 0;
    request.columns.columns = NULL;

    json_object_object_foreach(object, key, val) {
        if (strcmp("table", key) == 0) {
            request.table_name = strdup(json_object_get_string(val));
            continue;
        }

        if (strcmp("columns", key) == 0) {
            request.columns.amount = json_object_array_length(val);
            request.columns.columns = malloc(sizeof(*request.columns.columns) * request.columns.amount);

            for (int i = 0; i < request.columns.amount; ++i) {
                request.columns.columns[i] = strdup(json_object_get_string(json_object_array_get_idx(val, i)));
            }

            continue;
        }

        if (strcmp("values", key) == 0) {
            request.values.amount = json_object_array_length(val);
            request.values.values = malloc(sizeof(struct storage_value *) * request.values.amount);

            for (int i = 0; i < request.values.amount; ++i) {
                request.values.values[i] = json_to_storage_value(json_object_array_get_idx(val, i));
            }

            continue;
        }
    }

    return request;
}

static struct json_api_where * json_api_to_where(struct json_object * object) {
    struct json_api_where * where = malloc(sizeof(*where));

    {
        json_object_object_foreach(object, key, val) {
            if (strcmp("op", key) == 0) {
                where->op = (enum json_api_operator) json_object_get_int(val);
                break;
            }
        }
    }

    switch (where->op) {
        case JSON_API_OPERATOR_EQ:
        case JSON_API_OPERATOR_NE:
        case JSON_API_OPERATOR_LT:
        case JSON_API_OPERATOR_GT:
        case JSON_API_OPERATOR_LE:
        case JSON_API_OPERATOR_GE:
        {
            json_object_object_foreach(object, key, val) {
                if (strcmp("column", key) == 0) {
                    where->column = strdup(json_object_get_string(val));
                    continue;
                }

                if (strcmp("value", key) == 0) {
                    where->value = json_to_storage_value(val);
                    continue;
                }
            }

            break;
        }

        case JSON_API_OPERATOR_AND:
        case JSON_API_OPERATOR_OR:
        {
            json_object_object_foreach(object, key, val) {
                if (strcmp("left", key) == 0) {
                    where->left = json_api_to_where(val);
                    continue;
                }

                if (strcmp("right", key) == 0) {
                    where->right = json_api_to_where(val);
                    continue;
                }
            }

            break;
        }
    }

    return where;
}

struct json_api_delete_request json_api_to_delete_request(struct json_object * object) {
    struct json_api_delete_request request;
    request.where = NULL;

    json_object_object_foreach(object, key, val) {
        if (strcmp("table", key) == 0) {
            request.table_name = strdup(json_object_get_string(val));
            continue;
        }

        if (strcmp("where", key) == 0) {
            request.where = json_api_to_where(val);
            continue;
        }
    }

    return request;
}

struct json_api_select_request json_api_to_select_request(struct json_object * object) {
    struct json_api_select_request request;
    request.columns.amount = 0;
    request.columns.columns = NULL;
    request.joins.amount = 0;
    request.joins.joins = NULL;
    request.where = NULL;
    request.offset = 0;
    request.limit = 10;

    json_object_object_foreach(object, key, val) {
        if (strcmp("table", key) == 0) {
            request.table_name = strdup(json_object_get_string(val));
            continue;
        }

        if (strcmp("columns", key) == 0) {
            request.columns.amount = json_object_array_length(val);
            request.columns.columns = malloc(sizeof(*request.columns.columns) * request.columns.amount);

            for (int i = 0; i < request.columns.amount; ++i) {
                request.columns.columns[i] = strdup(json_object_get_string(json_object_array_get_idx(val, i)));
            }

            continue;
        }

        if (strcmp("where", key) == 0) {
            request.where = json_api_to_where(val);
            continue;
        }

        if (strcmp("offset", key) == 0) {
            request.offset = json_object_get_int(val);
            continue;
        }

        if (strcmp("limit", key) == 0) {
            request.limit = json_object_get_int(val);
            continue;
        }

        if (strcmp("joins", key) == 0) {
            request.joins.amount = json_object_array_length(val);
            request.joins.joins = malloc(sizeof(*request.joins.joins) * request.joins.amount);

            for (int i = 0; i < request.joins.amount; ++i) {
                struct json_object * elem = json_object_array_get_idx(val, i);

                json_object_object_foreach(elem, elem_key, elem_val) {
                    if (strcmp("table", elem_key) == 0) {
                        request.joins.joins[i].table = strdup(json_object_get_string(elem_val));
                    }

                    if (strcmp("t_column", elem_key) == 0) {
                        request.joins.joins[i].t_column = strdup(json_object_get_string(elem_val));
                    }

                    if (strcmp("s_column", elem_key) == 0) {
                        request.joins.joins[i].s_column = strdup(json_object_get_string(elem_val));
                    }
                }
            }

            continue;
        }
    }

    return request;
}

struct json_api_update_request json_api_to_update_request(struct json_object * object) {
    struct json_api_update_request request;
    request.where = NULL;

    json_object_object_foreach(object, key, val) {
        if (strcmp("table", key) == 0) {
            request.table_name = strdup(json_object_get_string(val));
            continue;
        }

        if (strcmp("columns", key) == 0) {
            request.columns.amount = json_object_array_length(val);
            request.columns.columns = malloc(sizeof(*request.columns.columns) * request.columns.amount);

            for (int i = 0; i < request.columns.amount; ++i) {
                request.columns.columns[i] = strdup(json_object_get_string(json_object_array_get_idx(val, i)));
            }

            continue;
        }

        if (strcmp("values", key) == 0) {
            request.values.amount = json_object_array_length(val);
            request.values.values = malloc(sizeof(struct storage_value *) * request.values.amount);

            for (int i = 0; i < request.values.amount; ++i) {
                request.values.values[i] = json_to_storage_value(json_object_array_get_idx(val, i));
            }

            continue;
        }

        if (strcmp("where", key) == 0) {
            request.where = json_api_to_where(val);
            continue;
        }
    }

    return request;
}

struct json_object * json_api_make_success(struct json_object * answer) {
    struct json_object * object = json_object_new_object();

    json_object_object_add(object, "success", answer);
    return object;
}

struct json_object * json_api_make_error(const char * msg) {
    struct json_object * object = json_object_new_object();

    json_object_object_add(object, "error", json_object_new_string(msg));
    return object;
}

struct json_object * json_api_from_value(struct storage_value * value) {
    if (value == NULL) {
        return NULL;
    }

    switch (value->type) {
        case STORAGE_COLUMN_TYPE_INT:
            return json_object_new_int64(value->value._int);

        case STORAGE_COLUMN_TYPE_UINT:
            return json_object_new_uint64(value->value.uint);

        case STORAGE_COLUMN_TYPE_NUM:
            return json_object_new_double(value->value.num);

        case STORAGE_COLUMN_TYPE_STR:
            return json_object_new_string(value->value.str);
    }
}
