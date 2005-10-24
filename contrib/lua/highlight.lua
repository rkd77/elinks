-- Source-code highlighting hook

highlight_file_types = {
    patch = "%.patch$",
    python = "%.py$",
    perl = "%.pl$",
    awk = "%.awk$",
    c = "%.[ch]$",
    sh = "%.sh$",
}

function highlight (url, html)
    for language,pattern in pairs(highlight_file_types) do
        if string.find (url, pattern) then
            local tmp = tmpname ()
            writeto (tmp) write (html) writeto()
            html = pipe_read ("(code2html -l "..language.." "..tmp.." - ) 2>/dev/null")
            os.remove(tmp)
            return html,nil
        end
    end

    return nil,nil
end

table.insert(pre_format_html_hooks, highlight)
