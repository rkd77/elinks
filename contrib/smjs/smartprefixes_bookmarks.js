/* Modern, bookmark-based smartprefixes */

var loaded_smartprefixes_common_code;
if (!loaded_smartprefixes_common_code) {
	do_file(elinks.home + "smartprefixes_common.js");
	loaded_smartprefixes_common_code = 1;
}

/* Create a top-level folder titled "smartprefixes". In it, add a bookmark
 * for each smartprefix, putting the keyword in the title and either a normal
 * URI or some JavaScript code prefixed with "javascript:" as the URI. When you
 * enter the keyword in the Go to URL box, ELinks will take the URI
 * of the corresponding bookmark, replace any occurrence of "%s" with the rest
 * of the text entered in the Go to URL box, evaluate the code if the URI
 * starts with "javascript:", and go to the resulting URI.
 */
function rewrite_uri(uri) {
	if (!elinks.bookmarks.smartprefixes) return uri;

	var parts = uri.split(" ");
	var prefix = parts[0];

	if (!elinks.bookmarks.smartprefixes.children[prefix]) return uri;

	var rule = elinks.bookmarks.smartprefixes.children[prefix].url;
	var rest = parts.slice(1).join(" ");

	if (rule.match(/^javascript:/))
		return eval(rule
		             .replace(/^javascript:/, "")
		             .replace(/%s/, rest));

	return rule.replace(/%s/, escape(rest));
}
elinks.goto_url_hooks.push(rewrite_uri);
