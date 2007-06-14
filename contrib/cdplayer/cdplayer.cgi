#!/bin/sh
cat <<EOF
Content-Type: text/html
Cache-Control: no-cache

<html>
<head>
<title> CD Player</title>
</head>
<body>
<center>
<table border="1" cols="5" rows="2">
<tr>
<td colspan="5">
<h3> CD Player 2007 </h3>
</td>
</tr>
<tr>
<form action="cdplayer.cgi">
<td> <input type="submit" name="key" value="<<" />
</td>
<td> <input type="submit" name="key" value="<" />
</td>
<td> <input type="submit" name="key" value=">" />
</td>
<td> <input type="submit" name="key" value=">>" />
</td>
<td> <input type="submit" name="key" value="Pause" />
</td>
</form>
</tr>
</table>
</center>
</body>
</html>
EOF
case "$QUERY_STRING" in
	"key=%3c%3c")
	cdplayer -F
	;;
	"key=%3e%3e")
	cdplayer -La
	;;
	"key=%3c")
	cdplayer -Pr
	;;
	"key=%3e")
	cdplayer -N
	;;
	"key=Pause")
	cdplayer -Pa
	;;
esac
