/* Play videos at video.google.com with minimal niggling. Just follow the link
 * from the front page or the search page, and the video will automatically
 * be loaded. */
function load_google_video(cached) {
	if (cached.uri.match(/^http:\/\/video.google.com\/videoplay/)) {
		var re;
		re = /(<object data="\/googleplayer.swf\?videoUrl=)(.*?)(\&)/;
		var uri = cached.content.match(re)[2];

		if (uri) elinks.location = unescape(uri);
	}

	return true;
}
elinks.preformat_html_hooks.push(load_google_video);
