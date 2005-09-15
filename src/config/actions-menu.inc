/* Please keep these tables in alphabetical order, and in sync with
 * the ACT_* constants in kbdbind.h.  */

/* These two actions are common over all keymaps: */
ACTION_(MENU, "none", NONE, N__("Do nothing"), 0),
ACTION_(MENU, " *scripting-function*", SCRIPTING_FUNCTION, NULL, 0),

ACTION_(MENU, "cancel", CANCEL, N__("Cancel current state"), 0),
ACTION_(MENU, "delete", DELETE, N__("Delete character under cursor"), 0),
ACTION_(MENU, "down", DOWN, N__("Move cursor downwards"), 0),
ACTION_(MENU, "end", END, N__("Go to the end of the page/line"), 0),
ACTION_(MENU, "enter", ENTER, N__("Follow the current link"), 0),
ACTION_(MENU, "expand", EXPAND, N__("Expand item"), 0),
ACTION_(MENU, "home", HOME, N__("Go to the start of the page/line"), 0),
ACTION_(MENU, "left", LEFT, N__("Move the cursor left"), 0),
ACTION_(MENU, "mark-item", MARK_ITEM, N__("Mark item"), 0),
ACTION_(MENU, "next-item", NEXT_ITEM, N__("Move to the next item"), 0),
ACTION_(MENU, "page-down", PAGE_DOWN, N__("Move downwards by a page"), 0),
ACTION_(MENU, "page-up", PAGE_UP, N__("Move upwards by a page"), 0),
ACTION_(MENU, "previous-item", PREVIOUS_ITEM, N__("Move to the previous item"), 0),
ACTION_(MENU, "redraw", REDRAW, N__("Redraw the terminal"), 0),
ACTION_(MENU, "right", RIGHT, N__("Move the cursor right"), 0),
ACTION_(MENU, "search", SEARCH, N__("Search for a text pattern"), 0),
ACTION_(MENU, "select", SELECT, N__("Select current highlighted item"), 0),
ACTION_(MENU, "unexpand", UNEXPAND, N__("Collapse item"), 0),
ACTION_(MENU, "up", UP, N__("Move cursor upwards"), 0),
