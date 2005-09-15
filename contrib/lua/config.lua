-- Configuration for hooks.lua file, put in ~/.elinks/ as config.lua.
-- $Id: config.lua,v 1.4 2003/10/02 12:16:04 kuser Exp $

-- ** IMPORTANT **
-- For security reasons, systems functions are not enabled by default.
-- To do so, uncomment the following line, but be careful about
-- including unknown code.  Individual functions may be disabled by
-- assigning them to `nil'.

    -- enable_systems_functions ()

    -- openfile = nil    -- may open files in write mode
    -- readfrom = nil    -- reading from pipe can execute commands
    -- writeto = nil
    -- appendto = nil
    -- pipe_read = nil
    -- remove = nil
    -- rename = nil
    -- execute = nil
    -- exit = nil

-- Home directory: If you do not enable system functions, you will
-- need to set the following to your home directory.

    home_dir = home_dir or (getenv and getenv ("HOME")) or "/home/MYSELF"
    hooks_file = elinks_home.."/hooks.lua"

-- Pausing: When external programs are run, sometimes we need to pause
-- to see the output.  This is the string we append to the command
-- line to do that.  You may customise it if you wish.

    pause = '; echo -ne "\\n\\e[1;32mPress ENTER to continue...\\e[0m"; read'

-- Highlightning: Set highlight_enable = 1 if you want to see highligted
-- source code.  You need to have installed code2html and set text/html
-- as mime-type for .c, .h, .pl, .py, .sh, .awk, .patch extensions in Options
-- Manager or in elinks.conf

    highlight_enable = nil

-- Make ALT="" into ALT="&nbsp;": Makes web pages with superfluous
-- images look better.  However, even if you disable the "Display links
-- to images" option, single space links to such images will appear.
-- To enable, set the following to 1.  If necessary, you can change
-- this while in Links using the Lua Console, then reload the page.
-- See also the keybinding section at the end of the file.

    mangle_blank_alt = nil

-- If you set this to non-`nil', the bookmark addon will be loaded,
-- and actions will be bound to my key bindings.  Change them at the
-- bottom of the file.
-- Note that you need to copy bm.lua (from contrib/) to ~/.elinks/ directory
-- as well.

    bookmark_addon = nil

-- For any other lua script to be loaded (note that you don't need to load
-- hooks.lua here, as it's loaded even when we'll get to here actually; you
-- don't need to load bm.lua here as well, as it gets done in hooks.lua if you
-- will just enable bookmark_addon variable), uncomment and clone following
-- line:

--  dofile (elinks_home.."/script.lua")

--  dofile (elinks_home.."/md5checks.lua")
--  dofile (elinks_home.."/remote.lua")
