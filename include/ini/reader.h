#ifndef INI_READER_H_
#define INI_READER_H_

#include <stddef.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define strdup _strdup
#define PATH_MAX MAX_PATH
#define realpath(fp, full_path) _fullpath(full_path, fp, MAX_PATH)
#define INI_API __declspec(dllexport)
#else
#define PATH_MAX 4096
#define INI_API
#endif

#define MAX_UPDATE_CALLBACK 256
#define MAX_VARIABLES       256
#define MAX_SECTIONS        128
#define MAX_BUCKET          32
#define COMMENT_CAPACITY    256
#define MULTI_LINE_CAPACITY 1024
#define INOTIFY_BUF_LEN     (sizeof(struct inotify_event) + NAME_MAX + 1)
// #define MAX_INI_LINES       

#define CHECK_ESC(cursor, start) \
    ((start) == (cursor) || (cursor)[-1] != '\\')

typedef enum {
    INI_STRING  = 1 << 0,
    INI_LIST    = 1 << 1,
    INI_INT     = 1 << 2,
    INI_BOOL    = 1 << 3,
    INI_FLOAT   = 1 << 4,
    INI_DOUBLE  = 1 << 5
} VARIABLE_TYPE;

typedef struct variable {
    char* key;
    char* value;
    VARIABLE_TYPE type;

    struct comment* comment;
    int line;

    char multi_line;
    char seen;

    unsigned long hash;

    struct variable* prev;
    struct variable* next;

    struct variable* map_prev;
    struct variable* map_next;

    struct ini* root;
    struct section* parent;
} variable;

typedef struct variable_bucket {
    variable* head;
    variable* tail;
    int size;
} variable_bucket;

typedef struct variable_map {
    variable* head;
    variable* tail;

    variable_bucket buckets[MAX_BUCKET];
} variable_map;

typedef struct section_bucket {
    struct section* head;
    struct section* tail;
    int size;
} section_bucket;

typedef struct section_map {
    struct section* head;
    struct section* tail;

    section_bucket buckets[MAX_BUCKET];
} section_map;

typedef struct section {
    char* name;
    
    int level;
    int line;
    char seen;

    struct comment* comment;

    variable_map variables;
    section_map sections;

    unsigned long hash;

    struct section* prev;
    struct section* next;

    struct section* map_next;
    struct section* map_prev;

    struct section* parent;
    struct ini* root;
} section;

typedef struct {
    section* current;
    section* stack[256];
    int size;
} section_stack;

typedef struct {
    section_stack* section_stacks[16];
    int section_stacks_size;
    int last_level;

    int multi_line_capacity;
    char* multi_line;
    char* multi_key;
    int multi_key_size;
    char* multi_end;

    char* line;
    int line_pos;
    int line_position;
    size_t line_size;
    size_t line_capacity;
} ini_context;

typedef struct comment {
    char* text;
    size_t size;
    char prefix;
    int line;

    char seen;

    struct comment* next;
    struct comment* prev;
} comment;

typedef struct comment_list {
    int capacity;
    int last_line;
    comment** array;

    comment* head;
    comment* tail;
} comment_list;

typedef enum update_mode {
    INI_ON_ADD      = 1 << 0,
    INI_ON_EDIT     = 1 << 1,
    INI_ON_REMOVE   = 1 << 2
} update_mode;

typedef void (*update_callback)(update_mode mode, section* section, variable* variable, comment* comment);

typedef struct update_node {
    update_callback callback;
    struct update_node* next;
    struct update_node* prev;
} update_node;

typedef struct update_array {
    size_t size;

    update_node* head;
    update_node* tail;
} update_array;

typedef struct line_info {
    variable* variable;
    section* section;
    comment* comment;
    int line;
    int line_count;
} line_info;

typedef struct line_array {
    line_info** array;
    int capacity;
    int size;
} line_array;

typedef struct ini {
    char* fp;
    line_array lines;
    update_array update;
    section_map sections;
    variable_map variables;
    comment_list comments;
} ini;

INI_API ini*            ini_read(const char* fp);

INI_API char            ini_init_update(ini* root);
INI_API update_node*    ini_on_update(ini* root, update_callback callback);
INI_API void            ini_remove_update(update_node* node);

INI_API section*        ini_create_section(const char* name);
INI_API variable*       ini_create_variable(const char* key, const char* value);
INI_API comment*        ini_create_comment(const char* text, int line);
INI_API ini*            ini_create();

INI_API variable*       ini_get_variable(ini* root, const char* key);
INI_API section*        ini_get_section(ini* root, const char* name);
INI_API section*        section_get_section(section* root, const char* name);
INI_API variable*       section_get_variable(section* root, const char* key);

INI_API void            variable_set_value(variable* var, const char* value);
INI_API char*           variable_get_value(variable* var);
INI_API char*           variable_get_key(variable* var);

INI_API void            ini_add_variable(ini* root, variable* var);
INI_API void            ini_add_section(ini* root, section* section);
INI_API void            ini_add_comment(ini* root, comment* comment);
INI_API void            section_add_section(section* root, section* section);
INI_API void            section_add_variable(section* root, variable* var);

INI_API void            ini_remove_variable(ini* root, const char* key);
INI_API void            ini_remove_section(ini* root, const char* name);
INI_API void            section_remove_variable(section* root, const char* key);

INI_API void            ini_free(ini* root);

#endif
