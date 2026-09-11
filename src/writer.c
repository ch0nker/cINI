#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "ini/reader.h"
#include "ini/writer.h"

char* find_chr(char* str, char chr) {
    while(*str) {
        if(*str == chr)
            return str;
        str++;
    }

    return NULL;
}

void write_pending_comments(char** cursor, int* line_count, comment_list list, int until_line) {
    while(*line_count < until_line) {
        int current_line = *line_count;
        ++*line_count;

        if(list.capacity <= current_line) {
            *(*cursor)++ = '\n';
            continue;
        }

        comment* m_comment = list.array[current_line];
        if(!m_comment || !m_comment->text || m_comment->size == 0) {
            *(*cursor)++ = '\n';
            continue;
        }

        *(*cursor)++ = m_comment->prefix;
        memcpy(*cursor, m_comment->text, m_comment->size);
        *cursor += m_comment->size;
        *(*cursor)++ = '\n';
    }
}

void write_line(char* line, size_t line_size, char** cursor, int* line_count, comment_list list) {
    if(!line)
        goto skip_line;

    memcpy(*cursor, line, line_size);
    *cursor += line_size;

skip_line:
    int current_line = *line_count;

    if(list.capacity <= current_line)
        goto append_newline;

    comment* m_comment = list.array[current_line];
    if(!m_comment || !m_comment->text || m_comment->size == 0)
        goto append_newline;

append_comment: {
    char before_chr = *((*cursor) - 1);

    if(before_chr != ' ' && before_chr != '\t' && before_chr != '\n')
        *(*cursor)++ = ' ';
    *(*cursor)++ = m_comment->prefix;

    memcpy(*cursor, m_comment->text, m_comment->size);
    *cursor += m_comment->size;
}

append_newline:
    ++*line_count;
    *(*cursor)++ = '\n';
}

void get_type_name(char** out, VARIABLE_TYPE type) {
    *out = "string";
    if(type == INI_LIST)
        *out = "list";
    else if(type == INI_INT)
        *out = "int";
    else if(type == INI_DOUBLE)
        *out = "double";
    else if(type == INI_FLOAT)
        *out = "float";
    else if(type == INI_BOOL)
        *out = "bool";
}

void write_comments(ini* root, char** cursor, int* line_count, int start, int end) {
    for(int line = start; line < end; ++line) {
        comment* m_comment = root->comments.array[line];
        if(m_comment) {
            *((*cursor)++) = m_comment->prefix;
            memcpy(*cursor, m_comment->text, m_comment->size);
            *cursor += m_comment->size;
        }
        *((*cursor)++) = '\n';
    }
    *line_count = end;
}

char write_variables(comment_list list, char** cursor, int* line_count, variable_map map) {
    variable* var = map.head;
    while(var) {
        write_pending_comments(cursor, line_count, list, var->line);

        char* line = malloc(8192);
        char* multi_tmp = NULL;
        char* value = var->value;
        if(var->multi_line) {
            char* line_cursor = var->value;
            char* last_cursor = line_cursor;
            multi_tmp = malloc(2048);
            char* multi_cursor = multi_tmp;

            while((line_cursor = find_chr(line_cursor, '\n'))) {
                *line_cursor = '\0';
                int line_size = (line_cursor - last_cursor) - 1;
                memcpy(multi_cursor, last_cursor, line_size);
                multi_cursor += line_size;
                memcpy(multi_cursor, " \\\n", 3);
                multi_cursor += 3;
                line_cursor++;
                last_cursor = line_cursor;
                ++*line_count;
            }

            strcpy(multi_cursor, last_cursor);
            value = multi_tmp;
        }

        snprintf(line, 8192, "%s = %s", var->key, value);

        write_line(line, strlen(line), cursor, line_count, list);
        free(line);
        if(multi_tmp)
            free(multi_tmp);
        var = var->map_next;
    }

    return 1;
}

char write_sections(comment_list list, char** cursor, int* line_count, section_map map) {
    section* section = map.head;
    while(section) {
        write_pending_comments(cursor, line_count, list, section->line);

        char line[512];
        char prefix[256] = {0};

        int dots = section->level < 255 ? section->level : 255;
        for(int i = 0; i < dots; i++)
            prefix[i] = '.';

        snprintf(line, 512, "[%s%s]", prefix, section->name);
        write_line(line, strlen(line), cursor, line_count, list);

        write_variables(list, cursor, line_count, section->variables);
        write_sections(list, cursor, line_count, section->sections);

        section = section->map_next;
    }
    return 1;
}

size_t calc_variable_map_size(variable_map* map) {
    size_t size = 0;
    variable* var = map->head;
    while(var) {
        size += strlen(var->key) + strlen(var->value) + 4;
        var = var->map_next;
    }
    return size;
}

size_t calc_section_map_size(section_map* map) {
    size_t size = 0;
    section* m_section = map->head;
    while(m_section) {
        size += strlen(m_section->name) + 4;
        size += calc_variable_map_size(&m_section->variables);
        size += calc_section_map_size(&m_section->sections);
        size += 1;
        m_section = m_section->map_next;
    }
    return size;
}

size_t calc_comments_size(comment_list* list) {
    size_t size = 0;
    comment* m_comment = list->head;
    while(m_comment) {
        size += m_comment->size + 2;
        m_comment = m_comment->next;
    }
    return size;
}

void ini_write(ini* root, const char* fp) {
    char full_path[PATH_MAX];
    realpath(fp, full_path);

    FILE* file = fopen(fp, "w");
    if(!file) {
        perror("ini_write fopen");
        return;
    }

    size_t size = calc_variable_map_size(&root->variables)
        + calc_section_map_size(&root->sections)
        + calc_comments_size(&root->comments)
        + 128;
    char* output = malloc(size);

    char* cursor = output;
    int line_count = 0;

    section* start_section = root->sections.head;
    variable* start_variable = root->variables.head;
   
    int start_line = start_variable ? start_variable->line :
                     start_section ? start_section->line : 0;

    write_comments(root, &cursor, &line_count, 1, start_line);
    
    write_variables(root->comments, &cursor, &line_count, root->variables);
    if(root->variables.head)
        write_line(NULL, 0, &cursor, &line_count, root->comments);
    write_sections(root->comments, &cursor, &line_count, root->sections);

    if(line_count - 1 < root->comments.last_line)
        write_comments(root, &cursor, &line_count, line_count, root->comments.last_line + 1);

    *--cursor = '\0';

    fprintf(file, "%s", output);
    fclose(file);

    free(output);
}