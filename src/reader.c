#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>


#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <linux/limits.h>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#endif

#include "ini/reader.h"


char cmp_string(const char* a, const char* b) {
    while(*b)
        if(*a++ != *b++)
            return 0;

    if(*b != *a)
        return 0;

    return 1;
}

// https://stackoverflow.com/a/735472
size_t getline_s(char **lineptr, size_t *n, FILE *stream) {
    char *bufptr = NULL;
    char *p = bufptr;
    size_t size;
    int c;

    if (lineptr == NULL) {
        return -1;
    }
    if (stream == NULL) {
        return -1;
    }
    if (n == NULL) {
        return -1;
    }
    bufptr = *lineptr;
    size = *n;

    c = fgetc(stream);
    if (c == EOF) {
        return -1;
    }
    if (bufptr == NULL) {
        bufptr = malloc(128);
        if (bufptr == NULL) {
            return -1;
        }
        size = 128;
    }
    p = bufptr;
    while(c != EOF) {
        if ((p - bufptr) > (size - 1)) {
            size = size + 128;
            bufptr = realloc(bufptr, size);
            if (bufptr == NULL) {
                return -1;
            }
        }
        *p++ = c;
        if (c == '\n') {
            break;
        }
        c = fgetc(stream);
    }

    *p++ = '\0';
    *lineptr = bufptr;
    *n = size;

    return p - bufptr - 1;
}

unsigned long hash_fnv1a(const char *str) {
    unsigned long hash = 2166136261UL;
    while (*str) {
        hash ^= (unsigned char)(*str++);
        hash *= 1099511628211UL;
    }
    return hash;
}

ini* ini_create() {
    ini* result = calloc(1, sizeof(ini));
    if(!result) {
        fprintf(stderr, "Failed to allocate ini\n");
        exit(EXIT_FAILURE);
    }

    result->comments.array = calloc(COMMENT_CAPACITY, sizeof(comment*));
    result->comments.capacity = COMMENT_CAPACITY;

    return result;
}

section* ini_create_section(const char* name) {
    section* result = calloc(1, sizeof(section));
    if(!result) {
        fprintf(stderr, "Failed to allocate section\n");
        exit(EXIT_FAILURE);
    }

    result->name = strdup(name);

    return result;
}

comment* ini_create_comment(const char* text, int line) {
    comment* result = calloc(1, sizeof(comment));

    char* text_copy = strdup(text);
    size_t text_size = strlen(text);

    result->text = text_copy;
    result->size = text_size;
    result->prefix = ';';
    result->line = line;

    return result;
}

variable* ini_create_variable(const char* key, const char* value) {
    variable* result = calloc(1, sizeof(variable));
    if(!result) {
        fprintf(stderr, "Failed to allocate variable\n");
        exit(1);
    }

    result->key = strdup(key);
    result->value = strdup(value);

    return result;
}

void ini_run_updates(ini* root, update_mode mode, section* section, variable* variable, comment* comment) {
    if(!root)
        return;

    update_node* current = root->update.head;
    while(current) {
        current->callback(mode, section, variable, comment);
        current = current->next;
    }
}

variable* ini_get_variable(ini* root, const char* key) {
    variable_map* map = &root->variables;
    
    unsigned long hash = hash_fnv1a(key);
    int hash_key = hash % MAX_BUCKET;

    variable_bucket* bucket = &map->buckets[hash_key];

    if(!bucket->head)
        return NULL;

    variable* current = bucket->head;

    while(current)
        if(current->hash == hash && cmp_string(current->key, key))
            return current;
        else current = current->next;

    return NULL;
}

section* ini_get_section(ini* root, const char* name) {
    section_map* map = &root->sections;
    
    unsigned long hash = hash_fnv1a(name);
    int hash_key = hash % MAX_BUCKET;

    section_bucket* bucket = &map->buckets[hash_key];
    if(!bucket) 
        return NULL;

    if(!bucket->head)
        return NULL;

    section* current = bucket->head;
    while(current)
        if(current->hash == hash && cmp_string(current->name, name))
            return current;
        else current = current->next;

    return NULL;
}

section* section_get_section(section* root, const char* name) {
    if(!root)
        return NULL;

    section_map* map = &root->sections;
    
    unsigned long hash = hash_fnv1a(name);
    int hash_key = hash % MAX_BUCKET;

    section_bucket* bucket = &map->buckets[hash_key];
    if(!bucket) 
        return NULL;

    if(!bucket->head)
        return NULL;

    section* current = bucket->head;
    while(current)
        if(current->hash == hash && cmp_string(current->name, name))
            return current;
        else current = current->next;

    return NULL;
}

variable* section_get_variable(section* root, const char* key) {
    if(!root)
        return NULL;

    variable_map* map = &root->variables;
    
    unsigned long hash = hash_fnv1a(key);
    int hash_key = hash % MAX_BUCKET;

    variable_bucket* bucket = &map->buckets[hash_key];

    if(!bucket->head)
        return NULL;

    variable* current = bucket->head;
    while(current)
        if(current->hash == hash && cmp_string(current->key, key))
            return current;
        else current = current->next;

    return NULL;
}

void variable_set_value(variable* var, const char* value) {
    free(var->value);
    var->value = strdup(value);
}

char* variable_get_value(variable* var) {
    return var->value;
}

char* variable_get_key(variable* var) {
    return var->key;
}

void ini_add_variable(ini* root, variable* var) {
    variable_map* map = &root->variables;

    unsigned long hash = hash_fnv1a(var->key);
    int hash_key = hash % MAX_BUCKET;

    variable_bucket* bucket = &map->buckets[hash_key];

    var->hash = hash;

    var->map_prev = map->tail;
    if(map->tail)
        map->tail->map_next = var;
    else map->head = var;
    map->tail = var;

    var->prev = bucket->tail;
    if(bucket->tail)
        bucket->tail->next = var;
    else bucket->head = var;
    bucket->tail = var;

    var->parent = NULL;

    ini_run_updates(root, INI_ON_ADD, NULL, var, NULL);
}

void ini_add_comment(ini* root, comment* comment) {
    comment_list* list = &root->comments;
    int old_capacity = list->capacity;
    while(comment->line >= list->capacity)
        list->capacity *= 2;

    if(list->capacity != old_capacity) {
      struct comment** tmp = realloc(list->array, list->capacity * sizeof(void*));
      if(!tmp) {
          fprintf(stderr, "Failed to allocate commnets array\n");
          exit(EXIT_FAILURE);
      }
      list->array = tmp;
    }

    list->array[comment->line] = comment;

    if(!list->head)
        list->head = comment;

    if(list->tail)
        list->tail->next = comment;

    comment->prev = list->tail;
    list->tail = comment;
    list->last_line = comment->line;

    ini_run_updates(root, INI_ON_ADD, NULL, NULL, comment);
}

void ini_add_section(ini* root, section* section) {
    section_map* map = &root->sections;

    unsigned long hash = hash_fnv1a(section->name);
    int hash_key = hash % MAX_BUCKET;

    section_bucket* bucket = &map->buckets[hash_key];

    section->hash = hash;

    section->map_prev = map->tail;
    if(map->tail)
        map->tail->map_next = section;
    else map->head = section;
    map->tail = section;

    section->prev = bucket->tail;
    if(bucket->tail)
        bucket->tail->next = section;
    else bucket->head = section;
    bucket->tail = section;

    section->parent = NULL;
    section->root = root;

    ini_run_updates(root, INI_ON_ADD, section, NULL, NULL);
}

void ini_add_line(ini* root, variable* variable, section* section, comment* comment) {
    line_array* array = &root->lines;

    if(!array->capacity) {
        array->capacity = 32;
        array->size = 0;
        array->array = malloc(array->capacity * sizeof(line_info*));
    }

    int line = array->size > 0 ? 
        array->array[array->size - 1]->line + 1 : 1;

    if(array->capacity <= array->size + 1) {
        array->capacity += 64;

        line_info** tmp = realloc(array->array, array->capacity * sizeof(line_info*));
        if(!tmp) {
            fprintf(stderr, "Failed to realloc line_info\n");
            exit(EXIT_FAILURE);
            return;
        }

        free(array->array);
        array->array = tmp;
    }

    if(comment) {
        if(variable)
            variable->comment = comment;
        else if(section)
            section->comment = comment;
    }

    line_info* info = malloc(sizeof(line_info));
    info->line = line;
    info->comment = comment;
    info->variable = variable;
    info->section = section;

    array->array[line - 1] = info;
    array->size++;
}

void section_add_section(section* root, section* section) {
    section_map* map = &root->sections;

    unsigned long hash = hash_fnv1a(section->name);
    int hash_key = hash % MAX_BUCKET;

    section_bucket* bucket = &map->buckets[hash_key];

    section->hash = hash;

    section->map_prev = map->tail;
    if(map->tail)
        map->tail->map_next = section;
    else map->head = section;
    map->tail = section;

    section->prev = bucket->tail;
    if(bucket->tail)
        bucket->tail->next = section;
    else bucket->head = section;
    bucket->tail = section;

    section->parent = root;
    section->root = root->root;

    ini_run_updates(section->root, INI_ON_ADD, section, NULL, NULL);
}

void section_add_variable(section* root, variable* var) {
    variable_map* map = &root->variables;
    
    unsigned long hash = hash_fnv1a(var->key);
    int hash_key = hash % MAX_BUCKET;
    
    variable_bucket* bucket = &map->buckets[hash_key];

    var->hash = hash;

    var->map_prev = map->tail;
    if(map->tail)
        map->tail->map_next = var;
    else map->head = var;
    map->tail = var;

    var->prev = bucket->tail;
    if(bucket->tail)
        bucket->tail->next = var;
    else bucket->head = var;
    bucket->tail = var;

    var->parent = root;
    var->root = root->root;

    ini_run_updates(root->root, INI_ON_ADD, NULL, var, NULL);
}

void free_variable_map(variable_map* map) {
    variable* current = map->head;
    while(current) {
        variable* next = current->map_next;

        free(current->key);
        free(current->value);
        free(current);

        current = next;
    }

    map->tail = NULL;
    map->head = NULL;
}

void free_section_map(section_map* map) {
    section* current = map->head;
    while(current) {
        section* next = current->map_next;

        free_section_map(&current->sections);
        free_variable_map(&current->variables);

        free(current->name);
        free(current);

        current = next;
    }

    map->head = NULL;
    map->tail = NULL;
}

void free_comment_list(comment_list* list) {
    comment* current = list->head;
    while(current) {
        comment* next = current->next;

        free(current->text);
        free(current);

        current = next;
    }

    free(list->array);

    list->head = NULL;
    list->tail = NULL;
    list->capacity = 0;
    list->array = NULL;
}

void ini_free(ini* root) {
    free_variable_map(&root->variables);
    free_section_map(&root->sections);
    free_comment_list(&root->comments);
    free(root->fp);
    free(root);
}

void ini_remove_variable(ini* root, const char* key) {
    variable_map* map = &root->variables;

    int hash_key = hash_fnv1a(key) % MAX_BUCKET;

    variable_bucket* bucket = &map->buckets[hash_key];
    variable* result = ini_get_variable(root, key);
    if(!result)
        return;

    if(result->map_prev)
        result->map_prev->map_next = result->map_next;
    else
        map->head = result->map_next;

    if(result->map_next)
        result->map_next->map_prev = result->map_prev;
    else
        map->tail = result->map_prev;

    if(result->prev)
        result->prev->next = result->next;
    else
        bucket->head = result->next;

    if(result->next)
        result->next->prev = result->prev;
    else
        bucket->tail = result->prev;

    --bucket->size;

    free(result->key);
    free(result->value);
    free(result);
}

void ini_remove_section(ini* root, const char* name) {
    section_map* map = &root->sections;

    int hash_key = hash_fnv1a(name) % MAX_BUCKET;

    section_bucket* bucket = &map->buckets[hash_key];
    section* result = ini_get_section(root, name);
    if(!result)
        return;

    // section* prev = result->prev;

    if(result->map_prev)
        result->map_prev->map_next = result->map_next;
    else
        map->head = result->map_next;

    if(result->map_next)
        result->map_next->map_prev = result->map_prev;
    else
        map->tail = result->map_prev;

    if(result->prev)
        result->prev->next = result->next;
    else
        bucket->head = result->next;

    if(result->next)
        result->next->prev = result->prev;
    else
        bucket->tail = result->prev;

    --bucket->size;

    free(result->name);
    free_section_map(&result->sections);
    free_variable_map(&result->variables);
    free(result);
}

void section_remove_variable(section* root, const char* key) {
    variable_map* map = &root->variables;

    int hash_key = hash_fnv1a(key) % MAX_BUCKET;

    variable_bucket* bucket = &map->buckets[hash_key];
    variable* result = section_get_variable(root, key);
    if(!result)
        return;

    if(result->map_prev)
        result->map_prev->map_next = result->map_next;
    else
        map->head = result->map_next;

    if(result->map_next)
        result->map_next->map_prev = result->map_prev;
    else
        map->tail = result->map_prev;

    if(result->prev)
        result->prev->next = result->next;
    else
        bucket->head = result->next;

    if(result->next)
        result->next->prev = result->prev;
    else
        bucket->tail = result->prev;

    --bucket->size;

    free(result->key);
    free(result->value);
    free(result);
}

void section_remove_section(section* root, const char* name) {
    section_map* map = &root->sections;

    int hash_key = hash_fnv1a(name) % MAX_BUCKET;

    section_bucket* bucket = &map->buckets[hash_key];
    section* result = section_get_section(root, name);
    if(!result)
        return;

    if(result->map_prev)
        result->map_prev->map_next = result->map_next;
    else
        map->head = result->map_next;

    if(result->map_next)
        result->map_next->map_prev = result->map_prev;
    else
        map->tail = result->map_prev;

    if(result->prev)
        result->prev->next = result->next;
    else
        bucket->head = result->next;

    if(result->next)
        result->next->prev = result->prev;
    else
        bucket->tail = result->prev;

    --bucket->size;

    free(result->name);
    free_section_map(&result->sections);
    free_variable_map(&result->variables);
    free(result);
}

char* trim_string(const char* str, size_t* size) {
    const char* start = str;
    while (isspace(*start))
        ++start;

    const char* end = start + strlen(start);
    while (end > start && isspace(*(end - 1)))
        --end;

    size_t new_len = end - start;

    char* out = malloc(new_len + 1);
    if (!out)
        return NULL;

    memcpy(out, start, new_len);
    out[new_len] = '\0';
    if(size != NULL)
        *size = new_len;

    return out;
}

int is_comment(char* line) {
    int i = 0;

    if(line[i] == '#' || line[i] == ';')
        return 1;

    while(line[i++] == ' ')
        if(line[i] == '#' || line[i] == ';')
            return 1;

    return 0;
}

char* trim_comment(char* str, char* out_prefix) {
    char* ptr = NULL;
    char* start = str;
    while(*str) {
        if((*str == '#' || *str == ';') && (str == start || str[-1] != '\\')) {
            ptr = str;
            break;
        }
        ++str;
    }

    if(!ptr)
        return NULL;

    char before_chr = ptr[-1];
    if(isspace(before_chr))
        ptr[-1] = '\0';
    
    *out_prefix = *ptr;
    *ptr = '\0';
    return ptr + 1;
}

char check_multiline(char* value, char** multi_line, char** multi_end) {
    char* cursor = value;
    while(*cursor) {
        if(*cursor == '\\' && *(cursor + 1) != '\\' && (*(cursor + 1) == '\0' || isspace(*(cursor + 1)))) {
            *cursor = '\n';
            if(!*multi_line)
                *multi_line = malloc(MULTI_LINE_CAPACITY);

            int size = (cursor - value + 1);
            memcpy(*multi_line, value, size);
            *multi_end = *multi_line + size;
            return 1;
        }
        ++cursor;
    }

    return 0;
}

update_node* ini_on_update(ini* root, update_callback callback) {
    update_array* updates = &root->update;
    if(++updates->size >= MAX_UPDATE_CALLBACK)
        return NULL;
    
    update_node* node = malloc(sizeof(update_node));
    
    node->callback = callback;
    node->next = NULL;
    node->prev = NULL;

    if(updates->tail) {
        node->prev = updates->tail;
        updates->tail->next = node;
    }

    updates->tail = node;
    if(!updates->head)
        updates->head = node;
    
    return node;
}

void ini_remove_update(update_node* node) {
    if(node->next)
        node->next->prev = node->prev;
    if(node->prev)
        node->prev->next = node->next;

    free(node);
}

void ini_check_removed_variables(ini* root, variable_map* map) {
    variable* current = map->head;
    while(current) {
        variable* next = current->map_next;
        int hash_key = current->hash % MAX_BUCKET;
        variable_bucket* bucket = &map->buckets[hash_key];

        if(!current->seen) {
            ini_run_updates(root, INI_ON_REMOVE, NULL, current, NULL);

            if(current->map_prev)
                current->map_prev->map_next = current->map_next;
            else
                map->head = current->map_next;

            if(current->map_next)
                current->map_next->map_prev = current->map_prev;
            else
                map->tail = current->map_prev;

            if(current->prev)
                current->prev->next = current->next;
            else
                bucket->head = current->next;

            if(current->next)
                current->next->prev = current->prev;
            else
                bucket->tail = current->prev;

            --bucket->size;

            free(current->key);
            free(current->value);
            free(current);
        } else {
            current->seen = 0;
        }

        current = next;
    }
}

void ini_check_removed_sections(ini* root, section_map* map) {
    section* current = map->head;
    while(current) {
        section* next = current->map_next;
        int hash_key = current->hash % MAX_BUCKET;
        section_bucket* bucket = &map->buckets[hash_key];

        if(!current->seen) {
            ini_run_updates(root, INI_ON_REMOVE, current, NULL, NULL);

            if(current->map_prev)
                current->map_prev->map_next = current->map_next;
            else
                map->head = current->map_next;

            if(current->map_next)
                current->map_next->map_prev = current->map_prev;
            else
                map->tail = current->map_prev;

            if(current->prev)
                current->prev->next = current->next;
            else
                bucket->head = current->next;

            if(current->next)
                current->next->prev = current->prev;
            else
                bucket->tail = current->prev;

            --bucket->size;

            free(current->name);
            free_section_map(&current->sections);
            free_variable_map(&current->variables);
            free(current);
        } else {
            current->seen = 0;
            ini_check_removed_sections(root, &current->sections);
            ini_check_removed_variables(root, &current->variables);
        }

        current = next;
    }
}

ini_context* create_ini_context() {
    ini_context* ctx = calloc(1, sizeof(ini_context));
    if(!ctx) {
        fprintf(stderr, "Failed to allocate ini_context\n");
        exit(1);
    }

    ctx->section_stacks_size = 0;

    ctx->multi_line_capacity = MULTI_LINE_CAPACITY;
    ctx->multi_line = NULL;
    ctx->multi_key = NULL;
    ctx->multi_key_size = 0;
    ctx->multi_end = NULL;

    ctx->line_size = 0;
    ctx->line_pos = 0;
    ctx->line = NULL;
    ctx->line_size = -1;
    ctx->line_capacity = 0;

    return ctx;
}

section_stack* create_section_stack(ini_context* ctx) {
    if(ctx->section_stacks_size >= 16) {
        fprintf(stderr, "Label stacks extended further than the max amount of 16\n");
        exit(1);
    }

    section_stack* stack = calloc(1, sizeof(section_stack));
    if(!stack) {
        fprintf(stderr, "Failed to allocate section_stack\n");
        exit(1);
    }

    stack->size = 0;
    stack->current = NULL;

    ctx->section_stacks[ctx->section_stacks_size++] = stack;

    return stack;
}

void free_ini_context(ini_context* ctx) {
    for(int i = 0; i < ctx->section_stacks_size; ++i)
        free(ctx->section_stacks[i]);

    free(ctx);
}

void parse_variable(char* txt, char** out_key, char** out_type, char** out_value, char** out_comment, char* out_comment_prefix) {
    char* value_pos = NULL;
    char* type_pos = NULL;
    char* comment_pos = NULL;

    char* cursor = txt;
    while(*cursor != '=' && *cursor) {
        if(*cursor == ':')
            type_pos = cursor;
        cursor++;
    }

    if(cursor != txt)
        value_pos = cursor;

    if(!value_pos) {
        *out_key = NULL;
        *out_value = NULL;
        *out_type = NULL;
        *out_comment = NULL;
        return;
    }

    char* key_end = type_pos ? type_pos - 1 : value_pos - 1;
    while(isspace(*key_end)) key_end--;
    key_end++;

    if(type_pos)
        while(*type_pos == ':' || isspace(*type_pos)) type_pos++;

    while(*value_pos == '=' || isspace(*value_pos)) value_pos++;

    char* key = calloc(1, 256);
    memcpy(key, txt, (size_t)(key_end - txt));

    char* type = NULL;
    if(type_pos) {
        type = calloc(1, 256);
        char* t_cursor = type;
        while(*type_pos != ' ' && *type_pos != '=')
            *t_cursor++ = *type_pos++;

    }
    char* comment_ptr = trim_comment(value_pos, out_comment_prefix); // zeros out the comment so this is needed before value copy.
    char* value = malloc(1024);
    strcpy(value, value_pos);

    char* comment = NULL;
    if(comment_ptr) { 
        comment = malloc(512);
        strcpy(comment, comment_ptr);
    }

    *out_key = key;
    *out_value = value;
    *out_type = type;
    *out_comment = comment;
}

VARIABLE_TYPE parse_type(char* type) {
    VARIABLE_TYPE result = INI_STRING;

    if(memcmp(type, "list", 4) == 0)
        result = INI_LIST;
    else if(cmp_string(type, "int"))
        result = INI_INT;
    else if(cmp_string(type, "float"))
        result = INI_FLOAT;
    else if(cmp_string(type, "bool"))
        result = INI_BOOL;
    else if(cmp_string(type, "double"))
        result = INI_DOUBLE;

    return result;
}

void handle_sub_section(ini* root, ini_context* ctx, char* name) {
    int level = 0;
    section_stack* stack = ctx->section_stacks[0];

    char* level_cursor = name;
    while(*level_cursor++ == '.') level++;

    if(level < ctx->last_level)
        stack->size = 1;

    if(level > stack->size)
        level = stack->size;

    section* last_section = stack->stack[level - 1];
    char* section_name = name + level;

    section* existing = last_section
        ? section_get_section(last_section, section_name)
        : ini_get_section(root, section_name);

    if(existing) {
        stack->current = existing;
    } else {
        stack->current = ini_create_section(section_name);
        stack->current->level = level;

        if(last_section) {
            section_add_section(last_section, stack->current);
        } else {
            stack->current->level = 0;
            stack->size = 1;
            stack->stack[0] = stack->current;
            ini_add_section(root, stack->current);
        }
    }

    stack->current->level = level;
    stack->current->line = ctx->line_pos;
    stack->stack[stack->size++] = stack->current;
    ctx->last_level = level;
}

void ini_check_update(ini* root) {
    FILE* file = fopen(root->fp, "r");
    if(!file) {
        fprintf(stderr, "Failed to open %s\n", root->fp);
        exit(EXIT_FAILURE);
    }

    ini_context* ctx = create_ini_context();
    section_stack* stack = create_section_stack(ctx);

    while((ctx->line_size = getline_s(&ctx->line, &ctx->line_capacity, file)) != -1) {
        ++ctx->line_pos;
        
        if(ctx->multi_line) {
            int multi_size = (int)(ctx->multi_end - ctx->multi_line);
            if(multi_size + ctx->line_size > ctx->multi_line_capacity) {
                ctx->multi_line_capacity *= 2;
                char* tmp = realloc(ctx->multi_line, ctx->multi_line_capacity);
                if(!tmp) {
                    fprintf(stderr, "Failed to realloc multiline\n");
                    exit(EXIT_FAILURE);
                }
                ctx->multi_end = tmp + multi_size;
                ctx->multi_line = tmp;
            }

            memcpy(ctx->multi_end, ctx->line, ctx->line_size - 1);
            ctx->multi_end += ctx->line_size - 1;

            char end_1 = *(ctx->multi_end - 1);
            char end_2 = *(ctx->multi_end - 2);
            char is_multiline = (end_1 == '\\' && end_2 != '\\'
                && end_1 == '\\' && end_2 == ' ')
                || (end_1 == ' ' && end_2 == '\\');

            if(!is_multiline) {
                variable* var;

                if(stack->current)
                    var = section_get_variable(stack->current, ctx->multi_key);
                else
                    var = ini_get_variable(root, ctx->multi_key);

                if(!var) {
                    var = ini_create_variable(ctx->multi_key, ctx->multi_line);
                    if(stack->current)
                        section_add_variable(stack->current, var);
                    else
                        ini_add_variable(root, var);
                    free(ctx->multi_line);
                } else {
                    if(!cmp_string(var->value, ctx->multi_line)) {
                        if(var->value)
                            free(var->value);
                        var->value = ctx->multi_line;
                        ini_run_updates(root, INI_ON_EDIT, NULL, var, NULL);
                    }
                }
                var->seen = 1;

                free(ctx->multi_key);

                ctx->multi_key = NULL;
                ctx->multi_end = NULL;
                ctx->multi_line = NULL;
                ctx->multi_key_size = 0;
                ctx->multi_line_capacity = MULTI_LINE_CAPACITY;
            } else ctx->multi_end[-1] = '\n';
            continue;
        }

        size_t tmp_line_size = 0;
        char* tmp_line = trim_string(ctx->line, &tmp_line_size);
        switch(*tmp_line) {
            case '#':
            case ';': {
                comment* real_comment = root->comments.capacity > ctx->line_pos ?
                    root->comments.array[ctx->line_pos] : NULL;

                char* comment_text = calloc(1, tmp_line_size);
                memcpy(comment_text, tmp_line + 1, tmp_line_size - 1);

                if(!real_comment) {
                    comment* new_comment = calloc(1, sizeof(comment));
                    new_comment->text = comment_text;
                    new_comment->size = strlen(comment_text);
                    new_comment->line = ctx->line_pos;
                    ini_add_comment(root, new_comment);
                } else {
                    root->comments.array[real_comment->line] = NULL;
                    real_comment->line = ctx->line_pos;
                    root->comments.array[real_comment->line] = real_comment;
                    real_comment->seen = 1;
                    if(!cmp_string(real_comment->text, comment_text)) {
                        if(real_comment->text)
                            free(real_comment->text);
                        real_comment->text = comment_text;
                        real_comment->size = strlen(comment_text);
                        ini_run_updates(root, INI_ON_EDIT, NULL, NULL, real_comment);
                    } else free(comment_text);
                }
                goto end_loop;
            }
            case '[': {
                char name[1024];
                int name_size = 0;
                while(name_size < 1024) {
                    char copy_chr = tmp_line[name_size + 1];
                    if(!copy_chr || copy_chr == ']')
                        break;
                    name[name_size++] = copy_chr;
                }
                name[name_size] = '\0';

                stack->current = NULL;

                if(*name == '.') {
                    handle_sub_section(root, ctx, name);
                    stack->current->line = ctx->line_pos;
                    stack->current->seen = 1;
                } else {
                    stack->size = 0;
                    stack->current = ini_get_section(root, name);

                    if(!stack->current) {
                        stack->current = ini_create_section(name);
                        ini_add_section(root, stack->current);
                    }

                    stack->current->line = ctx->line_pos;
                    stack->current->seen = 1;
                    stack->stack[stack->size++] = stack->current;
                }

                char prefix = 0;
                char* comment_text = trim_comment(tmp_line + name_size, &prefix);
                if(comment_text) {
                    comment* real_comment = root->comments.capacity > ctx->line_pos ?
                        root->comments.array[ctx->line_pos] : NULL;

                    if(!real_comment) {
                        comment* new_comment = calloc(1, sizeof(comment));
                        new_comment->prefix = prefix;
                        new_comment->text = strdup(comment_text);
                        new_comment->size = strlen(new_comment->text);
                        new_comment->line = ctx->line_pos;
                        ini_add_comment(root, new_comment);
                    } else {
                        root->comments.array[real_comment->line] = NULL;
                        real_comment->line = ctx->line_pos;
                        root->comments.array[real_comment->line] = real_comment;
                        real_comment->seen = 1;
                        if(!cmp_string(real_comment->text, comment_text)) {
                            if(real_comment->text)
                                free(real_comment->text);
                            real_comment->text = strdup(comment_text);
                            real_comment->prefix = prefix;
                            real_comment->size = strlen(real_comment->text);
                            ini_run_updates(root, INI_ON_EDIT, NULL, NULL, real_comment);
                        }
                    }
                }
                goto end_loop;
            }
            default: {
                char* s_key = NULL;
                char* s_type = NULL;
                char* s_value = NULL;
                char* s_comment = NULL;
                char c_comment_prefix;

                parse_variable(tmp_line, &s_key, &s_type, &s_value, &s_comment, &c_comment_prefix);

                if(s_comment) {
                    comment* real_comment = root->comments.capacity > ctx->line_pos ?
                        root->comments.array[ctx->line_pos] : NULL;

                    if(!real_comment) {
                        comment* new_comment = calloc(1, sizeof(comment));
                        new_comment->text = s_comment;
                        new_comment->size = strlen(s_comment);
                        new_comment->prefix = c_comment_prefix;
                        new_comment->line = ctx->line_pos;
                        ini_add_comment(root, new_comment);
                    } else {
                        root->comments.array[real_comment->line] = NULL;
                        real_comment->line = ctx->line_pos;
                        root->comments.array[real_comment->line] = real_comment;
                        real_comment->seen = 1;
                        if(!cmp_string(real_comment->text, s_comment)) {
                            if(real_comment->text)
                                free(real_comment->text);
                            real_comment->text = s_comment;
                            real_comment->size = strlen(s_comment);
                            ini_run_updates(root, INI_ON_EDIT, NULL, NULL, real_comment);
                        }
                    }
                }

                if(!s_key || !s_value) {
                    if(stack->current) {
                        stack->stack[0] = NULL;
                        stack->size = 0;
                        stack->current = NULL;
                    }
                    goto end_loop;
                }

                variable* m_variable = NULL;
                char found = 1;
                if(stack->current)
                    m_variable = section_get_variable(stack->current, s_key);
                else m_variable = ini_get_variable(root, s_key);

                if(!m_variable) {
                    m_variable = ini_create_variable(s_key, s_value);
                    if(s_type)
                        m_variable->type = parse_type(s_type);
                    else m_variable->type = INI_STRING;
                    free(s_type);
                    found = 0;
                }

                m_variable->line = ctx->line_pos;

                if(check_multiline(s_value, &ctx->multi_line, &ctx->multi_end)) {
                    ctx->multi_key = s_key;
                    free(s_value);
                    goto end_loop;
                }

                m_variable->seen = 1;

                if(found) {
                    if(!cmp_string(m_variable->value, s_value)) {
                        if(m_variable->value)
                            free(m_variable->value);
                        m_variable->value = s_value;
                        ini_run_updates(root, INI_ON_EDIT, NULL, m_variable, NULL);
                    } else free(s_value);

                    VARIABLE_TYPE type = s_type ? parse_type(s_type) : INI_STRING;
                    if(m_variable->type != type) {
                        m_variable->type = type;
                        ini_run_updates(root, INI_ON_EDIT, NULL, m_variable, NULL);
                    }

                    free(s_type);
                }
                else {
                    if(!stack->current)
                        ini_add_variable(root, m_variable);
                    else
                        section_add_variable(stack->current, m_variable);
                    
                    free(s_value);
                    free(s_type);                    
                }
                free(s_key);
            }
        }
end_loop:
        free(tmp_line);
        tmp_line = NULL;
    }

    if(ctx->line)
        free(ctx->line);

    // ini_check_removed_comments(root);
    ini_check_removed_variables(root, &root->variables);
    ini_check_removed_sections(root, &root->sections);

    free_ini_context(ctx);
    fclose(file);
}

void* ini_update_handler(void* arg) {
    ini* root = (ini*)arg;
#ifdef _WIN32
    char dir[256];
    char fname[256];
    char ext[256];

    _splitpath_s(root->fp,
        NULL, 0,
        dir, sizeof(dir),
        fname, sizeof(fname),
        ext, sizeof(ext));

    char target[256];
    snprintf(target, sizeof(target), "%s%s", fname, ext);

    HANDLE hDir = CreateFile(
        dir,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    char buffer[4096];
    DWORD bytesReturned;

    for (;;) {
        if (!ReadDirectoryChangesW(
            hDir,
            buffer,
            sizeof(buffer),
            FALSE,
            FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytesReturned,
            NULL,
            NULL
        )) {
            printf("ReadDirectoryChangesW failed\n");
            break;
        }

        char changed = 0;
        FILE_NOTIFY_INFORMATION *fni = (FILE_NOTIFY_INFORMATION *)buffer;
        do {
            size_t filenameSize = fni->FileNameLength / sizeof(WCHAR);
            char narrowName[256];
            WideCharToMultiByte(CP_UTF8, 0,
                fni->FileName,
                filenameSize,
                narrowName, sizeof(narrowName),
                NULL, NULL);
            narrowName[filenameSize] = '\0';

            if (strcmp(narrowName, target) == 0)
                changed = 1;

            if (fni->NextEntryOffset == 0) break;
            fni = (FILE_NOTIFY_INFORMATION *)((char *)fni + fni->NextEntryOffset);
        } while (1);

        if(changed) {
            Sleep(100);
            ini_check_update(root);
        }
    }

    CloseHandle(hDir);
#else
    int fd = inotify_init1(IN_NONBLOCK);
    if(fd == -1) {
        fprintf(stderr, "Failed to init inotify\n");
        exit(EXIT_FAILURE);
    }

    int wd = inotify_add_watch(fd, root->fp, IN_CLOSE_WRITE);
    if(wd == -1) {
        fprintf(stderr, "Cannot watch '%s': %s\n\n", root->fp, strerror(errno));
        exit(EXIT_FAILURE);
    }

    int poll_num;
    nfds_t nfds;
    struct pollfd fds[2];

    nfds = 2;

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = fd;
    fds[1].events = POLLIN;

    char stdin_buf;

    for(;;) {
        poll_num = poll(fds, nfds, -1);
        if(poll_num == -1) {
            if(errno == EINTR)
                continue;

            fprintf(stderr, "poll\n");
            exit(EXIT_FAILURE);
        }

        if(poll_num < 1)
            continue;

        if(fds[0].revents & POLLIN) {
            while(read(STDIN_FILENO, &stdin_buf, 1) > 0 && stdin_buf != '\n')
                continue;
            break;
        }

        if(fds[1].revents & POLLIN) {
            char buf[INOTIFY_BUF_LEN * 16];
            struct inotify_event* event;

            for(;;) {    
                int size = read(fd, buf, sizeof(buf));
                if (size <= 0)
                    break;
            
                for (char* ptr = buf; ptr < buf + size;
                     ptr += sizeof(struct inotify_event) + event->len) {
                    event = (struct inotify_event*) ptr;
                    if (!(event->mask & IN_CLOSE_WRITE))
                        continue;
            
                    ini_check_update(root);
                }
            }
        }
    } 

    close(fd);
#endif

    return NULL;
}

#ifdef _WIN32
DWORD WINAPI ini_update_handler_win(LPVOID arg) {
    ini_update_handler(arg);
    return 0;
}
#endif

char ini_init_update(ini* root) {
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, ini_update_handler_win, root, 0, NULL);
    if(!thread) {
        fprintf(stderr, "Failed to create update thread\n");
        return 0;
    }

    CloseHandle(thread);
#else
    pthread_t thread;
    if(pthread_create(&thread, NULL, ini_update_handler, root) != 0) {
        fprintf(stderr, "Failed to create update thread\n");
        return 0;
    }

    pthread_detach(thread);
#endif

    return 1;
}


ini* ini_read(const char* fp) {
    char full_path[PATH_MAX];
    realpath(fp, full_path);

    FILE* file = fopen(full_path, "r");

    if(!file) {
        fprintf(stderr, "Failed to open %s\n", full_path);
        exit(EXIT_FAILURE);
    }

    ini* root = ini_create();
    root->fp = strdup(full_path);

    ini_context* ctx = create_ini_context();

    section_stack* stack = create_section_stack(ctx);

    while((ctx->line_size = getline_s(&ctx->line, &ctx->line_capacity, file)) != -1) {
        ++ctx->line_pos;
        if(ctx->multi_line) {
            int multi_size = (int)(ctx->multi_end - ctx->multi_line);
            if(multi_size + ctx->line_size > ctx->multi_line_capacity) {
                ctx->multi_line_capacity *= 2;
                char* tmp = realloc(ctx->multi_line, ctx->multi_line_capacity);
                if(!tmp) {
                    fprintf(stderr, "Failed to realloc multiline\n");
                    exit(EXIT_FAILURE);
                }
                ctx->multi_end = tmp + multi_size;
                ctx->multi_line = tmp;
            }

            memcpy(ctx->multi_end, ctx->line, ctx->line_size - 1);
            ctx->multi_end += ctx->line_size - 1;

            char end_1 = *(ctx->multi_end - 1);
            char end_2 = *(ctx->multi_end - 2);
            char is_multiline = (end_1 == '\\' && end_2 != '\\'
                && end_1 == '\\' && end_2 == ' ')
                || (end_1 == ' ' && end_2 == '\\');

            if(!is_multiline) {
                variable* var = ini_create_variable(ctx->multi_key, ctx->multi_line);
                var->multi_line = 1;

                free(ctx->multi_key);
                free(ctx->multi_line);

                if(!stack->current)
                    ini_add_variable(root, var);
                else
                    section_add_variable(stack->current, var);

                ctx->multi_line = NULL;
                ctx->multi_end = NULL;
                ctx->multi_key = NULL;
                ctx->multi_line_capacity = MULTI_LINE_CAPACITY;
            } else *(ctx->multi_end - 1) = '\n';

            continue;
        }

        size_t tmp_line_size = 0;
        char* tmp_line = trim_string(ctx->line, &tmp_line_size);
        switch(*tmp_line) {
            case '#':
            case ';': {
                comment* m_comment = calloc(1, sizeof(comment));

                char* s_comment = calloc(1, tmp_line_size);
                memcpy(s_comment, tmp_line + 1, tmp_line_size - 1);

                m_comment->prefix = *tmp_line;
                m_comment->text = s_comment;
                m_comment->size = strlen(s_comment);
                m_comment->line = ctx->line_pos;

                ini_add_comment(root, m_comment);

                goto end_loop;
            }
            case '[': {
                    char name[1024];
                    int name_size = 0;
                    while(name_size < 1024) {
                        char copy_chr = tmp_line[name_size + 1];
                        if(!copy_chr || copy_chr == ']')
                            break;
                        name[name_size++] = copy_chr;
                    }
                    name[name_size] = '\0';

                    stack->current = NULL;

                    if(*name == '.') {
                        handle_sub_section(root, ctx, name);
                        stack->current->line = ctx->line_pos;
                    } else {
                        stack->size = 0;
                        stack->current = ini_create_section(name);
                        stack->current->line = ctx->line_pos;
                        stack->stack[stack->size++] = stack->current;
                        ini_add_section(root, stack->current);
                    }

                    char prefix;
                    char* s_comment = trim_comment(tmp_line + name_size, &prefix);
                    if(s_comment) {
                        comment* m_comment = calloc(1, sizeof(comment));
                
                        m_comment->text = strdup(s_comment);
                        m_comment->size = strlen(m_comment->text);
                        m_comment->prefix = prefix;
                        m_comment->line = ctx->line_pos;

                        ini_add_comment(root, m_comment);
                    }
                }

                goto end_loop;
            default: {
                    char* s_key = NULL;
                    char* s_type = NULL;
                    char* s_value = NULL;
                    char* s_comment = NULL;
                    char c_comment_prefix;

                    parse_variable(tmp_line, &s_key, &s_type, &s_value, &s_comment, &c_comment_prefix);

                    comment* m_comment = NULL;
                    if(s_comment) {
                        m_comment = calloc(1, sizeof(comment));
                        m_comment->prefix = c_comment_prefix;
                        m_comment->line = ctx->line_pos;
                        m_comment->text = s_comment;
                        m_comment->size = strlen(s_comment);
                        
                        ini_add_comment(root, m_comment);
                    }

                    if(!s_key || !s_value) {
                        if(stack->current) {
                            stack->stack[0] = NULL;
                            stack->size = 0;
                            stack->current = NULL;
                        }
                        goto end_loop;
                    }

                    variable* m_variable = ini_create_variable(s_key, s_value);

                    if(s_type)
                        m_variable->type = parse_type(s_type);
                    else m_variable->type = INI_STRING;

                    free(s_type);

                    if(check_multiline(s_value, &ctx->multi_line, &ctx->multi_end)) {
                        ctx->multi_key = s_key;
                        free(s_value);
                        goto end_loop;
                    }

                    free(s_value);
                    free(s_key);

                    m_variable->line = ctx->line_pos;

                    if(!stack->current) 
                        ini_add_variable(root, m_variable);
                    else
                        section_add_variable(stack->current, m_variable);
                }
                goto end_loop;
        }
end_loop:
        free(tmp_line);
        tmp_line = NULL;
    }
    if(ctx->line)
        free(ctx->line);

    free_ini_context(ctx);
    fclose(file);

    return root;
}