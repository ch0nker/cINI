#include "ini/reader.h"
#include "ini/types.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32
#define stat _stat
#endif

#include "cini.h"

char* test_func(ini* root) {
    return "test";
}

void update_handler(update_mode mode, section* section, variable* variable, comment* comment) {
    const char* status = mode == INI_ON_REMOVE ? "removed" :
        mode == INI_ON_EDIT ? "edited" :
        mode == INI_ON_ADD ? "added" : "unknown";

    if(section) {
        printf("%s section \"%s\"\n", status, section->name);
        return;
    }

    if(variable && variable->value) {
        printf("%s variable \"%s\" to \"%s\"\n", status, variable->key, variable->value);
        return;
    }
    if(comment) {
        printf("%s comment at line %i to %s\n", status, comment->line, comment->text);
        return;
    }
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <ini-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char* ini_fp = argv[1];

    ini* root = ini_read(ini_fp);

    section* parent     = ini_get_section(root, "parent");
    section* child      = section_get_section(parent, "child");
    section* gchild     = section_get_section(child, "grandchild");
    section* spouse     = ini_get_section(root, "spouse");

    char* full_name_1 = section_read_string(parent, "full_name");
    char* full_name_2 = section_read_string(child, "full_name");
    char* full_name_4 = section_read_string(gchild, "full_name");
    char* full_name_5 = section_read_string(spouse, "full_name");

    int age_1 = section_read_int(parent, "age", 1);
    int age_2 = section_read_int(child, "age", 1);
    int age_4 = section_read_int(gchild, "age", 1);
    int age_5 = section_read_int(spouse, "age", 1);

    printf("Parent:\n\tName: %s\n\tAge: %i\n", full_name_1, age_1);
    printf("Child:\n\tName: %s\n\tAge: %i\n", full_name_2, age_2);
    printf("GrandChild:\n\tName: %s\n\tAge: %i\n", full_name_4, age_4);
    printf("Spouse:\n\tName: %s\n\tAge: %i\n", full_name_5, age_5);

    free(full_name_1);
    free(full_name_2);
    free(full_name_4);

    ini_on_update(root, update_handler);
    ini_init_update(root);

    getchar();

    ini_write(root, argv[1]);
    ini_free(root);

    printf("successful exit\n");

    return EXIT_SUCCESS;
}