#pragma once
#include "djui.h"

void djui_panel_confirm_create(struct DjuiBase* caller, char* title, char* message, void (*on_yes_click)(struct DjuiBase*));
// Same as djui_panel_confirm_create(), but the "no" button also runs a callback
// instead of just closing the panel.
void djui_panel_confirm_create_ex(struct DjuiBase* caller, char* title, char* message,
                                  void (*on_yes_click)(struct DjuiBase*),
                                  void (*on_no_click)(struct DjuiBase*));

