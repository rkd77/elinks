-- Bookmark system for Links-Lua.

-----------------------------------------------------------------------
--  User options
---------------------------------------------------------------------

-- Default location to save and load bookmarks from.
bm_bookmark_file = elinks_home.."/bookmark.lst"

-- Set to non-`nil' to see URLs in the generated page.
bm_display_urls = nil

-- Set to non-`nil' to show links to category headers.
-- Only useful while sorting categories, otherwise just annoying.
bm_display_category_links = 1

-- Set to non-`nil' to automatically sort bookmarks alphabetically.
-- Do not set this if you care about the sorting of your bookmarks!
bm_auto_sort_bookmarks = nil


----------------------------------------------------------------------
--  Implementation
----------------------------------------------------------------------

-- We use a special syntax that looks like
-- user:bookmark/1			     (for categories)
-- user:bookmark/1,2/http://some.phony.url/  (for bookmarks)
bm_marker = 'user:bookmark'


if not bm_bookmarks then
    -- If the user reloads this script, we don't want to
    -- lose all the bookmarks! :-)
    bm_bookmarks = {}
end


function bm_is_category (str)
    local _,n = string.gsub (str, bm_marker..'/%d+$', '')
    return n ~= 0
end


function bm_is_bookmark (str)
    local _,n = string.gsub (str, bm_marker..'/%d+,%d+/', '')
    return n ~= 0
end


function bm_decode_info (str)
    if bm_is_category (str) then
	str = string.gsub (str, '.-/(%d+)', 'return %1')
    else
	str = string.gsub (str, '.-/(%d+),(%d+)/(.*)', 'return %1,%2,"%3"')
    end
    return dostring (str)
end


function bm_find_category (cat)
    for i = 1, table.getn (bm_bookmarks) do
	local table = bm_bookmarks[i]
	if table.category == cat then return table end
    end
end


function bm_sort_bookmarks ()
    if not bm_auto_sort_bookmarks then return end
    table.sort (bm_bookmarks, function (a, b) return a.category < b.category end)
    foreachi (bm_bookmarks,
		function (i, v)
		    table.sort (v, function (a, b) return a.name < b. name end)
		end)
end


function bm_generate_html ()
    local s = '<html><head><title>Bookmarks</title></head>'
		..'<body link=blue>\n'
    for i = 1, table.getn (bm_bookmarks) do
	local table = bm_bookmarks[i]
	s = s..'<h3>'
	if bm_display_category_links then
	    s = s..'<a href="'..bm_marker..'/'..i..'">'
	end
	s = s..'<font color=gray>'..table.category..'</font>'
	if bm_display_category_links then
	    s = s..'</a>'
	end
	s = s..'</h3>\n<ul>\n'
	for j = 1, table.getn (table) do
	    local bm = table[j]
	    s = s..'<li><a href="'..bm_marker..'/'..i..','..j..'/'
		..bm.url..'">'..bm.name..'</a>\n'
	    if bm_display_urls then s = s..'<br>'..bm.url..'\n' end
	end
	s = s..'</ul>\n'
    end
    s = s..'<hr>'..os.date ()..'\n'
    return s..'</body></html>\n'
end


-- Write bookmarks to disk.
function bm_save_bookmarks (filename)
    if bm_dont_save then return end

    function esc (str)
	return string.gsub (str, "([^-%w \t_@#:/'().])",
		     function (s)
			return string.format ("\\%03d", string.byte (s))
		     end)
    end

    if not filename then filename = bm_bookmark_file end
    local tab = '  '
    writeto (filename)
    write ('return {\n')
    for i = 1, table.getn (bm_bookmarks) do
	local table = bm_bookmarks[i]
	write (tab..'{\n'..tab..tab..'category = "'
		..esc (table.category)..'";\n')
	for i = 1, table.getn (table) do
	    local bm = table[i]
	    write (tab..tab..'{ name = "'..esc (bm.name)..'", url = "'
		    ..esc (bm.url)..'" },\n')
	end
	write (tab..'},\n')
    end
    write ('}\n')
    writeto ()
end


-- Load bookmarks from disk.
function bm_load_bookmarks (filename)
    if not filename then filename = bm_bookmark_file end
    local tmp = dofile (filename)
    if type (tmp) == 'table' then
	bm_bookmarks = tmp
	bm_sort_bookmarks ()
	bm_dont_save = nil
    else
	_ALERT ("Error loading "..filename)
	bm_dont_save = 1
    end
end


-- Return the URL of a bookmark.
function bm_get_bookmark_url (bm)
    if bm_is_bookmark (bm) then
	local _,_,url = bm_decode_info (bm)
	return url
    end
end


-- Bind this to a key to display bookmarks.
function bm_view_bookmarks ()
    local tmp = tmpname()..'.html'
    writeto (tmp)
    write (bm_generate_html ())
    writeto ()
    table.insert (tmp_files, tmp)
    return 'goto_url', tmp
end


function bm_do_add_bookmark (cat, name, url)
    if cat == "" or name == "" or url == "" then
	_ALERT ("Bad bookmark entry")
    end
    local table = bm_find_category (cat)
    if not table then
        table = { category = cat }
        table.insert (bm_bookmarks, table)
    end
    table.insert (table, { name = name, url = url })
    bm_sort_bookmarks ()
end


-- Bind this to a key to add a bookmark.
function bm_add_bookmark ()
    edit_bookmark_dialog ('', current_title () or '', current_url () or '',
			    function (cat, name, url)
				bm_do_add_bookmark (cat, name, url)
				if current_title () == 'Bookmarks' then
				    return bm_view_bookmarks ()
				end
			    end)
end


-- Bind this to a key to edit the currently highlighted bookmark.
function bm_edit_bookmark ()
    local bm = current_link ()
    if not bm then
    elseif bm_is_category (bm) then
	local i = bm_decode_info (bm)
	edit_bookmark_dialog (bm_bookmarks[i].category, '', '',
	    function (cat)
		if cat == '' then
		    _ALERT ('Bad input')
		elseif bm_bookmarks[i].category ~= cat then
		    local j = bm_find_category (cat)
		    if not j then
			bm_bookmarks[i].category = cat
		    else
			local tmp = bm_bookmarks[i]
			for i = 1, table.getn (tmp) do
			    bm_do_add_bookmark (cat, tmp[i].name, tmp[i].url)
			end
			bm_delete_bookmark (i)
		    end
		    return bm_view_bookmarks ()
		end
	    end)

    elseif bm_is_bookmark (bm) then
	local i,j = bm_decode_info (bm)
	local entry = bm_bookmarks[i][j]
	edit_bookmark_dialog (bm_bookmarks[i].category,
			     entry.name, entry.url,
			     function (cat, name, url)
				if cat == '' or name == '' or url == '' then
				    _ALERT ('Bad input')
				else
				    if cat ~= bm_bookmarks[i].category then
					bm_do_delete_bookmark (i, j)
					bm_do_add_bookmark (cat, name, url)
				    else
					entry.name = name
					entry.url = url
				    end
				    return bm_view_bookmarks ()
				end
			     end)
    end
end


function bm_do_delete_bookmark (i, j)
    if not j then
	table.remove (bm_bookmarks, i)
    else
	table.remove (bm_bookmarks[i], j)
	if table.getn (bm_bookmarks[i]) == 0 then table.remove (bm_bookmarks, i) end
    end
end


-- Bind this to a key to delete the currently highlighted bookmark.
function bm_delete_bookmark ()
    local bm = current_link ()
    if bm and (bm_is_category (bm) or bm_is_bookmark (bm)) then
	local i,j = bm_decode_info (bm)
	bm_do_delete_bookmark (i, j)
	return bm_view_bookmarks ()
    end
end


function bm_do_move_bookmark (dir)
    function tswap (t, i, j)
	if i > 0 and j > 0 and i <= table.getn (t) and j <= table.getn (t) then
	    local x = t[i]; t[i] = t[j]; t[j] = x
	    return 1
	end
    end

    local bm = current_link ()
    if not bm then
    elseif bm_is_category (bm) then
	local i = bm_decode_info (bm)
	if tswap (bm_bookmarks, i, i+dir) then
	    return bm_view_bookmarks ()
	end
    elseif bm_is_bookmark (bm) then
	local i,j = bm_decode_info (bm)
	if bm_bookmarks[i] and tswap (bm_bookmarks[i], j, j+dir) then
	    return bm_view_bookmarks ()
	end
    end
end


-- Bind this to a key to move the currently highlighted bookmark up.
function bm_move_bookmark_up ()
    if not bm_auto_sort_bookmarks then return bm_do_move_bookmark (-1) end
end


-- Bind this to a key to move the currently highlighted bookmark down.
function bm_move_bookmark_down ()
    if not bm_auto_sort_bookmarks then return bm_do_move_bookmark (1) end
end


function bookmarks_follow_url_hook (url)
    if bm_is_category (url) then
	return nil,true
    else
	url = bm_get_bookmark_url (url)
	if url then return url,true end
    end

    return url,nil
end
table.insert(follow_url_hooks, bookmarks_follow_url_hook)


function bookmarks_quit_hook ()
    bm_save_bookmarks ()
end
table.insert(quit_hooks, bookmarks_quit_hook)


-- Be careful not to load bookmarks if this script is being
-- reloaded while in ELinks, or we will lose unsaved changes.
if not bm_bookmarks or table.getn (bm_bookmarks) == 0 then
    bm_load_bookmarks ()
end


-- My bookmark key bindings.
--    bind_key ('main', 'a', bm_add_bookmark)
--    bind_key ('main', 's', bm_view_bookmarks)
--    bind_key ('main', 'Alt-e', bm_edit_bookmark)
--    bind_key ('main', 'Alt-d', bm_delete_bookmark)
--    bind_key ('main', 'Alt-k', bm_move_bookmark_up)
--    bind_key ('main', 'Alt-j', bm_move_bookmark_down)


-- vim: shiftwidth=4 softtabstop=4
