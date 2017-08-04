//TODO(Chen): undo doesn't undo the "jk" stroke
//TODO(Chen): a better color scheme
//TODO(Chen): implement the dot command
//TODO(Chen): have a easier way to change keymaps easily (data-driven instead of code)
//TODO(Chen): fix the seek* operations that are 1 letter off for some reason

//TODO(Chen): BUGS:
//TODO(Chen): fix visual mode's weird behavior

#if !defined(FCODER_CUSTOM_BINDINGS)
#define FCODER_CUSTOM_BINDINGS

#include "4coder_default_include.cpp"

#define debug_print(str) print_message(app, str, (int32_t)strlen(str))

#define RIGHT true
#define LEFT false

CUSTOM_COMMAND_SIG(goto_begin_of_file)
{
    View_Summary ActiveView = get_active_view(app, AccessOpen|AccessProtected);
    
    Buffer_Seek NewCursorPos = {};
    NewCursorPos.type = buffer_seek_pos;
    NewCursorPos.pos = 0;
    
    view_set_cursor(app, &ActiveView, NewCursorPos, true);
}

CUSTOM_COMMAND_SIG(goto_end_of_file)
{
    View_Summary ActiveView = get_active_view(app, AccessOpen|AccessProtected);
    Buffer_Summary CurrentBuffer = get_buffer(app, ActiveView.buffer_id, AccessOpen|AccessProtected);
    
    Buffer_Seek NewCursorPos = {};
    NewCursorPos.type = buffer_seek_pos;
    NewCursorPos.pos = CurrentBuffer.size; 
    
    view_set_cursor(app, &ActiveView, NewCursorPos, true);
}

template <bool is_until_seek>
CUSTOM_COMMAND_SIG(seek_right_char)
{
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
    
    if (!in.abort)
    {
        int32_t access = AccessOpen | AccessProtected;
        View_Summary view = get_active_view(app, access);
        Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
        
        int32_t next_ret_pos, char_pos;
        
        buffer_seek_delimiter_forward(app, &buffer, view.cursor.pos+1, 
                                      (char)in.key.keycode, &char_pos);
        buffer_seek_delimiter_forward(app, &buffer, view.cursor.pos+1, 
                                      '\n', &next_ret_pos);
        if (next_ret_pos >= char_pos)
        {
            if (is_until_seek)
            {
                view_set_cursor(app, &view, seek_pos(char_pos), true);
            }
            else
            {
                view_set_cursor(app, &view, seek_pos(char_pos+1), true);
            }
        }
    }
}

template <bool is_until_seek>
CUSTOM_COMMAND_SIG(seek_left_char)
{
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
    
    if (!in.abort)
    {
        int32_t access = AccessOpen | AccessProtected;
        View_Summary view = get_active_view(app, access);
        Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
        
        int32_t last_ret_pos, char_pos;
        
        buffer_seek_delimiter_backward(app, &buffer, view.cursor.pos-1, 
                                       (char)in.key.keycode, &char_pos);
        buffer_seek_delimiter_backward(app, &buffer, view.cursor.pos-1, 
                                       '\n', &last_ret_pos);
        if (last_ret_pos <= char_pos)
        {
            if (is_until_seek)
            {
                view_set_cursor(app, &view, seek_pos(char_pos+1), true);
            }
            else
            {
                view_set_cursor(app, &view, seek_pos(char_pos), true);
            }
        }
    }
}

template <Custom_Command_Function seeker>
CUSTOM_COMMAND_SIG(visual_wrapper);

//
//
// Customization

struct Key_Movement
{
    char key;
    Custom_Command_Function *seeker;
    bool dir;
};

Key_Movement global_key_movements[] = {
    {'w', visual_wrapper<seek_alphanumeric_or_camel_right>, RIGHT},
    {'b', visual_wrapper<seek_alphanumeric_or_camel_left>, LEFT},
    {'W', visual_wrapper<seek_whitespace_right>, RIGHT},
    {'B', visual_wrapper<seek_whitespace_left>, LEFT},
    {'$', visual_wrapper<seek_end_of_line>, RIGHT},
    {'0', visual_wrapper<seek_beginning_of_line>, LEFT},
    {'[', visual_wrapper<seek_whitespace_up_end_line>, LEFT},
    {']', visual_wrapper<seek_whitespace_down_end_line>, RIGHT},
    {'g', visual_wrapper<goto_begin_of_file>, LEFT},
    {'G', visual_wrapper<goto_end_of_file>, RIGHT},
    {'t', visual_wrapper<seek_right_char<true> >, RIGHT},
    {'f', visual_wrapper<seek_right_char<false> >, RIGHT},
    {'T', visual_wrapper<seek_left_char<true> >, LEFT},
    {'F', visual_wrapper<seek_left_char<false> >, LEFT},
};

#define Insert_Color    0xff0000
#define Normal_Color    0x00ff00
#define Transient_Color 0x0000ff
#define Visual_Color    0x0000ff

//
//
// System

#include <stdio.h>
#include <stdarg.h>

inline int32_t
max(int32_t a, int32_t b)
{
    return a > b? a: b;
}

static struct
{
    bool is_in_visual_mode = false;
    int32_t highlight_marker = 0;
    
    char *clipboard_mem = 0;
    int32_t clipboard_size = 0;
    
    bool j_is_pressed = false;
    
    Custom_Command_Function *LastCommand;
} global_editor_state;

inline void
set_theme_color(Application_Links *app, Style_Tag tag, int_color Color)
{
    Theme_Color ThemeColor = {tag, Color};
    set_theme_colors(app, &ThemeColor, 1);
}

static void set_current_keymap(struct Application_Links* app, int map) {
    unsigned int access = AccessAll;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
    
    if (!buffer.exists) { return; }
    
    buffer_set_setting(app, &buffer, BufferSetting_MapID, map);
}

CUSTOM_COMMAND_SIG(kill_line)
{
    //NOTE(chen): accomplished by marking curent pos, mvoe cursor to the end of line, then delete range
    View_Summary View = get_active_view(app, AccessOpen|AccessProtected);
    Buffer_Summary Buffer = get_buffer(app, View.buffer_id, AccessOpen|AccessProtected);
    
    Buffer_Seek CurrentPosSeek = {};
    CurrentPosSeek.type = buffer_seek_pos;
    CurrentPosSeek.pos = View.cursor.pos;
    int32_t LineEndPos = seek_line_end(app, &Buffer, View.cursor.pos);
    
    if (view_set_mark(app, &View, CurrentPosSeek))
    {
        Buffer_Seek LineEndPosSeek = {};
        LineEndPosSeek.type = buffer_seek_pos;
        LineEndPosSeek.pos = LineEndPos;
        
        if (LineEndPos == CurrentPosSeek.pos) //NOTE(chen): if there's just the \n char, delete the \n then
        {
            LineEndPosSeek.pos += 1; 
        }
        
        if (view_set_cursor(app, &View, LineEndPosSeek, true))
        {
            delete_range(app);
        }
    }
}

enum custom_mapid
{
    mapid_insert = 10000,
    mapid_normal,
    mapid_movement,
    mapid_visual,
    mapid_shared,
};

template <int32_t mapid>
CUSTOM_COMMAND_SIG(enter_mode)
{
    set_current_keymap(app, mapid);
    
    if (mapid == mapid_normal)
    {
        set_theme_color(app, Stag_Cursor, Normal_Color);
    }
    if (mapid == mapid_insert)
    {
        set_theme_color(app, Stag_Cursor, Insert_Color);
    }
    if (mapid == mapid_visual)
    {
        print_message(app, "enter visual\n", (int32_t)strlen("enter visual\n"));
        View_Summary view = get_active_view(app, AccessOpen|AccessProtected);
        global_editor_state.is_in_visual_mode = true;
        global_editor_state.highlight_marker = view.cursor.pos;
    }
}

CUSTOM_COMMAND_SIG(exit_visual_mode)
{
    print_message(app, "exit visual\n", (int32_t)strlen("exit visual\n"));
    View_Summary view = get_active_view(app, AccessOpen);
    view_set_highlight(app, &view, 0, 0, false);
    global_editor_state.is_in_visual_mode = false;
    enter_mode<mapid_normal>(app);
}

template <Custom_Command_Function seeker>
CUSTOM_COMMAND_SIG(visual_wrapper)
{
    exec_command(app, seeker);
    
    if (global_editor_state.is_in_visual_mode)
    {
        View_Summary view = get_active_view(app, AccessOpen|AccessProtected);
        int32_t cursor_pos = view.cursor.pos;
        int32_t marker_pos = global_editor_state.highlight_marker;
        
        view_set_cursor(app, &view, seek_pos(cursor_pos), false);
        
        if (marker_pos < view.cursor.pos)
        {
            view_set_highlight(app, &view, marker_pos, view.cursor.pos, true);
        }
        else
        {
            view_set_highlight(app, &view, view.cursor.pos, marker_pos, true);
        }
        
        view_set_cursor(app, &view, seek_pos(cursor_pos), false);
    }
}

inline Range
get_visual_highlight_range(Application_Links *app)
{
    int32_t pos1, pos2;
    
    View_Summary view = get_active_view(app, AccessOpen|AccessProtected);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen); 
    int32_t visual_marker = global_editor_state.highlight_marker;
    
    if (visual_marker < view.cursor.pos)
    {
        pos1 = visual_marker;
        pos2 = view.cursor.pos;
    }
    else 
    {
        pos1 = view.cursor.pos;
        pos2 = visual_marker;
    }
    
    Range result = {};
    result.start = pos1;
    result.end = pos2;
    
    return result;
}

CUSTOM_COMMAND_SIG(delete_highlight)
{
    View_Summary view = get_active_view(app, AccessOpen);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen); 
    Range selected_range = get_visual_highlight_range(app);
    
    global_editor_state.clipboard_size = selected_range.end - selected_range.start;
    if (global_editor_state.clipboard_mem)
    {
        free(global_editor_state.clipboard_mem);
    }
    global_editor_state.clipboard_mem = (char *)malloc(global_editor_state.clipboard_size);
    
    buffer_read_range(app, &buffer, selected_range.start, selected_range.end, global_editor_state.clipboard_mem);
    buffer_replace_range(app, &buffer, selected_range.start, selected_range.end, 0, 0);
    view_set_cursor(app, &view, seek_pos(selected_range.start), false);
    
    exit_visual_mode(app);
    enter_mode<mapid_normal>(app);
}

CUSTOM_COMMAND_SIG(copy_highlight)
{
    View_Summary view = get_active_view(app, AccessOpen|AccessProtected);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen); 
    Range selected_range = get_visual_highlight_range(app);
    
    global_editor_state.clipboard_size = selected_range.end - selected_range.start;
    if (global_editor_state.clipboard_mem)
    {
        free(global_editor_state.clipboard_mem);
    }
    global_editor_state.clipboard_mem = (char *)malloc(global_editor_state.clipboard_size);
    
    buffer_read_range(app, &buffer, selected_range.start, selected_range.end, global_editor_state.clipboard_mem);
    
    exit_visual_mode(app);
    enter_mode<mapid_normal>(app);
}

CUSTOM_COMMAND_SIG(change_highlight)
{
    View_Summary view = get_active_view(app, AccessOpen);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen); 
    Range selected_range = get_visual_highlight_range(app);
    
    buffer_replace_range(app, &buffer, selected_range.start, selected_range.end, 0, 0);
    view_set_cursor(app, &view, seek_pos(selected_range.start), false);
    
    exit_visual_mode(app);
    enter_mode<mapid_insert>(app);
}

//NOTE(Chen): editor's local clipboard, not OS's
CUSTOM_COMMAND_SIG(paste_editor_clipboard)
{
    View_Summary view = get_active_view(app, AccessOpen);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen); 
    
    buffer_replace_range(app, &buffer, view.cursor.pos, view.cursor.pos, global_editor_state.clipboard_mem, global_editor_state.clipboard_size);
    refresh_view(app, &view);
    
    view_set_cursor(app, &view, seek_pos(view.cursor.pos + global_editor_state.clipboard_size), false);
}

inline char 
get_trigger_character(Application_Links *app)
{
    char result = 0;
    
    User_Input in = get_command_input(app);
    uint8_t character[4];
    uint32_t length = to_writable_character(in, character);
    if (length != 0)
    {
        result = *(char *)character;
    }
    
    return result;
}

inline Key_Movement
get_key_movement(char key)
{
    for (int i = 0; i < ArrayCount(global_key_movements); ++i)
    {
        if (global_key_movements[i].key == key)
        {
            return global_key_movements[i];
        }
    }
    return {};
}

CUSTOM_COMMAND_SIG(close_other_panels)
{
    Access_Flag access = AccessOpen|AccessProtected;
    View_Summary active_view = get_active_view(app, access);
    
    View_Summary view_it = get_view_first(app, access); 
    while (view_it.exists)
    {
        View_Summary last_view = view_it;
        get_view_next(app, &view_it, access);
        if (last_view.view_id != active_view.view_id)
        {
            close_view(app, &last_view);
        }
    }
}

inline void
delete_portion(Application_Links *app, Key_Movement key_movement)
{
    if (key_movement.seeker)
    {
        uint32_t access = AccessOpen;                                    
        View_Summary view = get_active_view(app, access);                
        Buffer_Summary buffer = get_buffer(app, view.buffer_id, access); 
        if (buffer.exists){                                              
            int32_t pos1 = 0, pos2 = 0;                                  
            pos1 = view.cursor.pos;                                      
            exec_command(app, key_movement.seeker);
            refresh_view(app, &view);                                    
            pos2 = view.cursor.pos;                                      
            if (key_movement.dir == RIGHT) {                                     
                buffer_replace_range(app, &buffer, pos1, pos2, 0, 0);    
            }                                                            
            else {                                                       
                buffer_replace_range(app, &buffer, pos2, pos1, 0, 0);    
            }                                                            
        }                                                                
    }
}

CUSTOM_COMMAND_SIG(delete_chord)
{
    set_theme_color(app, Stag_Cursor, Transient_Color);
    
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
    if (!in.abort) 
    {
        Key_Movement key_movement = get_key_movement((char)in.key.keycode);
        if (key_movement.seeker)
        {
            delete_portion(app, key_movement);
        }
    }
    
    enter_mode<mapid_normal>(app);
}

CUSTOM_COMMAND_SIG(overwrite_chord)
{
    set_theme_color(app, Stag_Cursor, Transient_Color);
    
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
    if (in.abort) 
    {
        enter_mode<mapid_normal>(app);
        return;
    }
    
    Key_Movement key_movement = get_key_movement((char)in.key.keycode);
    if (key_movement.seeker)
    {
        delete_portion(app, key_movement);
        enter_mode<mapid_insert>(app);
    }
    else
    {
        enter_mode<mapid_normal>(app);
    }
}

typedef bool Qualifier(View_Summary *view, View_Summary *active_view);
typedef bool More_Qualifier(View_Summary *view, View_Summary *target_view, View_Summary *active_view);
#define QUALIFIER_SIG(func_name) bool func_name(View_Summary *view, View_Summary *active_view)
#define MORE_QUALIFIER_SIG(func_name) bool func_name(View_Summary *view, View_Summary *target_view, View_Summary *active_view)

inline int
dist(int A, int B)
{
    return abs(A - B);
}

QUALIFIER_SIG(seek_right_panel_qualifer)
{
    if (view->view_region.x0 >= active_view->view_region.x1 &&
        view->view_region.y0 < active_view->view_region.y1 &&
        view->view_region.y1 > active_view->view_region.y0)
    {
        return true;
    }
    return false;
}

MORE_QUALIFIER_SIG(seek_right_panel_more_qualifer)
{
    if (seek_right_panel_qualifer(view, active_view))
    {
        if (view->view_region.x0 < target_view->view_region.x0)
        {
            return true;
        }
        else if (view->view_region.x0 == view->view_region.x0 &&
                 dist(view->view_region.y0, active_view->view_region.y0) <
                 dist(target_view->view_region.y0, active_view->view_region.y0))
        {
            return true;
        }
    }
    return false;
}

QUALIFIER_SIG(seek_left_panel_qualifer)
{
    if (view->view_region.x1 <= active_view->view_region.x0 &&
        view->view_region.y0 < active_view->view_region.y1 &&
        view->view_region.y1 > active_view->view_region.y0)
    {
        return true;
    }
    return false;
}

MORE_QUALIFIER_SIG(seek_left_panel_more_qualifer)
{
    if (seek_left_panel_qualifer(view, active_view))
    {
        if (view->view_region.x1 > target_view->view_region.x1)
        {
            return true;
        }
        else if (view->view_region.x1 == view->view_region.x1 &&
                 dist(view->view_region.y0, active_view->view_region.y0) <
                 dist(target_view->view_region.y0, active_view->view_region.y0))
        {
            return true;
        }
    }
    return false;
}

QUALIFIER_SIG(seek_up_panel_qualifer)
{
    if (view->view_region.y1 <= active_view->view_region.y0 &&
        view->view_region.x0 < active_view->view_region.x1 &&
        view->view_region.x1 > active_view->view_region.x0)
    {
        return true;
    }
    return false;
}

MORE_QUALIFIER_SIG(seek_up_panel_more_qualifer)
{
    if (seek_up_panel_qualifer(view, active_view))
    {
        if (view->view_region.y1 > target_view->view_region.y1)
        {
            return true;
        }
        else if (view->view_region.y1 == view->view_region.y1 &&
                 dist(view->view_region.x0, active_view->view_region.x0) <
                 dist(target_view->view_region.x0, active_view->view_region.x0))
        {
            return true;
        }
    }
    return false;
}

QUALIFIER_SIG(seek_down_panel_qualifer)
{
    if (view->view_region.y0 >= active_view->view_region.y1 &&
        view->view_region.x0 < active_view->view_region.x1 &&
        view->view_region.x1 > active_view->view_region.x0)
    {
        return true;
    }
    return false;
}

MORE_QUALIFIER_SIG(seek_down_panel_more_qualifer)
{
    if (seek_down_panel_qualifer(view, active_view))
    {
        if (view->view_region.y0 < target_view->view_region.y0)
        {
            return true;
        }
        else if (view->view_region.y0 == view->view_region.y0 &&
                 dist(view->view_region.x0, active_view->view_region.x0) <
                 dist(target_view->view_region.x0, active_view->view_region.x0))
        {
            return true;
        }
    }
    return false;
}

template <Qualifier *qualify, More_Qualifier *more_qualify>
CUSTOM_COMMAND_SIG(seek_panel)
{
    Access_Flag access = AccessOpen|AccessProtected;
    View_Summary active_view = get_active_view(app, access);
    
    View_Summary target_view = active_view;
    bool target_view_found = false;
    
    for (View_Summary view = get_view_first(app, access); view.exists; get_view_next(app, &view, access))
    {
        if (target_view_found)
        {
            if (more_qualify(&view, &target_view, &active_view))
            {
                target_view = view;
            }
        }
        else
        {
            if (qualify(&view, &active_view))
            {
                target_view = view;
                target_view_found = true;
            }
        }
    }
    
    if (target_view_found)
    {
        set_active_view(app, &target_view);
    }
}

static Custom_Command_Function * const seek_right_panel = seek_panel<seek_right_panel_qualifer, seek_right_panel_more_qualifer>;
static Custom_Command_Function * const seek_left_panel  = seek_panel<seek_left_panel_qualifer, seek_left_panel_more_qualifer>;
static Custom_Command_Function * const seek_up_panel = seek_panel<seek_up_panel_qualifer, seek_up_panel_more_qualifer>;
static Custom_Command_Function * const seek_down_panel  = seek_panel<seek_down_panel_qualifer, seek_down_panel_more_qualifer>;

CUSTOM_COMMAND_SIG(window_chord)
{
    set_theme_color(app, Stag_Cursor, Transient_Color);
    
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
    if (!in.abort) 
    {
        if (in.key.keycode == 'w')
        {
            exec_command(app, basic_change_active_panel);
        }
        if (in.key.keycode == 'o')
        {
            exec_command(app, close_other_panels);
        }
        if (in.key.keycode == 'l')
        {
            exec_command(app, seek_right_panel);
        }
        if (in.key.keycode == 'h')
        {
            exec_command(app, seek_left_panel);
        }
        if (in.key.keycode == 'j')
        {
            exec_command(app, seek_down_panel);
        }
        if (in.key.keycode == 'k')
        {
            exec_command(app, seek_up_panel);
        }
    }
    
    enter_mode<mapid_normal>(app);
}

CUSTOM_COMMAND_SIG(open_vsplit_panel)
{
    View_Summary view = get_active_view(app, AccessAll);
    View_Summary new_view = open_view(app, &view, ViewSplit_Right);
    new_view_settings(app, &new_view);
    set_active_view(app, &view);
}

CUSTOM_COMMAND_SIG(open_hsplit_panel)
{
    View_Summary view = get_active_view(app, AccessAll);
    View_Summary new_view = open_view(app, &view, ViewSplit_Bottom);
    new_view_settings(app, &new_view);
    set_active_view(app, &view);
}

CUSTOM_COMMAND_SIG(command_chord)
{
    char command[100] = {};
    String command_string = make_fixed_width_string(command);
    
    Query_Bar command_bar = {};
    start_query_bar(app, &command_bar, 0);
    command_bar.prompt = make_lit_string(":");
    command_bar.string = command_string;
    
    for (;;) 
    {
        User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc | EventOnButton);
        if (in.abort) break;
        if (in.key.keycode == '\n') {
            if (match_ss(command_string, make_lit_string("bd"))) {
                exec_command(app, kill_buffer);
            }
            if (match_ss(command_string, make_lit_string("bb"))) {
                exec_command(app, interactive_switch_buffer);
            }
            if(match_ss(command_string, make_lit_string("w"))) {
                exec_command(app, save);
            }
            if (match_ss(command_string, make_lit_string("vs"))) {
                exec_command(app, open_vsplit_panel);
            }
            if (match_ss(command_string, make_lit_string("hs"))) {
                exec_command(app, open_hsplit_panel);
            }
            if (match_ss(command_string, make_lit_string("q"))) {
                exec_command(app, close_panel);
            }
            if (match_ss(command_string, make_lit_string("q!"))) {
                exec_command(app, exit_4coder);
            }
            if (match_ss(command_string, make_lit_string("e"))) {
                exec_command(app, interactive_open_or_new);
            }
            if (match_ss(command_string, make_lit_string("e!"))) {
                exec_command(app, reopen);
            }
            
            if (match_ss(command_string, make_lit_string("load project"))){
                exec_command(app, load_project);
            }
            if (match_ss(command_string, make_lit_string("open all code"))){
                exec_command(app, open_all_code);
            }
            if (match_ss(command_string, make_lit_string("open all code recursive"))){
                exec_command(app, open_all_code_recursive);
            }
            if(match_ss(command_string, make_lit_string("close all code"))){
                exec_command(app, close_all_code);
            }
            if (match_ss(command_string, make_lit_string("dos lines")) ||
                match_ss(command_string, make_lit_string("dosify"))){
                exec_command(app, eol_dosify);
            }
            if (match_ss(command_string, make_lit_string("nix lines")) ||
                match_ss(command_string, make_lit_string("nixify"))){
                exec_command(app, eol_nixify);
            }
            
            if (match_ss(substr(command_string, 0, 1), make_lit_string("!"))) {
                String system_command = substr(command_string, 1, command_string.size-1);
                
                char directory_buffer[1024];
                String hot_directory = make_fixed_width_string(directory_buffer);
                hot_directory.size = directory_get_hot(app, hot_directory.str, hot_directory.memory_size);
                
                String out_buffer = make_fixed_width_string(out_buffer_space);
                exec_system_command(app, 0, buffer_identifier(out_buffer.str, out_buffer.size), hot_directory.str, hot_directory.size, system_command.str, system_command.size, CLI_OverlapWithConflict | CLI_CursorAtEnd);
            }
            break;
        }
        
        if (in.key.keycode == key_back)
        {
            if (command_string.size > 0)
            {
                command_string.size -= 1;
            }
        }
        else
        {
            append_s_char(&command_string, (char)in.key.character);
        }
        command_bar.string = command_string;
    }
    
    end_query_bar(app, &command_bar, 0);
}

CUSTOM_COMMAND_SIG(insert_under)
{
    uint32_t access = AccessOpen;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
    
    exec_command(app, seek_end_of_line);
    refresh_view(app, &view);
    
    int32_t cursor_pos = view.cursor.pos;
    buffer_replace_range(app, &buffer, cursor_pos, cursor_pos, "\n", 1);
    view_set_cursor(app, &view, seek_pos(cursor_pos+1), true);
    
    enter_mode<mapid_insert>(app);
}

CUSTOM_COMMAND_SIG(insert_above)
{
    uint32_t access = AccessOpen;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
    
    exec_command(app, seek_beginning_of_line);
    refresh_view(app, &view);
    
    int32_t cursor_pos = view.cursor.pos;
    buffer_replace_range(app, &buffer, cursor_pos, cursor_pos, "\n", 1);
    
    enter_mode<mapid_insert>(app);
}

CUSTOM_COMMAND_SIG(insert_beginning_of_line)
{
    uint32_t access = AccessOpen;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
    
    exec_command(app, seek_beginning_of_line);
    refresh_view(app, &view);
    
    enter_mode<mapid_insert>(app);
}

CUSTOM_COMMAND_SIG(insert_end_of_line)
{
    uint32_t access = AccessOpen;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
    
    exec_command(app, seek_end_of_line);
    refresh_view(app, &view);
    
    enter_mode<mapid_insert>(app);
}

CUSTOM_COMMAND_SIG(replace_char)
{
    set_theme_color(app, Stag_Cursor, Transient_Color);
    
    uint32_t access = AccessOpen;
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
    
    User_Input in = get_user_input(app, EventOnAnyKey, EventOnEsc);
    if (!in.abort) 
    {
        uint8_t character[4];
        uint32_t length = to_writable_character(in, character);
        if (length != 0)
        {
            buffer_replace_range(app, &buffer, view.cursor.pos, view.cursor.pos+1, (char *)character, 1);
        }
    }
    enter_mode<mapid_normal>(app);
}

CUSTOM_COMMAND_SIG(insert_character)
{
    User_Input in = get_command_input(app);
    char character = (char)in.key.keycode;
    
    if (global_editor_state.j_is_pressed)
    {
        if (character == 'k')
        {
            global_editor_state.j_is_pressed = false;
            
            View_Summary view = get_active_view(app, AccessOpen);
            Buffer_Summary buffer = get_buffer(app, view.buffer_id, AccessOpen);
            
            buffer_replace_range(app, &buffer, view.cursor.pos-1, view.cursor.pos, 0, 0);
            refresh_view(app, &view);
            
            enter_mode<mapid_normal>(app);
            return;
        }
    }
    else
    {
        if (character == 'j')
        {
            global_editor_state.j_is_pressed = true;
        }
    }
    
    exec_command(app, write_character);
}

CUSTOM_COMMAND_SIG(seek_panel_bottom)
{
    View_Summary view = get_active_view(app, AccessOpen|AccessProtected);
    
    float new_y = view.scroll_vars.scroll_y + (float)view.view_region.y1 - view.line_height*4.0f;
    float x = view.preferred_x;
    
    view_set_cursor(app, &view, seek_xy(x, new_y, 0, view.unwrapped_lines), 0);
}

CUSTOM_COMMAND_SIG(seek_panel_top)
{
    View_Summary view = get_active_view(app, AccessOpen|AccessProtected);
    
    float new_y = view.scroll_vars.scroll_y + (float)view.view_region.y0 + view.line_height*4.0f;
    float x = view.preferred_x;
    
    view_set_cursor(app, &view, seek_xy(x, new_y, 0, view.unwrapped_lines), 0);
}

CUSTOM_COMMAND_SIG(seek_panel_middle)
{
    View_Summary view = get_active_view(app, AccessOpen|AccessProtected);
    
    float new_y = view.scroll_vars.scroll_y + ((float)view.view_region.y0 + (float)view.view_region.y1) / 2.0f;
    float x = view.preferred_x;
    
    view_set_cursor(app, &view, seek_xy(x, new_y, 0, view.unwrapped_lines), 0);
}

CUSTOM_COMMAND_SIG(dot_command)
{
    //TODO
}

inline void
begin_map_shared(Bind_Helper *context, int32_t mapid)
{
    begin_map(context, mapid);
    inherit_map(context, mapid_shared);
}

static void
custom_keys(Bind_Helper *context){
    begin_map(context, mapid_shared);
    bind(context, key_esc, MDFR_NONE, enter_mode<mapid_normal>);
    bind(context, 'p', MDFR_CTRL, interactive_open_or_new);
    bind(context, '\n', MDFR_CTRL, toggle_fullscreen);
    bind(context, 'E', MDFR_ALT, exit_4coder);
    bind(context, 'z', MDFR_CTRL, undo);
    bind(context, 'I', MDFR_CTRL, list_all_functions_current_buffer);
    bind(context, 'w', MDFR_CTRL, window_chord);
    
    bind(context, 'd', MDFR_CTRL, page_down);
    bind(context, 'u', MDFR_CTRL, page_up);
    bind(context, '.', MDFR_ALT, change_to_build_panel);
    bind(context, ',', MDFR_ALT, close_build_panel);
    bind(context, 'n', MDFR_ALT, goto_next_error);
    bind(context, 'N', MDFR_ALT, goto_prev_error);
    bind(context, 'M', MDFR_ALT, goto_first_error);
    bind(context, 'm', MDFR_ALT, build_in_build_panel);
    bind(context, 'z', MDFR_ALT, execute_any_cli);
    
    bind(context, 'c', MDFR_ALT, open_color_tweaker);
    bind(context, 'd', MDFR_ALT, open_debug);
    bind(context, key_f1, MDFR_NONE, project_fkey_command);
    bind(context, key_f2, MDFR_NONE, project_fkey_command);
    bind(context, key_f3, MDFR_NONE, project_fkey_command);
    bind(context, key_f4, MDFR_NONE, project_fkey_command);
    bind(context, key_f5, MDFR_NONE, project_fkey_command);
    bind(context, key_f6, MDFR_NONE, project_fkey_command);
    bind(context, key_f7, MDFR_NONE, project_fkey_command);
    bind(context, key_f8, MDFR_NONE, project_fkey_command);
    bind(context, key_f9, MDFR_NONE, project_fkey_command);
    bind(context, key_f10, MDFR_NONE, project_fkey_command);
    bind(context, key_f11, MDFR_NONE, project_fkey_command);
    bind(context, key_f12, MDFR_NONE, project_fkey_command);
    bind(context, key_f13, MDFR_NONE, project_fkey_command);
    bind(context, key_f14, MDFR_NONE, project_fkey_command);
    bind(context, key_f15, MDFR_NONE, project_fkey_command);
    bind(context, key_f16, MDFR_NONE, project_fkey_command);
    end_map(context);
    
    begin_map_shared(context, mapid_movement);
    bind(context, 'k', MDFR_NONE, visual_wrapper<move_up>);
    bind(context, 'j', MDFR_NONE, visual_wrapper<move_down>);
    bind(context, 'h', MDFR_NONE, visual_wrapper<move_left>);
    bind(context, 'l', MDFR_NONE, visual_wrapper<move_right>);
    for (int i = 0; i < ArrayCount(global_key_movements); ++i)
    {
        bind(context, global_key_movements[i].key, MDFR_NONE, global_key_movements[i].seeker);
    }
    end_map(context);
    
    begin_map_shared(context, mapid_visual);
    inherit_map(context, mapid_movement);
    bind(context, 'v', MDFR_NONE, exit_visual_mode);
    bind(context, key_esc, MDFR_NONE, exit_visual_mode);
    bind(context, 'd', MDFR_NONE, delete_highlight);
    bind(context, 'y', MDFR_NONE, copy_highlight);
    bind(context, 'c', MDFR_NONE, change_highlight);
    end_map(context);
    
    begin_map_shared(context, mapid_normal);
    inherit_map(context, mapid_movement);
    
    bind(context, 'i', MDFR_NONE, enter_mode<mapid_insert>);
    bind(context, 'o', MDFR_NONE, insert_under);
    bind(context, 'O', MDFR_NONE, insert_above);
    bind(context, 'A', MDFR_NONE, insert_end_of_line);
    bind(context, 'I', MDFR_NONE, insert_beginning_of_line);
    bind(context, 'r', MDFR_NONE, replace_char);
    bind(context, 'x', MDFR_NONE, delete_char);
    bind(context, '\n', MDFR_NONE, newline_or_goto_position);
    
    bind(context, 'v', MDFR_NONE, enter_mode<mapid_visual>);
    bind(context, '.', MDFR_NONE, dot_command);
    
    bind(context, 'L', MDFR_NONE, seek_panel_bottom);
    bind(context, 'M', MDFR_NONE, seek_panel_middle);
    bind(context, 'H', MDFR_NONE, seek_panel_top);
    
    bind(context, 'p', MDFR_NONE, paste_editor_clipboard);
    bind(context, 'P', MDFR_NONE, paste_and_indent);
    bind(context, 'u', MDFR_NONE, undo);
    bind(context, 'D', MDFR_NONE, kill_line);
    bind(context, 'f', MDFR_CTRL, search);
    bind(context, 'r', MDFR_CTRL, reverse_search);
    bind(context, 'q', MDFR_NONE, query_replace);
    bind(context, 'F', MDFR_NONE, list_all_locations);
    bind(context, 'g', MDFR_CTRL, goto_line);
    bind(context, 'z', MDFR_NONE, center_view);
    bind(context, ' ', MDFR_NONE, set_mark);
    
    bind(context, 'd', MDFR_NONE, delete_chord);
    bind(context, 'c', MDFR_NONE, overwrite_chord);
    bind(context, ';', MDFR_NONE, command_chord);
    bind(context, ':', MDFR_NONE, command_chord);
    end_map(context);
    
    begin_map_shared(context, mapid_insert);
    bind_vanilla_keys(context, insert_character);
    
    bind(context, '\n', MDFR_NONE, write_and_auto_tab);
    bind(context, '\n', MDFR_SHIFT, write_and_auto_tab);
    bind(context, '}', MDFR_NONE, write_and_auto_tab);
    bind(context, ')', MDFR_NONE, write_and_auto_tab);
    bind(context, ']', MDFR_NONE, write_and_auto_tab);
    bind(context, ';', MDFR_NONE, write_and_auto_tab);
    bind(context, '#', MDFR_NONE, write_and_auto_tab);
    
    bind(context, '\t', MDFR_NONE, word_complete);
    bind(context, '\t', MDFR_CTRL, auto_tab_range);
    bind(context, '\t', MDFR_SHIFT, auto_tab_line_at_cursor);
    
    bind(context, key_back, MDFR_NONE, backspace_char);
    bind(context, ' ', MDFR_CTRL, set_mark);
    bind(context, '\n', MDFR_NONE, newline_or_goto_position);
    
    bind(context, key_esc, MDFR_NONE, enter_mode<mapid_normal>);
    end_map(context);
}

OPEN_FILE_HOOK_SIG(custom_file_settings)
{
    char *extra_code_file_extensions[] = {
        "glsl",
    };
    
    Buffer_Summary buffer = get_buffer(app, buffer_id, AccessAll);
    Assert(buffer.exists);
    
    bool32 treat_as_code = false;
    bool32 treat_as_todo = false;
    bool32 wrap_lines = true;
    
    int32_t extension_count = 0;
    char **extension_list = get_current_code_extensions(&extension_count);
    
    Parse_Context_ID parse_context_id = 0;
    
    if (buffer.file_name != 0 && buffer.size < (16 << 20)){
        String name = make_string(buffer.file_name, buffer.file_name_len);
        String ext = file_extension(name);
        
        for (int32_t i = 0; i < ArrayCount(extra_code_file_extensions); ++i)
        {
            if (match(ext, extra_code_file_extensions[i]))
            {
                treat_as_code = true;
            }
        }
        
        for (int32_t i = 0; i < extension_count; ++i){
            if (match(ext, extension_list[i])){
                treat_as_code = true;
                
                if (match(ext, "cs")){
                    if (parse_context_language_cs == 0){
                        init_language_cs(app);
                    }
                    parse_context_id = parse_context_language_cs;
                }
                
                if (match(ext, "java")){
                    if (parse_context_language_java == 0){
                        init_language_java(app);
                    }
                    parse_context_id = parse_context_language_java;
                }
                
                if (match(ext, "rs")){
                    if (parse_context_language_rust == 0){
                        init_language_rust(app);
                    }
                    parse_context_id = parse_context_language_rust;
                }
                
                break;
            }
        }
        
        if (!treat_as_code){
            String lead_name = front_of_directory(name);
            if (match_insensitive(lead_name, "todo.txt")){
                treat_as_todo = true;
            }
        }
    }
    
    if (treat_as_code){
        wrap_lines = false;
    }
    if (buffer.file_name == 0){
        wrap_lines = false;
    }
    
#if 0
    int32_t map_id = (treat_as_code)?((int32_t)default_code_map):((int32_t)mapid_file);
#else
    int32_t map_id = mapid_normal;
#endif
    
    buffer_set_setting(app, &buffer, BufferSetting_WrapPosition, default_wrap_width);
    buffer_set_setting(app, &buffer, BufferSetting_MinimumBaseWrapPosition, default_min_base_width);
    buffer_set_setting(app, &buffer, BufferSetting_MapID, map_id);
    buffer_set_setting(app, &buffer, BufferSetting_ParserContext, parse_context_id);
    
    if (treat_as_todo){
        buffer_set_setting(app, &buffer, BufferSetting_WrapLine, true);
        buffer_set_setting(app, &buffer, BufferSetting_LexWithoutStrings, true);
        buffer_set_setting(app, &buffer, BufferSetting_VirtualWhitespace, true);
    }
    else if (treat_as_code && enable_code_wrapping && buffer.size < (1 << 18)){
        // NOTE(allen|a4.0.12): There is a little bit of grossness going on here.
        // If we set BufferSetting_Lex to true, it will launch a lexing job.
        // If a lexing job is active when we set BufferSetting_VirtualWhitespace, the call can fail.
        // Unfortunantely without tokens virtual whitespace doesn't really make sense.
        // So for now I have it automatically turning on lexing when virtual whitespace is turned on.
        // Cleaning some of that up is a goal for future versions.
        buffer_set_setting(app, &buffer, BufferSetting_WrapLine, true);
        buffer_set_setting(app, &buffer, BufferSetting_VirtualWhitespace, true);
    }
    else{
        buffer_set_setting(app, &buffer, BufferSetting_WrapLine, wrap_lines);
        buffer_set_setting(app, &buffer, BufferSetting_Lex, treat_as_code);
    }
    buffer_set_setting(app, &buffer, BufferSetting_WrapLine, false);
    
    // no meaning for return
    return(0);
}

extern "C" int32_t
get_bindings(void *data, int32_t size){
    Bind_Helper context = begin_bind_helper(data, size);
    set_all_default_hooks(&context);
    set_open_file_hook(&context, custom_file_settings);
    custom_keys(&context);
    
    int32_t result = end_bind_helper(&context);
    return result;
}

#endif //FCODER_CUSTOM_BINDINGS

