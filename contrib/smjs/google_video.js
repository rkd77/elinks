/* Play videos at video.google.com with minimal niggling. Just follow the link
 * from the front page or the search page, and the video will automatically
 * be loaded. */
function load_google_video(cached, vs) {
	if (!cached.uri.match(/^http:\/\/video.google.com\/videoplay/))
		return true;

	var re = /(?:videoUrl(?:\\u003d|=))(.*?)\&/;
	var match = cached.content.match(re);
	var url = unescape(match[1]);
	var meta = '<meta http-equiv="refresh" content="1; url=' + url + '" />';

	cached.content = cached.content.replace(/<head>/, "<head>" + meta);

	return true;
}
elinks.preformat_html_hooks.push(load_google_video);
