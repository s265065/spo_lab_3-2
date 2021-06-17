#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "storage.h"

void generate(struct storage * storage) {
    const char * tables[] = { "test1", "test2", "test3", "test4", "test5", "test6" };

    for (int i = 0; i < 6; ++i) {
        struct storage_table table;

        table.storage = storage;
        table.first_row = 0;
        table.name = tables[i];
        table.next = 0;
        table.position = 0;
        table.columns.amount = 4;
        table.columns.columns[0].name = "int";
        table.columns.columns[0].type = STORAGE_COLUMN_TYPE_INT;
        table.columns.columns[1].name = "uint";
        table.columns.columns[1].type = STORAGE_COLUMN_TYPE_UINT;
        table.columns.columns[2].name = "num";
        table.columns.columns[2].type = STORAGE_COLUMN_TYPE_NUM;
        table.columns.columns[3].name = "str";
        table.columns.columns[3].type = STORAGE_COLUMN_TYPE_STR;
    }
}

int main(int argc, char * argv[]) {
    if (argc < 2) {
        return 0;
    }

    int fd = open(argv[1], O_RDWR);
    struct storage * storage;

    if (fd < 0 && errno != ENOENT) {
        perror("Error while opening file");
        return errno;
    }

    if (fd < 0 && errno == ENOENT) {
        fd = open("file.db", O_CREAT | O_RDWR, 0644);
        storage = storage_init(fd);
    } else {
        storage = storage_open(fd);
    }

    generate(storage);

    storage_delete(storage);
    close(fd);

    printf("Bye!\n");
    return 0;
}
