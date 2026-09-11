#include "ini/types.h"
#include "ini/reader.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

char** ini_deserialize_array(char* value, size_t* size) {
    char val[MAX_ARRAY_ITEM_SIZE];
    char* val_cursor = val;
    char** result = malloc(sizeof(char*) * MAX_ARRAY_SIZE);
    char quote_char = 0;
    char* start_value = value;

    *size = 0;

    while(*value) {
        if(!quote_char && (*value == '"' || *value == '\'') && (value == start_value || *(value - 1) != '\\')) {
            value++;
            continue;
        }

        if(quote_char && *value == quote_char && (value == start_value || *(value - 1) != '\\')) {
            value++;
            continue;
        }

        if(*value == ',' && !quote_char) {
            *val_cursor = '\0';

            if(*size < MAX_ARRAY_SIZE)
                result[(*size)++] = strdup(val);

            val_cursor = val;
            value++;

            while(isspace(*value)) value++;

            continue;
        }

        if(val_cursor - val < MAX_ARRAY_ITEM_SIZE - 1)
            *val_cursor++ = *value;

        value++;
    }

    if(val_cursor != val && *size < MAX_ARRAY_SIZE) {
        *val_cursor = '\0';
        result[(*size)++] = strdup(val);
    }

    return result;
}

char* ini_serialize_array(char** array, size_t size) {
    if(!array || size == 0)
        return NULL;

    char* value = malloc(MAX_ARRAY_BUFFER);
    if(!value)
        return NULL;

    size_t offset = 0; 
    for(int i = 0; i < size; ++i) {
        if(i > 0)
            offset += snprintf(value + offset, MAX_ARRAY_BUFFER - offset, ", ");

        offset += snprintf(value + offset, MAX_ARRAY_BUFFER - offset, "\"%s\"", array[i]);

        if(offset >= MAX_ARRAY_BUFFER) break;
    }

    return value;
}

void ini_free_array(char** array, size_t size) {
    for(int i = 0; i < size; ++i)
        free(array[i]);

    free(array);
}

void ini_free_ptr(void* ptr) {
    free(ptr);
}

char* ini_deserialize_string(const char* value) {
    char val[MAX_STRING_SIZE] = {0};
    char* val_cursor = val;
    const char* cursor = value;
    char quote_char = 0;

    while (*cursor) {
        if (*cursor == '\\' && cursor[1] != '\0') {
            ++cursor;
            switch (*cursor) {
                case 'n':  *val_cursor++ = '\n'; break;
                case 't':  *val_cursor++ = '\t'; break;
                case 'r':  *val_cursor++ = '\r'; break;
                case '\\': *val_cursor++ = '\\'; break;
                case '\'': *val_cursor++ = '\''; break;
                case '"':  *val_cursor++ = '"';  break;
                default:   *val_cursor++ = *cursor; break;
            }
            ++cursor;
            continue;
        }

        if (!quote_char && (*cursor == '"' || *cursor == '\'')) {
            quote_char = *cursor++;
            continue;
        }

        if (quote_char && *cursor == quote_char) {
            ++cursor;
            continue;
        }

        if (val_cursor - val < (ptrdiff_t)sizeof(val) - 1)
            *val_cursor++ = *cursor;
        ++cursor;
    }

    return strdup(val);
}

char* ini_serialize_string(const char* value) {
    char val[MAX_STRING_SIZE];
    char* val_cursor = val;
    *val_cursor++ = '\'';

    const char* cursor = value;

    while(*cursor) {
        if(val_cursor - val >= sizeof(val) - 2)
            break;

        if(*cursor == '\'')
            *val_cursor++ = '\\';

        *val_cursor++ = *cursor++;
    }

    *val_cursor++ = '\'';
    *val_cursor = '\0';

    return strdup(val);
}

resolve_result ini_resolve_path(ini* root, const char* path) {
    section* parent = NULL;
    resolve_result result = {
        .ptr = NULL,
        .type = INI_TYPE_NONE
    };

    char name[512];
    const char* path_cursor = path;

    while (*path_cursor) {
        char* name_end = strchr(path_cursor, '.');
        if(name_end) {
            size_t name_len = name_end - path_cursor;
            memcpy(name, path_cursor, name_len);
            name[name_len] = '\0';
            path_cursor += name_len;

            if(*++path_cursor == '\0')
                goto finish;

            parent = parent ?
                section_get_section(parent, name) :
                ini_get_section(root, name);

            continue;
        } else {
            variable* var = parent ?
                section_get_variable(parent, path_cursor) :
                ini_get_variable(root, path_cursor);

            if(var) {
                result.ptr = var;
                result.type = INI_TYPE_VARIABLE;
                goto finish;
            }

            section* sect = parent ?
                section_get_section(parent, path_cursor) :
                ini_get_section(root, path_cursor);

            if(sect) {
                result.ptr = sect;
                result.type = INI_TYPE_SECTION;
                goto finish;
            }

            goto finish;
        }
    }

finish:
    return result;
}

resolve_result section_resolve_path(section* parent, const char* path) {
    resolve_result result = {
        .ptr = NULL,
        .type = INI_TYPE_NONE
    };

    char name[512];
    const char* path_cursor = path;

    while (*path_cursor) {
        char* name_end = strchr(path_cursor, '.');
        if(name_end) {
            size_t name_len = name_end - path_cursor;
            memcpy(name, path_cursor, name_len);
            name[name_len] = '\0';
            path_cursor += name_len;

            if(*++path_cursor == '\0')
                goto finish;

            parent = section_get_section(parent, name);
            continue;
        } else {
            variable* var = section_get_variable(parent, path_cursor);

            if(var) {
                result.ptr = var;
                result.type = INI_TYPE_VARIABLE;
                goto finish;
            }

            section* sect = section_get_section(parent, path_cursor);
            if(sect) {
                result.ptr = sect;
                result.type = INI_TYPE_SECTION;
                goto finish;
            }

            goto finish;
        }
    }

finish:
    return result;
}

char* ini_resolve_expression(variable* self, char* expression) {
    variable* var = NULL;

    if(*expression != '.') {
        resolve_result result = ini_resolve_path(self->root, expression);

        if(result.type != INI_TYPE_VARIABLE)
            return strdup(expression);

        var = result.ptr;
    } else {
        char* cursor = expression;

        section* parent = self->parent;
        ini* root = NULL;

        while(*++cursor == '.') {
            if(parent->parent)
                parent = parent->parent;
            else {
                root = parent->root;
                var = ini_get_variable(root, cursor);
                goto finish;
            }
        }

        resolve_result result = section_resolve_path(parent, cursor);

        if(result.type != INI_TYPE_VARIABLE)
            return strdup(expression);

        var = result.ptr;
    }

finish:
    if(!var)
            return strdup(expression);

    return variable_read_string(var);
}

char* ini_format_string(variable* var, char* value) {
    char val[MAX_STRING_SIZE] = {0};
    char* val_cursor = val;

    char expression[MAX_STRING_SIZE] = {0};
    char* expression_cursor = expression;
    int is_expression = 0;

    char* value_start = value;

    while(*value) {
        if(!CHECK_ESC(value, value_start) &&
            (*value == '{' || *value == '}')) {
            if(val_cursor > val)
                --val_cursor;

            goto write_line;
        }

        if(*value == '{') {
            is_expression = 1;
            ++value;
            continue;
        }

        if(is_expression && *value == '}') {
            char* parsed = ini_resolve_expression(var, expression);
            if(parsed) {
                size_t len = strlen(parsed);
                size_t remaining = sizeof(val) - (val_cursor - val) - 1;
                size_t copy_len = len < remaining ? len : remaining;
                memcpy(val_cursor, parsed, copy_len);
                val_cursor += copy_len;
                free(parsed);
            }

            is_expression = 0;
            memset(expression, 0, sizeof(expression));
            expression_cursor = expression;
            ++value;
            continue;
        }
write_line:
        if(is_expression &&
            expression_cursor - expression < sizeof(expression) - 1)
            *expression_cursor++ = *value;
        else if(val_cursor - val < sizeof(val) - 1)
            *val_cursor++ = *value;

        ++value;
    }

    return strdup(val);
}

char* variable_read_string(variable* var) {
    char* str = ini_format_string(var, var->value);
    char* result = ini_deserialize_string(str);
    free(str);
    return result;
}

char variable_read_bool(variable* var) {
    char* value = var->value;
    
    if(strcmp(value, "true") == 0)
        return 1;

    if(strcmp(value, "false") == 0)
        return 0;

    return 0;
}

int variable_read_int(variable* var, int default_value) {
    char* end;
    long result = strtol(var->value, &end, 10);
    if(end == var->value)
        return default_value;

    return (int)result;
}

double variable_read_double(variable* var, double default_value) {
    char* end;
    double result = strtod(var->value, &end);
    if(end == var->value)
        return default_value;
 
    return result;
}

float variable_read_float(variable* var, float default_value) {
    char* end;
    float result = strtof(var->value, &end);
    if(end == var->value)
        return default_value;
 
    return result;
}

char** variable_read_array(variable* var, size_t* size) {
    *size = 0;

    if(!var)
        return NULL;

    return ini_deserialize_array(var->value, size);
}

char ini_read_bool(ini* root, const char* key) {
    variable* var = ini_get_variable(root, key);
    if(!var)
        return 0;

    return variable_read_bool(var);
}

char* ini_read_string(ini* root, const char* key) {
    variable* var = ini_get_variable(root, key);
    if(!var)
        return NULL;

    return variable_read_string(var);
}

int ini_read_int(ini* root, const char* key, int default_value) {
    variable* var = ini_get_variable(root, key);    
    if(!var)
        return default_value;

    return variable_read_int(var, default_value);
}

double ini_read_double(ini* root, const char* key, double default_value) {
    variable* var = ini_get_variable(root, key);
    if(!var)
        return default_value;
 
    return variable_read_double(var, default_value);
}

float ini_read_float(ini* root, const char* key, float default_value) {
    variable* var = ini_get_variable(root, key);
    if(!var)
        return default_value;
 
    return variable_read_float(var, default_value);
}

char** ini_read_array(ini* root, const char* key, size_t* size) {
    variable* var = ini_get_variable(root, key);
    return variable_read_array(var, size);
}

char section_read_bool(section* root, const char* key) {
    variable* var = section_get_variable(root, key);
    if(!var)
        return 0;

    return variable_read_bool(var);
}

char* section_read_string(section* root, const char* key) {
    variable* var = section_get_variable(root, key);
    if(!var)
        return NULL;

    return variable_read_string(var);
}

int section_read_int(section* root, const char* key, int default_value) {
    variable* var = section_get_variable(root, key);    
    if(!var)
        return default_value;

    return variable_read_int(var, default_value);
}

double section_read_double(section* root, const char* key, double default_value) {
    variable* var = section_get_variable(root, key);
    if(!var)
        return default_value;
 
    return variable_read_double(var, default_value);
}

float section_read_float(section* root, const char* key, float default_value) {
    variable* var = section_get_variable(root, key);
    if(!var)
        return default_value;
 
    return variable_read_float(var, default_value);
}

char** section_read_array(section* root, const char* key, size_t* size) {
    variable* var = section_get_variable(root, key);
    return variable_read_array(var, size);
}

void variable_write_bool(variable* var, char value) {
    char* str_value = value ? "true" : "false";
    variable_set_value(var, str_value);
}

void variable_write_string(variable* var, const char* value) {
    char* parsed_value = ini_serialize_string(value);
    free(var->value);
    var->type = INI_STRING;
    var->value = parsed_value;
}

void variable_write_int(variable* var, int value) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%d", value);
    var->type = INI_INT;
    variable_set_value(var, buf);
}

void variable_write_double(variable* var, double value) {
    char buf[256];
    var->type = INI_DOUBLE;
    snprintf(buf, sizeof(buf), "%.3f", value);
    variable_set_value(var, buf);
}

void variable_write_float(variable* var, float value) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%.3f", value);
    var->type = INI_FLOAT;
    variable_set_value(var, buf);
}

void variable_write_array(variable* var, char** array, size_t size) {
    char* value = ini_serialize_array(array, size);
    var->type = INI_LIST;
    variable_set_value(var, value);
    free(value);
}

void ini_write_bool(ini* root, const char* key, char value) {
    variable* var = ini_get_variable(root, key);
    if(!var) {
        char* str_value = value ? "true" : "false";

        var = ini_create_variable(key, str_value);
        var->type = INI_BOOL;

        ini_add_variable(root, var);
        return;
    }

    variable_write_bool(var, value);
}

void ini_write_string(ini* root, const char* key, const char* value) {
    variable* var = ini_get_variable(root, key);
    if(!var) {
        char* parsed_value = ini_serialize_string(value);

        var = ini_create_variable(key, parsed_value);
        var->type = INI_STRING;

        ini_add_variable(root, var);

        free(parsed_value);
        return;
    }

    variable_write_string(var, value);
}

void ini_write_int(ini* root, const char* key, int value) {
    variable* var = ini_get_variable(root, key);
    if(!var) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%d", value);

        var = ini_create_variable(key, buf);
        var->type = INI_INT;

        ini_add_variable(root, var);
        return;
    }

    variable_write_int(var, value);
}

void ini_write_double(ini* root, const char* key, double value) {
    variable* var = ini_get_variable(root, key);
    if(!var) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%.3f", value);
        var = ini_create_variable(key, buf);
        var->type = INI_DOUBLE;
        ini_add_variable(root, var);
        return;
    }

    variable_write_double(var, value);
}

void ini_write_float(ini* root, const char* key, float value) {
    variable* var = ini_get_variable(root, key);
    if(!var) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%.3f", value);
        var = ini_create_variable(key, buf);
        var->type = INI_FLOAT;
        ini_add_variable(root, var);
        return;
    }

    variable_write_float(var, value);
}

void ini_write_array(ini* root, const char* key, char** array, size_t size) {
    if(size <= 0)
        return;

    variable* var = ini_get_variable(root, key);
    if(!var) {
        char* value = ini_serialize_array(array, size);

        var = ini_create_variable(key, value);
        var->type = INI_LIST;
        ini_add_variable(root, var);
        return;
    }

    variable_write_array(var, array, size);
}

void section_write_bool(section* root, const char* key, char value) {
    variable* var = section_get_variable(root, key);
    if(!var) {
        char* str_value = value ? "true" : "false";

        var = ini_create_variable(key, str_value);
        var->type = INI_BOOL;

        section_add_variable(root, var);
        return;
    }

    variable_write_bool(var, value);
}

void section_write_string(section* root, const char* key, const char* value) {
    variable* var = section_get_variable(root, key);
    if(!var) {
        char* parsed_value = ini_serialize_string(value);

        var = ini_create_variable(key, parsed_value);
        var->type = INI_STRING;

        section_add_variable(root, var);

        free(parsed_value);
        return;
    }

    variable_write_string(var, value);
}

void section_write_int(section* root, const char* key, int value) {
    variable* var = section_get_variable(root, key);
    if(!var) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%d", value);
        var = ini_create_variable(key, buf);
        var->type = INI_INT;
        section_add_variable(root, var);
        return;
    }
    variable_write_int(var, value);
}

void section_write_double(section* root, const char* key, double value) {
    variable* var = section_get_variable(root, key);
    if(!var) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%.3f", value);
        var = ini_create_variable(key, buf);
        var->type = INI_DOUBLE;
        section_add_variable(root, var);
        return;
    }

    variable_write_double(var, value);
}

void section_write_float(section* root, const char* key, float value) {
    variable* var = section_get_variable(root, key);
    if(!var) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%.3f", value);
        var = ini_create_variable(key, buf);
        var->type = INI_FLOAT;
        section_add_variable(root, var);
        return;
    }
    var->type = INI_FLOAT;
    variable_write_float(var, value);
}

void section_write_array(section* root, const char* key, char** array, size_t size) {
    if(size <= 0)
        return;

    variable* var = section_get_variable(root, key);
    if(!var) {
        char* value = ini_serialize_array(array, size);

        var = ini_create_variable(key, value);
        var->type = INI_LIST;
        section_add_variable(root, var);
        return;
    }

    variable_write_array(var, array, size);    
}