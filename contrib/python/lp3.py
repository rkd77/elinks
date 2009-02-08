import re

PATTERN = re.compile('<a href="javascript:void\(null\);" onclick="play\(this,\'(http://lp3.polskieradio.pl/_files/mp3/.*mp3)\'\);">')

def zamien(m):

    return '<a href="' + m.group(1) + '">'

def lp3(html):

    return PATTERN.sub(zamien, html)
