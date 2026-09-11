#ifndef INI_TYPES_H_
#define INI_TYPES_H_

#include "ini/reader.h"

#define MAX_ARRAY_ITEM_SIZE 512
#define MAX_ARRAY_BUFFER    2048
#define MAX_ARRAY_SIZE      512

#define MAX_STRING_SIZE     2048

typedef enum resolve_type {
    INI_TYPE_NONE       = 1 << 0,
    INI_TYPE_VARIABLE   = 1 << 1,
    INI_TYPE_SECTION    = 1 << 2
} resolve_type;

typedef struct resolve_result {
    void* ptr;
    resolve_type type;
} resolve_result;

INI_API resolve_result  ini_resolve_path(ini* root, const char* path);
INI_API resolve_result  section_resolve_path(section* parent, const char* path);

INI_API void    ini_free_ptr(void* ptr);
INI_API void    ini_free_array(char** array, size_t size);

INI_API char**  ini_deserialize_array(char* value, size_t* size);
INI_API char*   ini_serialize_array(char** array, size_t size);

INI_API char*   ini_deserialize_string(const char* value);
INI_API char*   ini_serialize_string(const char* value);

INI_API char*   ini_resolve_expression(variable* self, char* expression);
INI_API char*   ini_format_string(variable* var, char* value);

INI_API char    ini_read_bool(ini* root, const char* key);
INI_API char*   ini_read_string(ini* root, const char* key);
INI_API int     ini_read_int(ini* root, const char* key, int default_value);
INI_API double  ini_read_double(ini* root, const char* key, double default_value);
INI_API float   ini_read_float(ini* root, const char* key, float default_value);
INI_API char**  ini_read_array(ini* root, const char* key, size_t* size);

INI_API void    ini_write_bool(ini* root, const char* key, char value);
INI_API void    ini_write_string(ini* root, const char* key, const char* value);
INI_API void    ini_write_int(ini* root, const char* key, int value);
INI_API void    ini_write_double(ini* root, const char* key, double value);
INI_API void    ini_write_float(ini* root, const char* key, float value);
INI_API void    ini_write_array(ini* root, const char* key, char** array, size_t size);

INI_API char    section_read_bool(section* root, const char* key);
INI_API char*   section_read_string(section* root, const char* key);
INI_API int     section_read_int(section* root, const char* key, int default_value);
INI_API double  section_read_double(section* root, const char* key, double default_value);
INI_API float   section_read_float(section* root, const char* key, float default_value);
INI_API char**  section_read_array(section* root, const char* key, size_t* size);

INI_API void    section_write_bool(section* root, const char* key, char value);
INI_API void    section_write_string(section* root, const char* key, const char* value);
INI_API void    section_write_int(section* root, const char* key, int value);
INI_API void    section_write_double(section* root, const char* key, double value);
INI_API void    section_write_float(section* root, const char* key, float value);
INI_API void    section_write_array(section* root, const char* key, char** array, size_t size);
INI_API void    section_write_array(section* root, const char* key, char** array, size_t size);

INI_API char*   variable_read_string(variable* var);
INI_API int     variable_read_int(variable* var, int default_value);
INI_API double  variable_read_double(variable* var, double default_value);
INI_API float   variable_read_float(variable* var, float default_value);
INI_API char**  variable_read_array(variable* var, size_t* size);
INI_API char    variable_read_bool(variable* var);

INI_API void    variable_write_bool(variable* var, char value);
INI_API void    variable_write_string(variable* var, const char* value);
INI_API void    variable_write_int(variable* var, int value);
INI_API void    variable_write_double(variable* var, double value);
INI_API void    variable_write_float(variable* var, float value);
INI_API void    variable_write_array(variable* var, char** array, size_t size);

#endif