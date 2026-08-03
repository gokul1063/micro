#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_VISUAL
} EditorMode;

typedef struct {
    int tab_stop;
    bool show_line_numbers;
    bool highlight_current_line;
    bool auto_indent;
    char theme[32];
} EditorSettings;

typedef struct {
    char normal_quit[32];
    char normal_save[32];
    char normal_find[32];
    char normal_move_up[32];
    char normal_move_down[32];
    char normal_move_left[32];
    char normal_move_right[32];
    char normal_move_line_start[32];
    char normal_move_line_end[32];
    char normal_page_up[32];
    char normal_page_down[32];
    char normal_enter_insert[32];
    char normal_enter_insert_after[32];
    char normal_enter_insert_line_start[32];
    char normal_enter_insert_line_end[32];
    char normal_enter_insert_new_line_below[32];
    char normal_enter_insert_new_line_above[32];
    char normal_delete_char[32];
    char normal_delete_line[32];
    char normal_undo[32];
    char normal_redo[32];
    char normal_search_forward[32];
    char normal_search_backward[32];
    char normal_next_match[32];
    char normal_prev_match[32];
    char normal_goto_line[32];
    char normal_copy_line[32];
    char normal_paste_after[32];
    char normal_paste_before[32];
} NormalKeybindings;

typedef struct {
    char exit_insert[32];
    char save[32];
    char newline[32];
    char backspace[32];
    char delete_key[32];
    char move_up[32];
    char move_down[32];
    char move_left[32];
    char move_right[32];
} InsertKeybindings;

typedef struct {
    int background;
    int foreground;
    int status_bar_bg;
    int status_bar_fg;
    int line_numbers_bg;
    int line_numbers_fg;
    int keyword1;
    int keyword2;
    int string;
    int number;
    int comment;
    int multiline_comment;
    int match;
} ColorTheme;

typedef struct {
    EditorSettings settings;
    NormalKeybindings normal_keys;
    InsertKeybindings insert_keys;
    ColorTheme colors;
} EditorConfig;

void config_init(void);
void config_load(const char *filename);
void config_free(void);
EditorConfig* config_get(void);
int config_key_to_code(const char *key);
int config_parse_color(const char *color_str);

#endif