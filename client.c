#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>

#include "json_api.h"
#include "y.tab.h"

void scan_string(const char * str);

static bool is_error_response(struct json_object * response) {
    json_object_object_foreach(response, key, val) {
        if (strcmp("error", key) == 0) {
            printf("Error: %s.\n", json_object_get_string(val));
            return true;
        }
    }

    return false;
}

static struct json_object * get_success_response(struct json_object * response) {
    json_object_object_foreach(response, key, val) {
        if (strcmp("success", key) == 0) {
            struct json_object * answer = json_object_get(val);
            json_object_put(response);
            return answer;
        }
    }

    printf("Bad answer: %s\n", json_object_to_json_string_ext(response, JSON_C_TO_STRING_PRETTY));
    return NULL;
}

static void print_amount_response(struct json_object * response, const char * action) {
    json_object_object_foreach(response, key, val) {
        if (strcmp("amount", key) == 0) {
            printf("%lu rows was %s.\n", json_object_get_uint64(val), action);
            return;
        }
    }

    printf("Bad answer: %s\n", json_object_to_json_string_ext(response, JSON_C_TO_STRING_PRETTY));
}

static void print_table_separator(unsigned int columns_length, const int * columns_width) {
    for (int i = 0; i < columns_length; ++i) {
        putchar('+');

        for (int j = 0; j < columns_width[i] + 2; ++j) {
            putchar('-');
        }
    }

    puts("+");
}

static void print_table_response(struct json_object * response) {
    struct json_object * columns = NULL;
    struct json_object * values = NULL;

    json_object_object_foreach(response, key, val) {
        if (strcmp("columns", key) == 0) {
            columns = val;
            continue;
        }

        if (strcmp("values", key) == 0) {
            values = val;
            continue;
        }
    }

    if (columns == NULL || values == NULL) {
        printf("Bad answer: %s\n", json_object_to_json_string_ext(response, JSON_C_TO_STRING_PRETTY));
        return;
    }

    unsigned int rows_length = json_object_array_length(values);
    unsigned int columns_length = json_object_array_length(columns);
    int columns_width[columns_length];

    for (int i = 0; i < columns_length; ++i) {
        columns_width[i] = (int) strlen(json_object_get_string(json_object_array_get_idx(columns, i)));
    }

    for (int i = 0; i < rows_length; ++i) {
        struct json_object * row = json_object_array_get_idx(values, i);

        for (int j = 0; j < columns_length; ++j) {
            int width = (int) strlen(json_object_to_json_string(json_object_array_get_idx(row, j)));
            columns_width[j] = columns_width[j] > width ? columns_width[j] : width;
        }
    }

    print_table_separator(columns_length, columns_width);

    for (int i = 0; i < columns_length; ++i) {
        printf("| %*s ", columns_width[i], json_object_get_string(json_object_array_get_idx(columns, i)));
    }

    puts("|");

    for (int i = 0; i < rows_length; ++i) {
        struct json_object * row = json_object_array_get_idx(values, i);

        print_table_separator(columns_length, columns_width);

        for (int j = 0; j < columns_length; ++j) {
            printf("| %*s ", columns_width[j], json_object_to_json_string(json_object_array_get_idx(row, j)));
        }

        puts("|");
    }

    print_table_separator(columns_length, columns_width);
}

static void print_response(enum json_api_action action, struct json_object * response) {
    if (!response) {
        printf("Server didn't understand request.\n");
        return;
    }

    if (is_error_response(response)) {
        return;
    }

    response = get_success_response(response);
    if (!response) {
        return;
    }

    switch (action) {
        case JSON_API_TYPE_CREATE_TABLE:
            printf("Table was created.\n");
            break;

        case JSON_API_TYPE_DROP_TABLE:
            printf("Table was dropped.\n");
            break;

        case JSON_API_TYPE_INSERT:
            printf("Row was inserted.\n");
            break;

        case JSON_API_TYPE_DELETE:
            print_amount_response(response, "deleted");
            break;

        case JSON_API_TYPE_SELECT:
            print_table_response(response);
            break;

        case JSON_API_TYPE_UPDATE:
            print_amount_response(response, "updated");
            break;

        default:
            return;
    }
}

static bool handle_request(int socket, struct json_object * request) {
    const char * request_string = json_object_to_json_string_ext(request, 0);

    ssize_t wrote;
    size_t remaining = strlen(request_string);
    while (remaining > 0) {
        wrote = write(socket, request_string, remaining);

        if (wrote < 0) {
            return false;
        }

        request_string += wrote;
        remaining -= wrote;
    }

    char buffer[64 * 1024];
    ssize_t was_read = read(socket, buffer, sizeof(buffer) / sizeof(*buffer));
    if (was_read <= 0) {
        return false;
    }

    if (was_read == sizeof(buffer) / sizeof(*buffer)) {
        buffer[sizeof(buffer) / sizeof(*buffer) - 1] = '\0';
    } else {
        buffer[was_read] = '\0';
    }

    enum json_tokener_error response_error;
    struct json_object * response = json_tokener_parse_verbose(buffer, &response_error);
    if (response_error == json_tokener_success) {
        print_response(json_api_get_action(request), response);
    } else {
        printf("Bad answer (%s): %s.\n", json_tokener_error_desc(response_error), buffer);
    }

    return true;
}

static bool handle_command(int socket, const char * command) {
    struct json_object * request = NULL;
    char * error = NULL;

    scan_string(command);
    if (yyparse(&request, &error) != 0) {
        printf("Parsing error: %s.\n", error);
        return true;
    }

    if (!request) {
        return true;
    }

    return handle_request(socket, request);
}

int main() {
    // create a socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    // specify an address for the socket
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(9002);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // check for error with the connection
    if (connect(client_socket, (struct sockaddr *) &server_address, sizeof(server_address)) != 0) {
        perror("There was an error making a connection to the remote socket");
        return -1;
    }

    bool working = true;
    while (working) {
        size_t command_capacity = 0;
        char * command = NULL;

        printf("> ");
        fflush(stdout);

        ssize_t was_read = getline(&command, &command_capacity, stdin);
        if (was_read <= 0) {
            free(command);
            break;
        }

        command[was_read] = '\0';
        working = handle_command(client_socket, command);
    }

    // and then close the socket
    close(client_socket);

    printf("Bye!\n");
    return 0;
}
