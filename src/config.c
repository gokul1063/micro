#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static EditorConfig g_config;
static char *g_json_buffer = NULL;

static int strcasecmp_custom(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

static int strncasecmp_custom(const char *s1, const char *s2, size_t n) {
    while (n-- && *s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    return 0;
}

static char* json_skip_whitespace(char *p) {
    while (*p && isspace(*p)) p++;
    return p;
}

static char* json_parse_string(char *p, char *out, int max_len) {
    p = json_skip_whitespace(p);
    if (*p != '"') return NULL;
    p++;
    int i = 0;
    while (*p && *p != '"' && i < max_len - 1) {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n': out[i++] = '\n'; break;
                case 't': out[i++] = '\t'; break;
                case 'r': out[i++] = '\r'; break;
                case '\\': out[i++] = '\\'; break;
                case '"': out[i++] = '"'; break;
                default: out[i++] = *p; break;
            }
        } else {
            out[i++] = *p;
        }
        p++;
    }
    out[i] = '\0';
    if (*p == '"') p++;
    return p;
}

static char* json_parse_int(char *p, int *out) {
    p = json_skip_whitespace(p);
    *out = atoi(p);
    while (*p && (isdigit(*p) || *p == '-')) p++;
    return p;
}

static char* json_parse_bool(char *p, bool *out) {
    p = json_skip_whitespace(p);
    if (strncmp(p, "true", 4) == 0) {
        *out = true;
        return p + 4;
    } else if (strncmp(p, "false", 5) == 0) {
        *out = false;
        return p + 5;
    }
    return NULL;
}

static char* json_find_key(char *p, const char *key) {
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    char *found = strstr(p, search_key);
    if (found) {
        found += strlen(search_key);
        found = json_skip_whitespace(found);
        if (*found == ':') return found + 1;
    }
    return NULL;
}

static void config_parse_settings(char *json) {
    char *p = json_find_key(json, "settings");
    if (!p) return;

    p = json_skip_whitespace(p);
    if (*p != '{') return;
    p++;

    char *tab_stop = json_find_key(p, "tab_stop");
    if (tab_stop) json_parse_int(tab_stop, &g_config.settings.tab_stop);

    char *show_ln = json_find_key(p, "show_line_numbers");
    if (show_ln) json_parse_bool(show_ln, &g_config.settings.show_line_numbers);

    char *hl_line = json_find_key(p, "highlight_current_line");
    if (hl_line) json_parse_bool(hl_line, &g_config.settings.highlight_current_line);

    char *auto_indent = json_find_key(p, "auto_indent");
    if (auto_indent) json_parse_bool(auto_indent, &g_config.settings.auto_indent);

    char *enable_mouse = json_find_key(p, "enable_mouse");
    if (enable_mouse) json_parse_bool(enable_mouse, &g_config.settings.enable_mouse);

    char *theme = json_find_key(p, "theme");
    if (theme) json_parse_string(theme, g_config.settings.theme, sizeof(g_config.settings.theme));
}

static void config_parse_normal_keys(char *json) {
    char *p = json_find_key(json, "normal_mode");
    if (!p) return;

    p = json_skip_whitespace(p);
    if (*p != '{') return;
    p++;

    #define PARSE_KEY(field, keyname) \
        do { \
            char *found = json_find_key(p, keyname); \
            if (found) json_parse_string(found, g_config.normal_keys.field, sizeof(g_config.normal_keys.field)); \
        } while(0)

    PARSE_KEY(normal_quit, "quit");
    PARSE_KEY(normal_save, "save");
    PARSE_KEY(normal_find, "find");
    PARSE_KEY(normal_move_up, "move_up");
    PARSE_KEY(normal_move_down, "move_down");
    PARSE_KEY(normal_move_left, "move_left");
    PARSE_KEY(normal_move_right, "move_right");
    PARSE_KEY(normal_move_line_start, "move_line_start");
    PARSE_KEY(normal_move_line_end, "move_line_end");
    PARSE_KEY(normal_page_up, "page_up");
    PARSE_KEY(normal_page_down, "page_down");
    PARSE_KEY(normal_enter_insert, "enter_insert_mode");
    PARSE_KEY(normal_enter_insert_after, "enter_insert_mode_after");
    PARSE_KEY(normal_enter_insert_line_start, "enter_insert_mode_line_start");
    PARSE_KEY(normal_enter_insert_line_end, "enter_insert_mode_line_end");
    PARSE_KEY(normal_enter_insert_new_line_below, "enter_insert_mode_new_line_below");
    PARSE_KEY(normal_enter_insert_new_line_above, "enter_insert_mode_new_line_above");
    PARSE_KEY(normal_delete_char, "delete_char");
    PARSE_KEY(normal_delete_line, "delete_line");
    PARSE_KEY(normal_undo, "undo");
    PARSE_KEY(normal_redo, "redo");
    PARSE_KEY(normal_search_forward, "search_forward");
    PARSE_KEY(normal_search_backward, "search_backward");
    PARSE_KEY(normal_next_match, "next_match");
    PARSE_KEY(normal_prev_match, "prev_match");
    PARSE_KEY(normal_goto_line, "goto_line");
    PARSE_KEY(normal_copy_line, "copy_line");
    PARSE_KEY(normal_paste_after, "paste_after");
    PARSE_KEY(normal_paste_before, "paste_before");

    #undef PARSE_KEY
}

static void config_parse_insert_keys(char *json) {
    char *p = json_find_key(json, "insert_mode");
    if (!p) return;

    p = json_skip_whitespace(p);
    if (*p != '{') return;
    p++;

    #define PARSE_KEY(field, keyname) \
        do { \
            char *found = json_find_key(p, keyname); \
            if (found) json_parse_string(found, g_config.insert_keys.field, sizeof(g_config.insert_keys.field)); \
        } while(0)

    PARSE_KEY(exit_insert, "exit_insert_mode");
    PARSE_KEY(save, "save");
    PARSE_KEY(newline, "newline");
    PARSE_KEY(backspace, "backspace");
    PARSE_KEY(delete_key, "delete");
    PARSE_KEY(move_up, "move_up");
    PARSE_KEY(move_down, "move_down");
    PARSE_KEY(move_left, "move_left");
    PARSE_KEY(move_right, "move_right");

    #undef PARSE_KEY
}

static char* json_parse_color(char *p, RGB *out) {
    p = json_skip_whitespace(p);
    if (*p == '"') {
        char str[16];
        p = json_parse_string(p, str, sizeof(str));
        if (str[0] == '#' && strlen(str) == 7) {
            unsigned int r, g, b;
            if (sscanf(str, "#%02x%02x%02x", &r, &g, &b) == 3) {
                out->r = (int)r;
                out->g = (int)g;
                out->b = (int)b;
            }
        } else {
            out->r = out->g = out->b = -1;
        }
    }
    return p;
}

static void config_parse_colors(char *json) {
    char *p = json_find_key(json, "colors");
    if (!p) return;

    char *theme_name = g_config.settings.theme;
    char *theme_obj = json_find_key(p, theme_name);
    if (!theme_obj) {
        theme_obj = json_find_key(p, "default");
    }
    if (!theme_obj) return;

    theme_obj = json_skip_whitespace(theme_obj);
    if (*theme_obj != '{') return;
    theme_obj++;

    #define PARSE_COLOR(field, keyname) \
        do { \
            char *found = json_find_key(theme_obj, keyname); \
            if (found) json_parse_color(found, &g_config.colors.field); \
        } while(0)

    PARSE_COLOR(status_bar_bg, "status_bar_bg");
    PARSE_COLOR(status_bar_fg, "status_bar_fg");
    PARSE_COLOR(line_numbers_bg, "line_numbers_bg");
    PARSE_COLOR(line_numbers_fg, "line_numbers_fg");
    PARSE_COLOR(keyword1, "keyword1");
    PARSE_COLOR(keyword2, "keyword2");
    PARSE_COLOR(string, "string");
    PARSE_COLOR(number, "number");
    PARSE_COLOR(comment, "comment");
    PARSE_COLOR(multiline_comment, "multiline_comment");
    PARSE_COLOR(match, "match");

    #undef PARSE_COLOR
}

static void config_set_defaults(void) {
    g_config.settings.tab_stop = 4;
    g_config.settings.show_line_numbers = true;
    g_config.settings.highlight_current_line = true;
    g_config.settings.auto_indent = true;
    g_config.settings.enable_mouse = true;
    strcpy(g_config.settings.theme, "opencode");

    strcpy(g_config.normal_keys.normal_quit, "Ctrl-q");
    strcpy(g_config.normal_keys.normal_save, "Ctrl-s");
    strcpy(g_config.normal_keys.normal_find, "Ctrl-f");
    strcpy(g_config.normal_keys.normal_move_up, "k");
    strcpy(g_config.normal_keys.normal_move_down, "j");
    strcpy(g_config.normal_keys.normal_move_left, "h");
    strcpy(g_config.normal_keys.normal_move_right, "l");
    strcpy(g_config.normal_keys.normal_move_line_start, "0");
    strcpy(g_config.normal_keys.normal_move_line_end, "$");
    strcpy(g_config.normal_keys.normal_page_up, "Ctrl-u");
    strcpy(g_config.normal_keys.normal_page_down, "Ctrl-d");
    strcpy(g_config.normal_keys.normal_enter_insert, "i");
    strcpy(g_config.normal_keys.normal_enter_insert_after, "a");
    strcpy(g_config.normal_keys.normal_enter_insert_line_start, "I");
    strcpy(g_config.normal_keys.normal_enter_insert_line_end, "A");
    strcpy(g_config.normal_keys.normal_enter_insert_new_line_below, "o");
    strcpy(g_config.normal_keys.normal_enter_insert_new_line_above, "O");
    strcpy(g_config.normal_keys.normal_delete_char, "x");
    strcpy(g_config.normal_keys.normal_delete_line, "dd");
    strcpy(g_config.normal_keys.normal_undo, "u");
    strcpy(g_config.normal_keys.normal_redo, "Ctrl-r");
    strcpy(g_config.normal_keys.normal_search_forward, "/");
    strcpy(g_config.normal_keys.normal_search_backward, "?");
    strcpy(g_config.normal_keys.normal_next_match, "n");
    strcpy(g_config.normal_keys.normal_prev_match, "N");
    strcpy(g_config.normal_keys.normal_goto_line, ":");
    strcpy(g_config.normal_keys.normal_copy_line, "yy");
    strcpy(g_config.normal_keys.normal_paste_after, "p");
    strcpy(g_config.normal_keys.normal_paste_before, "P");

    strcpy(g_config.insert_keys.exit_insert, "Escape");
    strcpy(g_config.insert_keys.save, "Ctrl-s");
    strcpy(g_config.insert_keys.newline, "Enter");
    strcpy(g_config.insert_keys.backspace, "Backspace");
    strcpy(g_config.insert_keys.delete_key, "Delete");
    strcpy(g_config.insert_keys.move_up, "Up");
    strcpy(g_config.insert_keys.move_down, "Down");
    strcpy(g_config.insert_keys.move_left, "Left");
    strcpy(g_config.insert_keys.move_right, "Right");

    /* opencode default (dark) palette */
    g_config.colors.background.r = 0x0a; g_config.colors.background.g = 0x0a; g_config.colors.background.b = 0x0a;
    g_config.colors.foreground.r = 0xee; g_config.colors.foreground.g = 0xee; g_config.colors.foreground.b = 0xee;
    g_config.colors.status_bar_bg.r = 0xfa; g_config.colors.status_bar_bg.g = 0xb2; g_config.colors.status_bar_bg.b = 0x83;
    g_config.colors.status_bar_fg.r = 0x0a; g_config.colors.status_bar_fg.g = 0x0a; g_config.colors.status_bar_fg.b = 0x0a;
    g_config.colors.line_numbers_bg.r = 0x14; g_config.colors.line_numbers_bg.g = 0x14; g_config.colors.line_numbers_bg.b = 0x14;
    g_config.colors.line_numbers_fg.r = 0x80; g_config.colors.line_numbers_fg.g = 0x80; g_config.colors.line_numbers_fg.b = 0x80;
    g_config.colors.keyword1.r = 0x9d; g_config.colors.keyword1.g = 0x7c; g_config.colors.keyword1.b = 0xd8;
    g_config.colors.keyword2.r = 0xe5; g_config.colors.keyword2.g = 0xc0; g_config.colors.keyword2.b = 0x7b;
    g_config.colors.string.r = 0x7f; g_config.colors.string.g = 0xd8; g_config.colors.string.b = 0x8f;
    g_config.colors.number.r = 0xf5; g_config.colors.number.g = 0xa7; g_config.colors.number.b = 0x42;
    g_config.colors.comment.r = 0x80; g_config.colors.comment.g = 0x80; g_config.colors.comment.b = 0x80;
    g_config.colors.multiline_comment.r = 0x80; g_config.colors.multiline_comment.g = 0x80; g_config.colors.multiline_comment.b = 0x80;
    g_config.colors.match.r = 0xfa; g_config.colors.match.g = 0xb2; g_config.colors.match.b = 0x83;
}

void config_init(void) {
    config_set_defaults();
}

void config_load(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return;

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    g_json_buffer = malloc(len + 1);
    fread(g_json_buffer, 1, len, fp);
    g_json_buffer[len] = '\0';
    fclose(fp);

    config_parse_settings(g_json_buffer);
    config_parse_normal_keys(g_json_buffer);
    config_parse_insert_keys(g_json_buffer);
    config_parse_colors(g_json_buffer);
}

void config_free(void) {
    free(g_json_buffer);
    g_json_buffer = NULL;
}

EditorConfig* config_get(void) {
    return &g_config;
}

int config_key_to_code(const char *key) {
    if (strcasecmp_custom(key, "Escape") == 0 || strcasecmp_custom(key, "Esc") == 0) return '\x1b';
    if (strcasecmp_custom(key, "Enter") == 0) return '\r';
    if (strcasecmp_custom(key, "Backspace") == 0) return 127;
    if (strcasecmp_custom(key, "Delete") == 0) return 1003;
    if (strcasecmp_custom(key, "Up") == 0) return 1000;
    if (strcasecmp_custom(key, "Down") == 0) return 1001;
    if (strcasecmp_custom(key, "Left") == 0) return 1002;
    if (strcasecmp_custom(key, "Right") == 0) return 1003;
    if (strcasecmp_custom(key, "PageUp") == 0 || strcasecmp_custom(key, "Page_Up") == 0) return 1004;
    if (strcasecmp_custom(key, "PageDown") == 0 || strcasecmp_custom(key, "Page_Down") == 0) return 1005;
    if (strcasecmp_custom(key, "Home") == 0) return 1006;
    if (strcasecmp_custom(key, "End") == 0) return 1007;
    if (strcasecmp_custom(key, "Tab") == 0) return '\t';
    if (strcasecmp_custom(key, "Space") == 0) return ' ';

    if (strncasecmp_custom(key, "Ctrl-", 5) == 0) {
        char c = key[5];
        if (c >= 'a' && c <= 'z') return c - 'a' + 1;
        if (c >= 'A' && c <= 'Z') return c - 'A' + 1;
    }

    if (strlen(key) == 1) return key[0];

    return -1;
}