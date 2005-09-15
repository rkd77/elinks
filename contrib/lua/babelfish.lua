function babelfish (url)
    local from, to, rest
    local lang_pair
    local param
    local lang2code = {
        ["chinese-simp"] = 'zh',
        ["chinese-simple"] = 'zh',
        ["chinese-simplified"] = 'zh',
        ["chinese-trad"] = 'zt',
        ["chinese-traditional"] = 'zt',
        ["dutch"] = 'nl',
        ["nederlands"] = 'nl',
        ["Nederlands"] = 'nl',
        ["german"] = 'de',
        ["deutsch"] = 'de',
        ["Deutsch"] = 'de',
        ["english"] = 'en',
        ["french"] = 'fr',
        ["fran\231ais"] = 'fr',
        ["greek"] = 'el',
        ["italian"] = 'it',
        ["italiano"] = 'it',
        ["japanese"] = 'ja',
        ["korean"] = 'ko',
        ["portuguese"] = 'pt',
        ["portugu\234s"] = 'pt',
        ["russian"] = 'ru',
        ["spanish"] = 'es',
        ["espanol"] = 'es',
        ["espa\241ol"] = 'es',
    }

    _,_,from,to,rest = string.find(url, '^bb%s*([^%s]+)[%s]+([^%s]+)[%s]*(.*)')

    if not rest then return url,nil end

    from = lang2code[from] or from
    to = lang2code[to] or to

    lang_pair = from..'_'..to

    if string.find(rest, ':[^%s]') then
        url = "http://babelfish.altavista.com/babelfish/urltrurl"
               .."?url="..escape(rest)
               .."&lp="..lang_pair
    else
        url = "http://babelfish.altavista.com/babelfish/tr"
               .."?trtext="..escape(rest)
               .."&lp="..lang_pair
    end

    return url,true
end

table.insert(goto_url_hooks, babelfish)
