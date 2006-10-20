/* Play videos at YouTube with minimal niggling. Just load the page for a video,
 * and the video will automatically be loaded. */
function load_youtube(cached, vs) {
	if (!cached.uri.match(/http:\/\/(?:www\.)?youtube\.com\/(?:watch(?:\.php)?)?\?v=(?:[^&]+).*/))
		return true;

	var params_match = cached.content.match(/player2\.swf\?([^"]+)"/);
	if (!params_match) return true;

	var url = 'http://www.youtube.com/get_video?' + params_match[1];
	var meta = '<meta http-equiv="refresh" content="1; url=' + url + '" />';

	cached.content = cached.content.replace(/<head>/, "<head>" + meta);

	return true;
}
elinks.preformat_html_hooks.push(load_youtube);

/* When one tries to follow a link to <http://www.youtube.com/v/foo>,
 * redirect to <http://www.youtube.com/watch?v=foo>, which has the information
 * that is necessary to get the actual video file. */
function redirect_embedded_youtube(uri) {
	var uri_match = uri.match(/http:\/\/(?:www\.)?youtube\.com\/v\/([^&]+).*/);
	if (!uri_match) return true;
	return 'http://www.youtube.com/watch?v=' + uri_match[1];
}
elinks.follow_url_hooks.push(redirect_embedded_youtube);
