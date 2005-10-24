-- Configuration for hooks.lua file, put in ~/.elinks/ as config.lua.

-- ** IMPORTANT **
-- Individual functions may be disabled for security by assigning them
-- to `nil'.

    -- openfile = nil    -- may open files in write mode
    -- readfrom = nil    -- reading from pipe can execute commands
    -- writeto = nil
    -- appendto = nil
    -- pipe_read = nil
    -- remove = nil
    -- rename = nil
    -- execute = nil
    -- exit = nil

-- Home directory:

    home_dir = home_dir or (getenv and getenv ("HOME")) or "/home/MYSELF"
    hooks_file = elinks_home.."/hooks.lua"

-- Pausing: When external programs are run, sometimes we need to pause
-- to see the output.  This is the string we append to the command
-- line to do that.  You may customise it if you wish.

    pause = '; echo -ne "\\n\\e[1;32mPress ENTER to continue...\\e[0m"; read'

-- Make ALT="" into ALT="&nbsp;": Makes web pages with superfluous
-- images look better.  However, even if you disable the "Display links
-- to images" option, single space links to such images will appear.
-- To enable, set the following to 1.  If necessary, you can change
-- this while in Links using the Lua Console, then reload the page.
-- See also the keybinding section at the end of the file.

    mangle_blank_alt = nil

-- For any other lua script to be loaded (note that you don't need to load
-- hooks.lua here, as it's loaded even when we'll get to here actually),
-- uncomment and clone following line:

--  dofile (elinks_home.."/script.lua")

-- The following commands, when uncommented, will load certain scripts that are
-- distributed with ELinks. If you enable any of them, you will need either
-- to copy the scripts to your home directory or to update the paths
-- in the commands relevant commands below.

-- Bookmarks: Uncomment the following line to enable the Lua bookmarks
-- manager

--  dofile (elinks_home.."/bm.lua")

-- Highlighting: Uncomment the following line if you want to see highlighted
-- source code.  You need to have code2html installed and set text/html
-- as the MIME-type for .c, .h, .pl, .py, .sh, .awk, .patch extensions
-- in the Options Manager or in elinks.conf

--  dofile (elinks_home.."/highlight.lua")

-- Babelfish: This allows one to enter the the URL 'bb <from> <to>
-- <url>|<text>' to translate the given url or text string between
-- the given languages.

--  dofile (elinks_home.."/babelfish.lua")

--  dofile (elinks_home.."/md5checks.lua")
--  dofile (elinks_home.."/remote.lua")
