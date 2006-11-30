#!/bin/sh
#
# Print all variables set by ELinks
#

cat <<EOF
Content-Type: text/html

<html>
	<head><title>CGI variables</title></head>
<body><table>
EOF

for var in \
	CONTENT_LENGTH \
	GATEWAY_INTERFACE \
	HTTP_ACCEPT \
	HTTP_ACCEPT_LANGUAGE \
	HTTP_CACHE_CONTROL \
	HTTP_COOKIE \
	HTTP_IF_MODIFIED_SINCE \
	HTTP_PRAGMA \
	HTTP_REFERER \
	HTTP_USER_AGENT \
	PATH_TRANSLATED \
	QUERY_STRING \
	REDIRECT_STATUS \
	REMOTE_ADDR \
	REQUEST_METHOD \
	SCRIPT_FILENAME \
	SCRIPT_NAME \
	SERVER_NAME \
	SERVER_PROTOCOL \
	SERVER_SOFTWARE;
do
	eval val=$`echo $var`
	echo "<tr><td><em>$var</em></td><td>$val</td></tr>";
done

cat <<EOF
</table>
</body>
</html>
EOF

