-- Check MD5 sums of download files automagically (it expects them in
-- downloadedfile.txt).

----------------------------------------------------------------------
-- Installation
----------------------------------------------------------------------
--
-- 1. Put this into your hooks.lua.  Alternatively you can put
-- this in a separate file and `dofile' it.
--
-- 2. Edit your `console_hook_functions' table in hooks.lua. e.g.
--
--	console_hook_functions = {
--	    ...
--	    md5 = md5,
--	    ...
--	}
--
-- 3. Edit the second last line of this file to point to your
-- download directory.  Sorry, but ELinks can't get this
-- information automatically (yet?).

function md5sum_check(download_dir)
    local results = {}
    string.gsub(current_document(), "([a-z%d]+)  ([^\n]+)\n",
	function (sum, filename)
	    -- lua regexps don't seem to be able to do this
	    if string.len(sum) ~= 32 then return end

	    local fn = download_dir.."/"..filename
	    if file_exists(fn) then
		local localsum = string.gsub(pipe_read("md5sum "..fn),
					     "^([a-z%d]+).*$", "%1")
		table.insert(results, filename.. " -- "..
			     ((sum == localsum) and "ok\n" or "MISMATCH!\n"))
	    end
	end)

    local tmp = tmpname()..'.txt'
    writeto(tmp)
    results.n = nil
    for i,v in results do write(v) end
    writeto()
    table.insert(tmp_files, tmp)
    return 'goto_url', tmp
end

function md5()
    -- Edit this to match your download directory.
    return md5sum_check(home_dir .. "/download")
end
