#include "djui.h"
#include "djui_panel.h"
#include "djui_panel_menu.h"
#include "djui_panel_menu_options.h"
#include "djui_panel_options.h"
#include "djui_panel_language.h"
#include "djui_panel_info.h"
#include "pc/utils/misc.h"
#include "pc/configfile.h"
#include "game/hardcoded.h"
#if defined(SM64COOPDX_CN_TEXT)
#include "game/segment2.h"
#include "pc/rom_checker.h"
#include "game/cn_rom_assets.h"
#endif
#if defined(_WIN32)
#include <windows.h>
#endif

static void djui_panel_options_open_user_folder(UNUSED struct DjuiBase* caller) {
    open_folder(fs_get_write_path(""));
}

#ifdef DEVELOPMENT
void djui_panel_options_debug_create(struct DjuiBase* caller) {
    struct DjuiThreePanel* panel = djui_panel_menu_create(DLANG(MISC, DEBUG_TITLE), false);
    struct DjuiBase* body = djui_three_panel_get_body(panel);
    {
        djui_checkbox_create(body, DLANG(MISC, FIXED_COLLISIONS), (bool*)&gLevelValues.fixCollisionBugs, NULL);
        djui_checkbox_create(body, DLANG(MISC, LUA_PROFILER), &configLuaProfiler, NULL);
        djui_checkbox_create(body, DLANG(MISC, CTX_PROFILER), &configCtxProfiler, NULL);
        djui_checkbox_create(body, DLANG(MISC, DEBUG_PRINT), &configDebugPrint, NULL);
        djui_checkbox_create(body, DLANG(MISC, DEBUG_INFO), &configDebugInfo, NULL);
        djui_checkbox_create(body, DLANG(MISC, DEBUG_ERRORS), &configDebugError, NULL);

#ifndef HANDHELD
#if defined(_WIN32)
        djui_button_create(body, DLANG(OPTIONS, APPDATA), DJUI_BUTTON_STYLE_NORMAL, djui_panel_options_open_user_folder);
#elif __linux__ || __APPLE__ || __MACH__
        djui_button_create(body, DLANG(OPTIONS, USER_FOLDER), DJUI_BUTTON_STYLE_NORMAL, djui_panel_options_open_user_folder);
#endif
#endif
        djui_button_create(body, DLANG(MENU, BACK), DJUI_BUTTON_STYLE_BACK, djui_panel_menu_back);
    }

    djui_panel_add(caller, panel, NULL);
}
#endif

#if defined(SM64COOPDX_CN_TEXT)
// Runtime switch for the built-in iQue/Chinese text pack. Applies immediately:
// chinese_text_pack_set_active() repoints the live dialog / course / act tables
// and persists the choice.
static void djui_panel_misc_chinese_text_changed(UNUSED struct DjuiBase* checkbox) {
    chinese_text_pack_set_active(configChineseText);
}

// The pack's glyphs, dialog text and textures are read out of the player's own
// 神游 dump at startup, so a machine without that dump has nothing to show --
// but "nothing to show" is not the same as "explained": without this gate the
// player just assumes the feature is broken. So the checkbox is greyed out
// (DJUI paints a disabled interactable in grey and refuses the click) plus a
// one-line reason underneath.
//
// chinese_text_pack_init() applies the same rule at startup and rom_assets.c
// applies it to the texture overlay; this function only makes the UI honest
// about it.
//
// The checkbox is bound to a mirror instead of configChineseText, because the
// stored value is deliberately left alone when the dump is missing (see
// chinese_text_pack_init): it can legitimately say "true" while nothing is on.
// Showing that would be a ticked-but-grey box, i.e. "it is on and you cannot
// turn it off". The mirror reads false, which is what is actually drawn, and
// nothing can write to it -- disabled controls do not dispatch value changes,
// but the pointer swap means even a future fix to that cannot leak into the
// config.
static bool sChineseTextUnavailable = false;
static void djui_panel_misc_chinese_text_create(struct DjuiBase* body) {
    struct DjuiCheckbox* checkbox = djui_checkbox_create(body, DLANG(MISC, CHINESE_TEXT_PACK),
                                                         &configChineseText,
                                                         djui_panel_misc_chinese_text_changed);
    if (checkbox == NULL || cn_rom_assets_ready()) {
        return;
    }

    sChineseTextUnavailable = false;
    checkbox->value = &sChineseTextUnavailable;
    djui_base_set_enabled(&checkbox->base, false);

    struct DjuiText* hint = djui_text_create(body, DLANG(MISC, CHINESE_TEXT_PACK_NO_ROM));
    if (hint == NULL) { return; }
    djui_text_set_alignment(hint, DJUI_HALIGN_LEFT, DJUI_VALIGN_TOP);
    djui_text_set_font_scale(hint, 0.7f);
    djui_base_set_color(&hint->base, 150, 150, 150, 255);
    djui_base_set_size_type(&hint->base, DJUI_SVT_RELATIVE, DJUI_SVT_ABSOLUTE);
    djui_base_set_size(&hint->base, 1.0f, 24);
}
#endif

void djui_panel_misc_create(struct DjuiBase* caller) {
    struct DjuiThreePanel* panel = djui_panel_menu_create(DLANG(MISC, MISC_TITLE), false);
    struct DjuiBase* body = djui_three_panel_get_body(panel);
    {
        djui_checkbox_create(body, DLANG(DISPLAY, SHOW_PING), &configShowPing, NULL);
        djui_checkbox_create(body, DLANG(MISC, DISABLE_POPUPS), &configDisablePopups, NULL);
#if defined(SM64COOPDX_CN_TEXT)
        djui_panel_misc_chinese_text_create(body);
#endif
#ifndef DEVELOPMENT
        djui_checkbox_create(body, DLANG(MISC, LUA_PROFILER), &configLuaProfiler, NULL);
#endif

        djui_button_create(body, DLANG(MISC, LANGUAGE), DJUI_BUTTON_STYLE_NORMAL, djui_panel_language_create);
        djui_button_create(body, DLANG(MISC, MENU_OPTIONS), DJUI_BUTTON_STYLE_NORMAL, djui_panel_main_menu_create);
        djui_button_create(body, DLANG(MISC, INFORMATION), DJUI_BUTTON_STYLE_NORMAL, djui_panel_info_create);
#ifdef DEVELOPMENT
        djui_button_create(body, DLANG(MISC, DEBUG), DJUI_BUTTON_STYLE_NORMAL, djui_panel_options_debug_create);
#endif

#ifndef DEVELOPMENT
#ifndef HANDHELD
#if defined(_WIN32)
        djui_button_create(body, DLANG(OPTIONS, APPDATA), DJUI_BUTTON_STYLE_NORMAL, djui_panel_options_open_user_folder);
#elif __linux__ || __APPLE__ || __MACH__
        djui_button_create(body, DLANG(OPTIONS, USER_FOLDER), DJUI_BUTTON_STYLE_NORMAL, djui_panel_options_open_user_folder);
#endif
#endif
#endif
        djui_button_create(body, DLANG(MENU, BACK), DJUI_BUTTON_STYLE_BACK, djui_panel_menu_back);
    }

    djui_panel_add(caller, panel, NULL);
}
