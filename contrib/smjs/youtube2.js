/* Play videos at YouTube with minimal niggling. Just load the page for a video,
 * and the video will automatically be loaded. */
function load_youtube(cached, vs) {
	var par = cached.uri.match(/http:\/\/\w+\.youtube\.com\/watch\?v=([^&]+).*/);
	if (!par) return true;

	var t = cached.content.match(/, \"t\": \"([^"]+)\"/);
	if (!t) return true;

	var url = 'http://uk.youtube.com/get_video?video_id=' +  par[1] + '&t=' + t[1];

	cached.content = '<a href="' + url + '">View</a>';

	return true;
}
elinks.preformat_html_hooks.push(load_youtube);

/* When one tries to follow a link to <http://www.youtube.com/v/foo>,
 * redirect to <http://www.youtube.com/watch?v=foo>, which has the information
 * that is necessary to get the actual video file. */
function redirect_embedded_youtube(uri) {
	var uri_match = uri.match(/http:\/\/\w+\.youtube\.com\/v\/([^&]+).*/);
	if (!uri_match) {
		return true;
	}
	return 'http://uk.youtube.com/watch?v=' + uri_match[1];
}
elinks.follow_url_hooks.push(redirect_embedded_youtube);
