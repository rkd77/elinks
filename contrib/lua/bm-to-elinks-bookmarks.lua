#!/usr/bin/lua -f
-- Convert bm.lua-format bookmarks to ELinks-format bookmarks.
-- Peter Wang, 2002-12-19

prog = arg[0]
infile = arg[1]

if not infile then
    print("Convert bm.lua-format bookmarks to ELinks-format bookmarks.\n")
    print("Usage: " .. prog .. " bookmark.lst")
    print("Output is written to stdout.\n")
    exit(1)
end

bookmarks = dofile(infile)
if type(bookmarks) ~= "table" then
    print("Error loading " .. infile)
    exit(1)
end

function tab2spc(s) return gsub(s, "\t", " ") end -- just in case

for i, cat in bookmarks do
    print(tab2spc(cat.category), "", 0, "FE")

    for i = 1, getn(cat) do
	local bm = cat[i]
	print(tab2spc(bm.name), tab2spc(bm.url), 1)
    end
end
