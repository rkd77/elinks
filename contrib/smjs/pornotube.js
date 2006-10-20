/* Play videos at pornotube.com with minimal niggling. Just follow the link
 * from the front page or the search page, and the video will automatically
 * be loaded. */

function pornotube_callback(cached) {
	var user_re = /\<user_id\>(\d+)\<\/user_id\>/;
	var media_re = /\<media_id\>(\d+)\<\/media_id\>/;

	var user = cached.content.match(user_re)[1];
	var media = cached.content.match(media_re)[1];

	var uri = "http://video.pornotube.com/" + user + "/" + media + ".flv";

	elinks.location = uri;
}

function load_pornotube(cached, vs) {
	if (!cached.uri.match(/^http:\/\/(?:www\.)?pornotube\.com\/media\.php/))
		return true;

	var re = /SWFObject\("player\/v\.swf\?v=(.*?)"/;
	var hash = cached.content.match(re)[1];

	var uri = "http://www.pornotube.com/player/player.php?" + hash;

	elinks.load_uri(uri, pornotube_callback);

	return true;
}
elinks.preformat_html_hooks.push(load_pornotube);
