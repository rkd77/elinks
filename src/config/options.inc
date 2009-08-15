/* Option definitions. */

/* TODO: Autogenerate this file from something more human readable. --jonas */
/* TODO: Make #ifdef FEATURE more consistent. */

/* A few guidelines for this file: */
/* 1. Keep options in alphabetical order with trees as the first children
 *    of their parents.
 * 2. Keep in mind that descriptions should be readable on 80 wide terminals
 *    so insert appropriate line breaks and newlines.
 * 3. If several command line options should share the same caption and
 *    description they should be declared after another and only the last one
 *    should have the caption and description defined. (ie. -h, -? and -help)
 * 4. When adding an alias put a comment about the date the alias was added.
 * 5. Love thy option system! :) */

static union option_info config_options_info[] = {
	/* [gettext_accelerator_context(IGNORE)] */
	INIT_OPT_TREE("", N_("Configuration system"),
		"config", 0,
		N_("Configuration handling options.")),

	INIT_OPT_INT("config", N_("Comments"),
		"comments", 0, 0, 3, 3,
		N_("Amount of comments automatically written to the config "
		"file:\n"
		"0 is no comments are written\n"
		"1 is only the \"blurb\" (name+type) is written\n"
		"2 is only the description is written\n"
		"3 is full comments are written")),

	INIT_OPT_INT("config", N_("Indentation"),
		"indentation", 0, 0, 16, 2,
		N_("Shift width of one indentation level in the configuration "
		"file. Zero means that no indentation is performed at all "
		"when saving the configuration.")),

	INIT_OPT_INT("config", N_("Saving style"),
		"saving_style", 0, 0, 3, 3,
		N_("Determines what happens when you tell ELinks to save "
		"options:\n"
		"0 is only values of current options are altered\n"
		"1 is values of current options are altered and missing\n"
		"     options are added at the end of the file\n"
		"2 is the configuration file is rewritten from scratch\n"
		"3 is values of current options are altered and missing\n"
		"     options CHANGED during this ELinks session are added at\n"
		"     the end of the file")),

	INIT_OPT_BOOL("config", N_("Comments localization"),
		"i18n", 0, 0,
		N_("If set to 1, comments in the configuration file will be "
		"translated to the language used by UI. Note that if you have "
		"different language set in different terminals, the language "
		"used in the configuration file MAY be the same as on the "
		"terminal where you saved the file, but it should be "
		"generally considered unpredictable.")),


	INIT_OPT_BOOL("config", N_("Saving style warnings"),
		"saving_style_w", 0, 0,
		N_("This is internal option used when displaying a warning "
		"about obsolete config.saving_style. You shouldn't touch "
		"it.")),

	INIT_OPT_BOOL("config", N_("Show template"),
		"show_template", 0, 0,
		N_("Show template options in autocreated trees in the options "
		"manager and save them to the configuration file.")),

	/* Keep options in alphabetical order. */


	INIT_OPT_TREE("", N_("Connections"),
		"connection", OPT_SORT,
		N_("Connection options.")),


	INIT_OPT_BOOL("connection", N_("Asynchronous DNS"),
		"async_dns", 0, 1,
		N_("Whether to use asynchronous DNS resolving.")),

	INIT_OPT_INT("connection", N_("Maximum connections"),
		"max_connections", 0, 1, 16, 10,
		N_("Maximum number of concurrent connections.")),

	INIT_OPT_INT("connection", N_("Maximum connections per host"),
		"max_connections_to_host", 0, 1, 8, 2,
		N_("Maximum number of concurrent connections to a given "
		"host.")),

	INIT_OPT_INT("connection", N_("Connection retries"),
		"retries", 0, 0, 16, 3,
		N_("Number of tries to establish a connection. "
		"Zero means try forever.")),

	INIT_OPT_INT("connection", N_("Receive timeout"),
		"receive_timeout", 0, 1, 1800, 120,
		N_("Receive timeout (in seconds).")),

#ifdef CONFIG_IPV6
	INIT_OPT_BOOL("connection", N_("Try IPv4 when connecting"),
		"try_ipv4", 0, 1,
		N_("Whether to try to connect to a host over IPv4. "
		"Note that if connection.try_ipv6 is enabled too, "
		"it takes precedence. And better do not touch this "
		"at all unless you are sure what are you doing.\n"
		"\n"
		"Note that you can also force a given protocol "
		"to be used on a per-connection basis by using a URL "
		"in the style of e.g. http4://elinks.cz/.")),
#else
	INIT_OPT_BOOL("connection", N_("Try IPv4 when connecting"),
		"try_ipv4", 0, 1,
		N_("Whether to try to connect to a host over IPv4. "
		"Do not touch this option.\n"
		"\n"
		"Note that you can also force a given protocol "
		"to be used on a per-connection basis by using a URL "
		"in the style of e.g. http4://elinks.cz/.")),
#endif

#ifdef CONFIG_IPV6
	INIT_OPT_BOOL("connection", N_("Try IPv6 when connecting"),
		"try_ipv6", 0, 1,
		N_("Whether to try to connect to a host over IPv6.\n"
		"\n"
		"Note that you can also force a given protocol "
		"to be used on a per-connection basis by using a URL "
		"in the style of e.g. http6://elinks.cz/.")),
#endif

	INIT_OPT_INT("connection", N_("Timeout for non-restartable connections"),
		"unrestartable_receive_timeout", 0, 1, 1800, 600,
		N_("Timeout for non-restartable connections (in seconds).")),

	/* Keep options in alphabetical order. */



	INIT_OPT_TREE("", N_("Document"),
		"document", OPT_SORT,
		N_("Document options.")),

	INIT_OPT_TREE("document", N_("Browsing"),
		"browse", 0,
		N_("Document browsing options (mainly interactivity).")),


	INIT_OPT_TREE("document.browse", N_("Access keys"),
		"accesskey", 0,
		N_("Options for handling of link access keys. "
		"An HTML document can use the ACCESSKEY attribute to assign "
		"an access key to an element. When an access key is pressed, "
		"the corresponding element will be given focus.")),

	INIT_OPT_BOOL("document.browse.accesskey", N_("Automatic links following"),
		"auto_follow", 0, 0,
		N_("Automatically follow a link or submit a form if "
		"appropriate accesskey is pressed - this is the standard "
		"behaviour, but it's considered dangerous.")),

	INIT_OPT_BOOL("document.browse.accesskey", N_("Display access key in link info"),
		"display", 0, 0,
		N_("Display access key in link info.")),

	INIT_OPT_INT("document.browse.accesskey", N_("Accesskey priority"),
		"priority", 0, 0, 2, 0,
		N_("Priority of 'accesskey' HTML attribute:\n"
		"0 is first try all normal bindings; if it fails,\n"
		"     check accesskey\n"
		"1 is first try only frame bindings; if it fails,\n"
		"     check accesskey\n"
		"2 is first check accesskey (this can be dangerous)")),


	INIT_OPT_TREE("document.browse", N_("Forms"),
		"forms", 0,
		N_("Options for handling of the forms interaction.")),

	INIT_OPT_BOOL("document.browse.forms", N_("Submit form automatically"),
		"auto_submit", 0, 1,
		N_("Automagically submit a form when enter is pressed with a "
		"text field selected.")),

	INIT_OPT_BOOL("document.browse.forms", N_("Confirm submission"),
		"confirm_submit", 0, 1,
		N_("Ask for confirmation when submitting a form.")),

	INIT_OPT_INT("document.browse.forms", N_("Default form input size"),
		"input_size", 0, 3, 300, HTML_DEFAULT_INPUT_SIZE,
		N_("Default form input size if none is specified.")),

	INIT_OPT_BOOL("document.browse.forms", N_("Insert mode"),
		"insert_mode", 0, 1,
		N_("The setting for this option affects how key presses are "
		"handled when one selects a text-input form-field. When "
		"enabled, one must explicitly 'enter' a selected text-field "
		"to edit it; this prevents text fields from capturing key "
		"presses, such as presses of a scroll key, when it is "
		"inadvertently selected. When disabled, key presses are "
		"always inserted into a selected text field.")),

	INIT_OPT_STRING("document.browse.forms", N_("External editor"),
		"editor", 0, "",
		N_("Path to the executable that ELinks should launch when the "
		"user requests to edit a textarea with an external editor.\n"
		"\n"
		"If this is blank, ELinks will use the value of the "
		"environmental variable $EDITOR. If $EDITOR is empty or not "
		"set, ELinks will then default to \"vi\".")),


	INIT_OPT_TREE("document.browse", N_("Images"),
		"images", 0,
		N_("Options for handling of images.")),

	INIT_OPT_INT("document.browse.images", N_("Display style for image tags"),
		"display_style", 0, 0, 3, 2,
		N_("Display style for image tags when displayed:\n"
		"0     means always display IMG\n"
		"1     means always display filename\n"
		"2     means display alt/title attribute if possible,\n"
		"      IMG if not\n"
		"3     means display alt/title attribute if possible,\n"
		"      filename if not")),

	INIT_OPT_INT("document.browse.images", N_("Maximum length for image filename"),
		"filename_maxlen", 0, 0, 500, 0,
		N_("Maximum length of image filename when displayed:\n"
		"0     means always display full filename\n"
		"1-500 means display filename with this maximal length;\n"
		"      if it is longer, the middle is substituted by\n"
		"      an asterisk")),

	/* Compatibility aliases. Added: 2004-12-15, 0.10pre3.CVS. It was
	 * replaced by display_style and filename_maxlen options combination. */
	INIT_OPT_ALIAS("document.browse.images", "file_tags", 0,
		"document.browse.images.filename_maxlen"),

	INIT_OPT_INT("document.browse.images", N_("Image links tagging"),
		"image_link_tagging", 0, 0, 2, 1,
		N_("When to enclose image links:\n"
		"0 means never\n"
		"1 means never if alt or title are provided (old behavior)\n"
		"2 means always")),

	INIT_OPT_STRING("document.browse.images", N_("Image link prefix"),
		"image_link_prefix", 0, "[",
		N_("Prefix string to use to mark image links.")),

	INIT_OPT_STRING("document.browse.images", N_("Image link suffix"),
		"image_link_suffix", 0, "]",
		N_("Suffix string to use to mark image links.")),

	INIT_OPT_INT("document.browse.images", N_("Maximum length for image label"),
		"label_maxlen", 0, 0, 500, 0,
		N_("Maximum length of image label (alt/title):\n"
		"0     means always display full label\n"
		"1-500 means display label with this maximal length;\n"
		"      if it is longer, the middle is substituted by\n"
		"      an asterisk")),

	INIT_OPT_BOOL("document.browse.images", N_("Display links to images w/o alt"),
		"show_as_links", 0, 0,
		N_("Display links to images without an alt attribute. If this "
		"option is off, these images are completely invisible.")),

	INIT_OPT_BOOL("document.browse.images", N_("Display links to images"),
		"show_any_as_links", 0, 1,
		N_("Display links to any images in the document, regardless "
		"of them having an alt attribute or not. If this option is "
		"off, the alt attribute contents is shown, but as normal "
		"text, not selectable as a link.")),


	INIT_OPT_TREE("document.browse", N_("Links"),
		"links", 0,
		N_("Options for handling of links to other documents.")),

	INIT_OPT_TREE("document.browse.links", N_("Active link"),
		"active_link", 0,
		N_("Options for the active link.")),

	INIT_OPT_TREE("document.browse.links.active_link", N_("Colors"),
		"colors", 0,
		N_("Active link colors.")),

	INIT_OPT_COLOR("document.browse.links.active_link.colors", N_("Background color"),
		"background", 0, "#0000ff",
		N_("Default background color.")),

	INIT_OPT_COLOR("document.browse.links.active_link.colors", N_("Text color"),
		"text", 0, "black",
		N_("Default text color.")),

	INIT_OPT_BOOL("document.browse.links.active_link", N_("Enable color"),
		"enable_color", 0, 0,
		N_("Enable use of the active link background and text color "
		"settings instead of the link colors from the document.")),

	INIT_OPT_BOOL("document.browse.links.active_link", N_("Bold"),
		"bold", 0, 0,
		N_("Make the active link text bold.")),

	INIT_OPT_BOOL("document.browse.links.active_link", N_("Invert colors"),
		"invert", 0, 1,
		N_("Invert the fore- and background color so the link "
		"stands out.")),

	INIT_OPT_BOOL("document.browse.links.active_link", N_("Underline"),
		"underline", 0, 0,
		N_("Underline the active link.")),


	INIT_OPT_BOOL("document.browse.links", N_("Directory highlighting"),
		"color_dirs", 0, 1,
		N_("Highlight links to directories in FTP and local directory "
		"listing.")),

	INIT_OPT_BOOL("document.browse.links", N_("Number links"),
		"numbering", 0, 0,
		N_("Display numbers next to the links.")),

	INIT_OPT_INT("document.browse.links", N_("Handling of target=_blank"),
		"target_blank", 0, 0, 3, 0,
		N_("Define how to handle links having target=_blank set:\n"
		"0 means open link in current tab\n"
		"1 means open link in new tab in foreground\n"
		"2 means open link in new tab in background\n"
		"3 means open link in new window")),

	/* Compatibility aliases. Added: 2004-01-07, 0.9.0.CVS. */
	INIT_OPT_ALIAS("document.browse.links", "typeahead_wraparound", 0,
		"document.browse.search.wraparound"),

	INIT_OPT_ALIAS("document.browse.links", "typeahead_error", 0,
		"document.browse.search.show_not_found"),

	INIT_OPT_BOOL("document.browse.links", N_("Use tabindex"),
		"use_tabindex", 0, 1,
		N_("Whether to navigate links using tabindex specified "
		"ordering. The TABINDEX attribute in HTML elements specifies "
		"the order in which links should receive focus when using the "
		"keyboard to navigate the document.")),

	INIT_OPT_BOOL("document.browse.links", N_("Missing fragment reporting"),
		"missing_fragment", 0, 1,
		N_("Open a message box when document has no tag with given "
		"id.")),

	INIT_OPT_INT("document.browse.links", N_("Number keys select links"),
		"number_keys_select_link", 0, 0, 2, 1,
		N_("Number keys select links rather than specify command "
		"prefixes. This is a tristate:\n"
		"0 means never\n"
		"1 means if document.browse.links.numbering = 1\n"
		"2 means always")),

	INIT_OPT_BOOL("document.browse.links", N_("Warn about maliciously crafted URIs"),
		"warn_malicious", 0, 1,
		N_("When following a link the user ID part of the URI is "
		"checked and if a maliciously crafted URI is detected a "
		"warning dialog will ask before following the link.")),

	/* TODO - this is somehow implemented by ff, but disabled
	 * for now as it doesn't work. */
	INIT_OPT_BOOL("document.browse.links", N_("Wrap-around links cycling"),
		"wraparound", /* 0 */ 0, 0,
		N_("When pressing 'down' on the last link, jump to the first "
		"one, and vice versa.")),


	INIT_OPT_TREE("document.browse", N_("Scrolling"),
		"scrolling", OPT_SORT,
		N_("Scrolling options.")),

	INIT_OPT_INT("document.browse.scrolling", N_("Horizontal step"),
		"horizontal_step", 0, 1, 9999, 8,
		N_("Number of columns to scroll when a key bound to "
		"scroll-left or scroll-right is pressed and no prefix was "
		"given.")),

	INIT_OPT_BOOL("document.browse.scrolling", N_("Extended horizontal scrolling"),
		"horizontal_extended", 0, 1,
		N_("Whether to allow horizontal scrolling when the document "
		"does not extend off the screen. Useful for copy/paste "
		"operations.")),

	INIT_OPT_INT("document.browse.scrolling", N_("Margin"),
		"margin", 0, 0, 20, 3,
		N_("Size of the virtual margin - when you click inside of "
		"that margin, document scrolls in that direction.")),

	INIT_OPT_INT("document.browse.scrolling", N_("Vertical step"),
		"vertical_step", 0, 1, 9999, 2,
		N_("Number of lines to scroll when a key bound to scroll-up "
		"or scroll-down is pressed and no prefix was given.")),


	INIT_OPT_TREE("document.browse", N_("Searching"),
		"search", 0,
		N_("Options for searching.")),

	INIT_OPT_BOOL("document.browse.search", N_("Case sensitivity"),
		"case", 0, 0,
		N_("Whether the search should match the document text while "
		"maintaining case sensitivity.")),

#ifdef CONFIG_TRE
	INIT_OPT_INT("document.browse.search", N_("Regular expressions"),
		"regex", 0, 0, 2, 0,
		N_("Enable searching with regular expressions:\n"
		"0 for plain text searching\n"
		"1 for basic regular expression searches\n"
		"2 for extended regular expression searches")),
#endif

	INIT_OPT_BOOL("document.browse.search", N_("Show search hit top or bottom dialogs"),
		"show_hit_top_bottom", 0, 1,
		N_("Whether to show a dialog when the search hits the top or "
		"bottom of the document.")),

	INIT_OPT_BOOL("document.browse.search", N_("Wraparound"),
		"wraparound", 0, 1,
		N_("Wrap around when searching. Currently only used for "
		"typeahead.")),

	INIT_OPT_INT("document.browse.search", N_("Show not found"),
		"show_not_found", 0, 0, 2, 2,
		N_("How to inform the user when nothing is matched:\n"
		"0 means do nothing\n"
		"1 means beep the terminal\n"
		"2 means pop up message box")),

	INIT_OPT_INT("document.browse.search", N_("Typeahead searching"),
		"typeahead", 0, 0, 2, 0,
		N_("Start typeahead searching when an unbound key is pressed "
		"without any modifiers. Note that most keys have default "
		"bindings, so this feature will not be useful unless you "
		"unbind them.\n"
		"\n"
		"0 disables this feature; typeahead searching will only be\n"
		"  used when you press a key bound to search-typeahead or\n"
		"  similar\n"
		"1 automatically starts typeahead searching thru link text\n"
		"2 automatically starts typeahead searching thru all document\n"
		"  text")),


	INIT_OPT_INT("document.browse", N_("Horizontal text margin"),
		"margin_width", 0, 0, 9, 3,
		N_("Horizontal text margin.")),

	INIT_OPT_BOOL("document.browse", N_("Document meta refresh"),
		"refresh", 0, 1,
		N_("Automatically follow document-specified refresh "
		"directives ('<meta> refresh' tags). Web-page authors use "
		"these to instruct the browser to reload a document at a "
		"given interval or to load another page. Regardless of the "
		"value the refresh URI is accessible as a link.\n"
		"\n"
		"Use the document.browse.minimum_refresh_time to control the "
		"minimum number of seconds a refresh will wait.")),

	INIT_OPT_INT("document.browse", N_("Document meta refresh minimum time"),
		"minimum_refresh_time", 0, 0, INT_MAX, 1000,
		N_("The minimum number of milliseconds that should pass "
		"before refreshing. If set to zero the document refresh time "
		"is used unchanged. It can fix going back in history for some "
		"sites that use refreshing with zero values.")),

	INIT_OPT_BOOL("document.browse", N_("Tables navigation order"),
		"table_move_order", 0, 0,
		N_("Move by columns in table, instead of rows.")),

	/* Keep options in alphabetical order. */



	INIT_OPT_TREE("document", N_("Cache"),
		"cache", OPT_SORT,
		N_("Cache options.")),

	INIT_OPT_BOOL("document.cache", N_("Cache information about redirects"),
		"cache_redirects", 0, 0,
		N_("Cache even redirects sent by server (usually thru HTTP by "
		"a 302 HTTP code and a Location header). This was the "
		"original behaviour for quite some time, but it causes "
		"problems in a situation very common to various web login "
		"systems - frequently, when accessing a certain location, "
		"they will redirect you to a login page if they don't receive "
		"an auth cookie, the login page then gives you the cookie and "
		"redirects you back to the original page, but there you have "
		"already cached redirect back to the login page! If this "
		"option has value of 0, this malfunction is fixed, but "
		"occasionally you may get superfluous (depends on how you "
		"take it ;-) requests to the server. If this option has value "
		"of 1, experienced users can still workaround it by clever "
		"combination of usage of reload, jumping around in session "
		"history and hitting ctrl+enter.\n"
		"\n"
		"Note that this option is checked when retrieving the "
		"information from cache, not when saving it to cache - thus "
		"if you enable it, even previous redirects will be taken from "
		"cache instead of asking the server.")),

	INIT_OPT_BOOL("document.cache", N_("Ignore cache-control info from server"),
		"ignore_cache_control", 0, 1,
		N_("Ignore Cache-Control and Pragma server headers. "
		"When set, the document is cached even with 'Cache-Control: "
		"no-cache'.")),

	INIT_OPT_TREE("document.cache", N_("Formatted documents"),
		"format", 0,
		N_("Format cache options.")),

	INIT_OPT_INT("document.cache.format", N_("Number"),
		"size", 0, 0, 256, 5,
		N_("Number of cached formatted pages. Do not get too "
		"generous here, 'formatted' means that all the accompanying "
		"structures are kept in memory so that you get the cached "
		"document immediatelly, but these structures may take a lot "
		"- 2x the size of the HTML source is probably not unusual, "
		"but it can be even more if the document consists of a lot "
		"of short lines (padded right, if possible) and links and "
		"not much other markup. So if you set this to 256 and then "
		"you don't like your ELinks eating 90M, don't come "
		"complaining to us. ;-)\n"
		"\n"
		"Also note that the format cache itself is not counted to "
		"the memory cache size, but the HTML source of the formatted "
		"documents is always cached, even if it is over the memory "
		"cache size threshold. (Then of course no other documents "
		"can be cached.)")),

	/* FIXME: Write more. */
	INIT_OPT_INT("document.cache", N_("Revalidation interval"),
		"revalidation_interval", 0, -1, 86400, -1,
		N_("Period in seconds that a cache entry is considered to be "
		"up-to-date. When a document is loaded and this interval has "
		"elapsed since the document was initially loaded or most "
		"recently revalidated with the server, the server will be "
		"checked in case there is a more up-to-date version of the "
		"document.\n"
		"\n"
		"A value of -1 disables automatic revalidation.")),

	INIT_OPT_TREE("document.cache", N_("Memory cache"),
		"memory", 0,
		N_("Memory cache options.")),

	INIT_OPT_LONG("document.cache.memory", N_("Size"),
		"size", 0, 0, LONG_MAX, 1048576,
		N_("Memory cache size (in bytes).")),



	INIT_OPT_TREE("document", N_("Charset"),
		"codepage", 0,
		N_("Charset options.")),

	INIT_OPT_CODEPAGE("document.codepage", N_("Default codepage"),
		"assume", 0, "System",
		N_("Default document codepage. 'System' stands for "
		"a codepage determined by a selected locale.")),

	INIT_OPT_BOOL("document.codepage", N_("Ignore charset info from server"),
		"force_assumed", 0, 0,
		N_("Ignore charset info sent by server.")),



	INIT_OPT_TREE("document", N_("Default color settings"),
		"colors", 0,
		N_("Default document color settings.")),


	INIT_OPT_COLOR("document.colors", N_("Text color"),
		"text", 0, "#bfbfbf",
		N_("Default text color.")),

	INIT_OPT_COLOR("document.colors", N_("Background color"),
		"background", 0, "#000000",
		N_("Default background color.")),

	INIT_OPT_COLOR("document.colors", N_("Link color"),
		"link", 0, "#0000ff",
		N_("Default link color.")),

	INIT_OPT_COLOR("document.colors", N_("Visited-link color"),
		"vlink", 0, "#ffff00",
		N_("Default visited link color.")),

	INIT_OPT_COLOR("document.colors", N_("Image-link color"),
		"image", 0, "darkolivegreen",
		N_("Default image link color.")),

	INIT_OPT_COLOR("document.colors", N_("Bookmarked-link color"),
		"bookmark", 0, "hotpink",
		N_("Default bookmarked link color.")),

	INIT_OPT_COLOR("document.colors", N_("Directory color"),
		"dirs", 0, "#ffff00",
		N_("Default directory color.\n"
		"\n"
		"See document.browse.links.color_dirs option.")),

	/* Compatibility alias: added by jonas at 2005-05-31, 0.11.CVS. */
	INIT_OPT_ALIAS("document.colors", "allow_dark_on_black", OPT_ALIAS_NEGATE,
		"document.colors.increase_contrast"),

	INIT_OPT_BOOL("document.colors", N_("Increase contrast"),
	        "increase_contrast", 0, 1,
	        N_("Increase the contrast between the foreground and "
		"background colors to ensure readability. For example it "
		"disallows dark colors on a black background. Note, this "
		"is different from ensuring the contrast with the "
		"ensure_contrast option.")),

	INIT_OPT_BOOL("document.colors", N_("Ensure contrast"),
		"ensure_contrast", 0, 1,
		N_("Makes sure that the back- and foreground colors are never "
		"equal.")),

	/* If you change this please also change ACT_MAIN_DOCUMENT_COLORS action
	 * handling. */
	INIT_OPT_INT("document.colors", N_("Use document-specified colors"),
		"use_document_colors", 0, 0, 2, 2,
		N_("Use colors specified in document:\n"
		"0 is use always the default settings\n"
		"1 is use document colors if available, except background\n"
		"2 is use document colors, including background. This can\n"
		"  mostly look very impressive, but some sites will appear\n"
		"  really ugly. Note, that obviously if the background is\n"
		"  not black, it will break the behaviour of transparency.")),


	/* Keep options in alphabetical order. */



	INIT_OPT_TREE("document", N_("Downloading"),
		"download", 0,
		N_("Options regarding files downloading and handling.")),

	INIT_OPT_STRING("document.download", N_("Default download directory"),
		"directory", 0, "./",
		N_("Default download directory.")),

	INIT_OPT_BOOL("document.download", N_("Set original time"),
		"set_original_time", 0, 0,
		N_("Set the timestamp of each downloaded file to the "
		"timestamp stored on the server.")),

	/* Does automatic resuming make sense as an option? */
	INIT_OPT_INT("document.download", N_("Prevent overwriting"),
		"overwrite", 0, 0, 2, 2,
		N_("Prevent overwriting the local files:\n"
		"0 is files will silently be overwritten\n"
		"1 is add a suffix .{number} (for example '.1') to the name\n"
		"2 is ask the user")),

	INIT_OPT_INT("document.download", N_("Notify download completion by bell"),
		"notify_bell", 0, 0, 2, 0,
		N_("Audio notification when download is completed:\n"
		"0 is never\n"
		"1 is when background notification is active\n"
		"2 is always")),


	INIT_OPT_TREE("document", N_("Dump output"),
		"dump", 0,
		N_("Dump output options.")),

	INIT_OPT_CODEPAGE("document.dump", N_("Codepage"),
		"codepage", 0, "System",
		N_("Codepage used in dump output. 'System' stands for "
		"a codepage determined by a selected locale.")),

	INIT_OPT_INT("document.dump", N_("Color mode"),
		"color_mode", 0, -1, COLOR_MODES - 1, -1,
		/* The list of modes must be at the end of this string
		 * because AsciiDoc 7.1.2 does not support continuing
		 * an outer list entry after an inner list.
		 * TRANSLATORS: This restriction applies only to the
		 * "en" (English) translation.  (See doc/Makefile.)  */
		N_("Color mode for dumps.\n"
		"\n"
		"Some modes may have been disabled at compile time. "
		"The Setup -> Terminal options dialog lists the modes "
		"supported by this executable. If you select an "
		"unsupported mode, ELinks uses 16 colors.\n"
		"\n"
		"The color modes are:\n"
		"-1 is standard dump mode\n"
		"0 is mono mode\n"
		"1 is 16 color mode\n"
		"2 is 88 color mode\n"
		"3 is 256 color mode\n"
		"4 is true color mode")),

	INIT_OPT_STRING("document.dump", N_("Footer"),
		"footer", 0, "",
		N_("Footer string used in dumps. %u is substituted by URL.")),

	INIT_OPT_STRING("document.dump", N_("Header"),
		"header", 0, "",
		N_("Header string used in dumps. %u is substituted by URL.")),

	INIT_OPT_BOOL("document.dump", N_("Numbering"),
		"numbering", 0, 1,
		N_("Whether to print link numbers in dump output.")),

	INIT_OPT_BOOL("document.dump", N_("References"),
		"references", 0, 1,
		N_("Whether to print references (URIs) of document links "
		"in dump output.")),

	INIT_OPT_STRING("document.dump", N_("Separator"),
		"separator", 0, "\n\n",
		N_("String which separates two dumps.")),

	INIT_OPT_INT("document.dump", N_("Width"),
		"width", 0, 1, 65536, DEFAULT_TERMINAL_WIDTH,
		N_("Width of screen in characters when dumping documents.")),



	INIT_OPT_TREE("document", N_("History"),
		"history", OPT_SORT,
		N_("History options.")),

	INIT_OPT_BOOL("document.history", N_("Keep unhistory"),
		"keep_unhistory", 0, 1,
		N_("Keep unhistory (\"forward history\").")),


	INIT_OPT_TREE("document", N_("HTML rendering"),
		"html", 0,
		N_("Options concerning the display of HTML pages.")),

	INIT_OPT_BOOL("document.html", N_("Display frames"),
		"display_frames", 0, 1,
		N_("Display frames.")),

	INIT_OPT_BOOL("document.html", N_("Display tables"),
		"display_tables", 0, 1,
		N_("Display tables.")),

	INIT_OPT_BOOL("document.html", N_("Display subscripts"),
		"display_subs", 0, 1,
		N_("Display subscripts (as [thing]).")),

	INIT_OPT_BOOL("document.html", N_("Display superscripts"),
		"display_sups", 0, 1,
		N_("Display superscripts (as ^thing).")),

	INIT_OPT_INT("document.html", N_("Rendering of HTML link element"),
		"link_display", 0, 0, 5, 2,
		N_("How to render <link> tags from the HTML header:\n"
		"0 is nothing\n"
		"1 is title\n"
		"2 is name in addition\n"
		"3 is hreflang in addition\n"
		"4 is type in addition\n"
		"5 is everything")),

	INIT_OPT_BOOL("document.html", N_("Underline links"),
		"underline_links", 0, 0,
		N_("Underline links.")),

	INIT_OPT_BOOL("document.html", N_("Wrap non breaking space"),
		"wrap_nbsp", 0, 0,
		N_("If set do not honour non breaking space (the nbsp entity) "
		"but allow to wrap the text. This can help keeping the width "
		"of documents down so no horizontal scrolling is needed.")),


	INIT_OPT_TREE("document", N_("Plain rendering"),
		"plain", 0,
		N_("Options concerning the display of plain text pages.")),

	INIT_OPT_BOOL("document.plain", N_("Display URIs"),
		"display_links", 0, 0,
		N_("Display URIs in the document as links.")),

	INIT_OPT_BOOL("document.plain", N_("Compress empty lines"),
		"compress_empty_lines", 0, 0,
		N_("Compress successive empty lines to only one in displayed "
		"text.")),


	INIT_OPT_TREE("document", N_("URI passing"),
		"uri_passing", OPT_SORT | OPT_AUTOCREATE,
		N_("Rules for passing URIs to external commands. When one "
		"rule is defined the link and tab menu will have a menu item "
		"that makes it possible to pass the the link, frame or tab "
		"URI to an external command. If several rules are defined the "
		"link and tab menu will have a submenu of items for each "
		"rule.\n"
		"\n"
		"Note, this is mostly useful for launching graphical viewers, "
		"since there is no support for releasing the terminal while "
		"the command runs. The action and submenus are also available "
		"by binding keys to the frame-external-command, the "
		"link-external-command, and the tab-external-command "
		"actions.")),

	INIT_OPT_STRING("document.uri_passing", NULL,
		"_template_", 0, "",
		N_("A rule for passing URI to an external command. "
		"The format is:\n"
		"%c in the string means the current URL\n"
		"%% in the string means '%'\n"
		"Do _not_ put single- or double-quotes around %c.")),

	/* Keep options in alphabetical order. */



	INIT_OPT_TREE("", N_("Information files"),
		"infofiles", OPT_SORT,
		N_("Options for information files in ~/.elinks.")),

	INIT_OPT_INT("infofiles", N_("Save interval"),
		"save_interval", 0, 0, INT_MAX, 300,
		N_("Interval at which to trigger information files in "
		"~/.elinks to be saved to disk if they have changed "
		"(seconds; 0 to disable)")),

	INIT_OPT_BOOL("infofiles", N_("Use secure file saving"),
		"secure_save", 0, 1,
		N_("First write data to unique temporary file, then rename "
		"this file upon successfully finishing this. Note that this "
		"relates only to config files, not downloaded files. "
		"You may want to disable it if you are using some exotic "
		"permissions for concerned files. Secure file saving is "
		"automagically disabled if file is symlink.\n"
		"\n"
		"Warning: some systems (ie. OS/2, Win32) require that "
		"destination file doesn't exist when rename(3) is called, "
		"breaking atomicity, and reducing reliability of this "
		"feature.")),

	INIT_OPT_BOOL("infofiles", N_("Use fsync(3) with secure file saving"),
		"secure_save_fsync", 0, 1,
		N_("When using secure file saving, call fsync(3), if the OS "
		"supports it, to force the OS immediately to write the "
		"data to permanent storage. This is optional for those who "
		"wish to avoid excessive disk I/O.")),

	/* Keep options in alphabetical order. */



	INIT_OPT_TREE("", N_("Terminals"),
		"terminal", OPT_AUTOCREATE,
		N_("Terminal options.")),

	INIT_OPT_TREE("terminal", NULL,
		"_template_", 0,
		N_("Options specific to this terminal type (according to "
		"$TERM value).")),

	/* TODO: Incorporate some of the following to the option text.
	 *
	 * When UTF-8 I/O is disabled:
	 * 0 (TERM_DUMB) outputs ASCII -+| characters.
	 * 1 (TERM_VT100) switches charsets with ^N and ^O.
	 * 2 (TERM_LINUX) outputs CP437 characters without switching
	 *   charsets, so it works correctly only if the terminal uses
	 *   CP437.  Can also be made CP850 and CP852 compatible with
	 *   the restrict_852 option.
	 * 3 (TERM_KOI8) outputs KOI8-R characters without switching
	 *   charsets, so it works correctly only if the terminal uses
	 *   KOI8-R and the user has selected either KOI8-R or ASCII
	 *   in ELinks.  It is also mostly compatible with KOI8-U.
	 * 4 (TERM_FREEBSD) outputs characters in the 0x80...0x9F
	 *   range, which FreeBSD 4.0 (but not 5.0) treated as
	 *   graphical.
	 *
	 * When UTF-8 I/O is enabled, ELinks outputs (almost) the same
	 * characters as above but encodes them in UTF-8 and does not
	 * switch charsets.  So, it will work in any terminal that
	 * understands UTF-8 and has the characters in its font.  */
	INIT_OPT_INT("terminal._template_", N_("Type"),
		"type", 0, 0, 4, 0,
		N_("Terminal type; matters mostly only when drawing frames "
		"and dialog box borders:\n"
		"0 is dumb terminal type, ASCII art\n"
		"1 is VT100, simple but portable\n"
		"2 is Linux, you get double frames and other goodies\n"
		"3 is KOI-8\n"
		"4 is FreeBSD")),

	INIT_OPT_BOOL("terminal._template_", N_("Always encode xterm title in ISO-8859-1"),
		"latin1_title", 0, 1,
		N_("When updating the window title of xterm or a similar "
		"terminal emulator, encode the title in ISO-8859-1 (Latin-1), "
		"rather than in the charset used for other text in the "
		"window. Cyrillic and other characters get replaced with "
		"Latin ones. Xterm requires this unless you explicitly "
		"enable UTF-8 titles in it.\n"
		"\n"
		"If this option does not take effect immediately, try "
		"switching to a different page so that ELinks notices "
		"it needs to update the title.")),

	INIT_OPT_BOOL("terminal._template_", N_("Switch fonts for line drawing"),
		"m11_hack", 0, 0,
		N_("Switch fonts when drawing lines, enabling both local "
		"characters and lines working at the same time. "
		"ELinks uses this option only if UTF-8 I/O is disabled "
		"and the terminal type is Linux or FreeBSD.")),

	/* When CONFIG_UTF8 is defined, any code that reads the "utf_8_io"
	 * option should also check whether the "codepage" option is UTF-8,
	 * and if so, behave as if "utf_8_io" were 1.  (When CONFIG_UTF8 is
	 * not defined, it should not be possible to set UTF-8 as "codepage";
	 * please report any such possibilities as bugs.)  */
	INIT_OPT_BOOL("terminal._template_", N_("UTF-8 I/O"),
		"utf_8_io", 0, 0,
		N_("Enable I/O in UTF-8 for Unicode terminals. "
		"Note that currently, only the subset of UTF-8 according to "
		"terminal codepage is used. ELinks ignores this option "
		"if the terminal codepage is UTF-8.")),

	INIT_OPT_BOOL("terminal._template_", N_("Restrict frames in cp850/852"),
		"restrict_852", 0, 0,
		N_("Restrict the characters used when drawing lines. "
		"Makes sense only with linux terminals using the cp850/852 "
		"character sets.")),

	INIT_OPT_BOOL("terminal._template_", N_("Block cursor"),
		"block_cursor", 0, 0,
		N_("Move cursor to bottom right corner when done drawing. "
		"This is particularly useful when we have a block cursor, "
		"so that inversed text is displayed correctly.")),

	INIT_OPT_INT("terminal._template_", N_("Color mode"),
		"colors", 0, 0, COLOR_MODES - 1, 0,
		/* The list of modes must be at the end of this string
		 * because AsciiDoc 7.1.2 does not support continuing
		 * an outer list entry after an inner list.
		 * TRANSLATORS: This restriction applies only to the
		 * "en" (English) translation.  (See doc/Makefile.)  */
		N_("The color mode controls what colors are used and how they "
		"are output to the terminal.\n"
		"\n"
		"Some modes may have been disabled at compile time. "
		"The Setup -> Terminal options dialog lists the modes "
		"supported by this executable. If you select an "
		"unsupported mode, ELinks uses 16 colors.\n"
		"\n"
		"The color modes are:\n"
		"0 is mono mode, only 2 colors are used\n"
		"1 is 16 color mode, uses the common ANSI colors\n"
		"2 is 88 color mode, uses XTerm RGB codes\n"
		"3 is 256 color mode, uses XTerm RGB codes\n"
		"4 is true color mode, uses konsole RGB codes")),

	INIT_OPT_BOOL("terminal._template_", N_("Transparency"),
		"transparency", 0, 0,
		N_("If we should not set the background to black. "
		"This is particularly useful when we have a terminal "
		"(typically in some windowing environment) with a background "
		"image or a transparent background - it will be visible in "
		"ELinks as well (but ELinks document color handling will "
		"still assume the background is black so if you have a "
		"bright background you might experience contrast problems). "
		"Note that this option makes sense only when colors are "
		"enabled.")),

	INIT_OPT_BOOL("terminal._template_", N_("Underline"),
		"underline", 0, 0,
		N_("If we should use underline or enhance the color "
		"instead.")),

	INIT_OPT_CODEPAGE("terminal._template_", N_("Codepage"),
		"charset", 0, "System",
		N_("Codepage of charset used for displaying content on "
		"terminal. 'System' stands for a codepage determined by "
		"a selected locale.")),

	/* Keep options in alphabetical order. */



	INIT_OPT_TREE("", N_("User interface"),
		"ui", OPT_SORT,
		N_("User interface options.")),



	INIT_OPT_TREE("ui", N_("Color settings"),
		"colors", 0,
		N_("Default user interface color settings.")),


	/* ========================================================== */
	/* ============= BORING PART (colors) START ================= */
	/* ========================================================== */
	/* XXX: All bfu colors needs to have both 'text' and 'background'
	 *	options even if it is not used. get_bfu_color() depends
	 *	on it. */
	/* The colors and mono tree should be similar but with different default
	 * values of course so always use the macros below. */

#define DO_INIT_OPT_COLORS(subtree, capt, name, fg, bg, desc)		\
	INIT_OPT_TREE(subtree, capt, name, 0, desc),			\
	INIT_OPT_COLOR(subtree "." name, N_("Text color"),		\
		      "text", 0, fg, N_("Default text color.")),	\
	INIT_OPT_COLOR(subtree "." name, N_("Background color"), 	\
		      "background", 0, bg, N_("Default background color."))

#define INIT_OPT_COLORS(subtree, capt, name, cfg, cbg, mfg, mbg, desc)	\
	DO_INIT_OPT_COLORS("ui.colors.color" subtree, capt, name, cfg, cbg, desc), \
	DO_INIT_OPT_COLORS("ui.colors.mono" subtree, capt, name, mfg, mbg, desc)

#define INIT_OPT_COLOR_TREE(subtree, capt, name, desc)			\
	INIT_OPT_TREE("ui.colors.color" subtree, capt, name, 0, desc),	\
	INIT_OPT_TREE("ui.colors.mono" subtree, capt, name, 0, desc)

	INIT_OPT_TREE("ui.colors", N_("Color terminals"),
		"color", 0,
		N_("Color settings for color terminal.")),

	INIT_OPT_TREE("ui.colors", N_("Non-color terminals"),
		"mono", 0,
		N_("Color settings for non-color terminal.")),

	/* FIXME: obsolete, how to alias them correctly ? --Zas */
	INIT_OPT_COLOR_TREE("", N_("Main menu bar"),
		"mainmenu",
		N_("Main menu bar colors.")),

	INIT_OPT_COLORS(".mainmenu", N_("Unselected main menu bar item"),
		"normal", "black", "white", "black", "white",
		N_("Unselected main menu bar item colors.")),

	INIT_OPT_COLORS(".mainmenu", N_("Selected main menu bar item"),
		"selected", "green", "black", "gray", "black",
		N_("Selected main menu bar item colors.")),

	INIT_OPT_COLOR_TREE(".mainmenu", N_("Hotkey"),
		"hotkey",
		N_("Main menu hotkey colors.")),

	INIT_OPT_COLORS(".mainmenu.hotkey", N_("Unselected hotkey"),
		"normal", "darkred", "white", "black", "white",
		N_("Main menu unselected hotkey colors.")),

	INIT_OPT_COLORS(".mainmenu.hotkey", N_("Selected hotkey"),
		"selected", "darkred", "green", "black", "white",
		N_("Main menu selected hotkey colors.")),


	INIT_OPT_COLOR_TREE("", N_("Menu bar"),
		"menu",
		N_("Menu bar colors.")),

	INIT_OPT_COLORS(".menu", N_("Unselected menu item"),
		"normal", "black", "white", "black", "white",
		N_("Unselected menu item colors.")),

	INIT_OPT_COLORS(".menu", N_("Selected menu item"),
		"selected", "black", "green", "gray", "black",
		N_("Selected menu item colors.")),

	INIT_OPT_COLORS(".menu", N_("Marked menu item"),
		"marked", "red", "white", "gray", "white",
		N_("Marked menu item colors.")),

	INIT_OPT_COLOR_TREE(".menu", N_("Hotkey"),
		"hotkey",
		N_("Menu item hotkey colors.")),

	INIT_OPT_COLORS(".menu.hotkey", N_("Unselected hotkey"),
		"normal", "darkred", "white", "gray", "black",
		N_("Menu item unselected hotkey colors.")),

	INIT_OPT_COLORS(".menu.hotkey", N_("Selected hotkey"),
		"selected", "darkred", "green", "gray", "black",
		N_("Menu item selected hotkey colors.")),

	INIT_OPT_COLORS(".menu", N_("Menu frame"),
		"frame", "black", "white", "black", "white",
		N_("Menu frame colors.")),


	INIT_OPT_COLOR_TREE("", N_("Dialog"),
		"dialog",
		N_("Dialog colors.")),

	/* Compatibility alias: added by jonas at 2003-10-18, 0.5pre7.CVS. */
	INIT_OPT_ALIAS("ui.colors.color.dialog", "background", 0,
		"ui.colors.color.dialog.generic.background"),

	/* Compatibility alias: added by jonas at 2003-10-18, 0.5pre7.CVS. */
	INIT_OPT_ALIAS("ui.colors.mono.dialog", "background", 0,
		"ui.colors.mono.dialog.generic.background"),

	INIT_OPT_COLORS(".dialog", N_("Generic"),
		"generic", "black", "white", "black", "white",
		N_("Generic dialog colors.")),

	INIT_OPT_COLORS(".dialog", N_("Frame"),
		"frame", "black", "white", "black", "white",
		N_("Dialog frame colors.")),

	INIT_OPT_COLORS(".dialog", N_("Scrollbar"),
		"scrollbar", "black", "blue", "white", "black",
		N_("Scrollbar colors.")),

	INIT_OPT_COLORS(".dialog", N_("Selected scrollbar"),
		"scrollbar-selected", "black", "green", "black", "white",
		N_("Scrollbar selected colors.")),

	INIT_OPT_COLORS(".dialog", N_("Title"),
		"title", "darkred", "white", "gray", "black",
		N_("Dialog title colors.")),

	INIT_OPT_COLORS(".dialog", N_("Text"),
		"text", "black", "white", "black", "white",
		N_("Dialog text colors.")),

	INIT_OPT_COLORS(".dialog", N_("Checkbox"),
		"checkbox", "darkred", "white", "black", "white",
		N_("Dialog checkbox colors.")),

	INIT_OPT_COLORS(".dialog", N_("Selected checkbox"),
		"checkbox-selected", "yellow", "green", "white", "black",
		N_("Dialog selected checkbox colors.")),

	INIT_OPT_COLORS(".dialog", N_("Checkbox label"),
		"checkbox-label", "black", "white", "black", "white",
		N_("Dialog checkbox label colors.")),

	INIT_OPT_COLORS(".dialog", N_("Button"),
		"button", "white", "blue", "black", "white",
		N_("Dialog button colors.")),

	INIT_OPT_COLORS(".dialog", N_("Selected button"),
		"button-selected", "yellow", "green", "white", "black",
		N_("Dialog selected button colors.")),

	INIT_OPT_COLORS(".dialog", N_("Button shortcut"),
		"button-shortcut", "yellow", "blue", "white", "black",
		N_("Dialog button colors.")),

	INIT_OPT_COLORS(".dialog", N_("Selected button shortcut"),
		"button-shortcut-selected", "white", "blue", "black", "white",
		N_("Dialog selected button colors.")),

	INIT_OPT_COLORS(".dialog", N_("Text field"),
		"field", "white", "blue", "gray", "black",
		N_("Dialog text field colors.")),

	INIT_OPT_COLORS(".dialog", N_("Text field text"),
		"field-text", "yellow", "blue", "gray", "black",
		N_("Dialog field text colors.")),

	INIT_OPT_COLORS(".dialog", N_("Meter"),
		"meter", "white", "blue", "gray", "black",
		N_("Dialog meter colors.")),

	INIT_OPT_COLORS(".dialog", N_("Shadow"),
		"shadow", "black", "black", "black", "black",
		N_("Dialog shadow colors (see ui.shadows option).")),

	INIT_OPT_COLOR_TREE("", N_("Title bar"),
		"title",
		N_("Title bar colors.")),

	INIT_OPT_COLORS(".title", N_("Generic title bar"),
		"title-bar", "black", "white", "gray", "black",
		N_("Generic title bar colors.")),

	INIT_OPT_COLORS(".title", N_("Title bar text"),
		"title-text", "black", "white", "gray", "black",
		N_("Title bar text colors.")),


	INIT_OPT_COLOR_TREE("", N_("Status bar"),
		"status",
		N_("Status bar colors.")),

	INIT_OPT_COLORS(".status", N_("Generic status bar"),
		"status-bar", "black", "white", "black", "white",
		N_("Generic status bar colors.")),

	INIT_OPT_COLORS(".status", N_("Status bar text"),
		"status-text", "black", "white", "black", "white",
		N_("Status bar text colors.")),


	INIT_OPT_COLOR_TREE("", N_("Tabs bar"),
		"tabs",
		N_("Tabs bar colors.")),

	INIT_OPT_COLORS(".tabs", N_("Unvisited tab"),
		"unvisited", "darkblue", "white", "gray", "white",
		N_("Tab colors for tabs that have not been "
		   "selected since they completed loading.")),

	INIT_OPT_COLORS(".tabs", N_("Unselected tab"),
		"normal", "black", "white", "black", "white",
		N_("Unselected tab colors.")),

	INIT_OPT_COLORS(".tabs", N_("Loading tab"),
		"loading", "darkred", "white", "gray", "white",
		N_("Tab colors for tabs that are loading in the background.")),

	INIT_OPT_COLORS(".tabs", N_("Selected tab"),
		"selected", "black", "green", "gray", "black",
		N_("Selected tab colors.")),

	INIT_OPT_COLORS(".tabs", N_("Tab separator"),
		"separator", "brown", "white", "gray", "white",
		N_("Tab separator colors.")),


	INIT_OPT_COLORS("", N_("Searched strings"),
		"searched", "black", "lime", "black", "white",
		N_("Searched string highlight colors.")),


	/* ========================================================== */
	/* ============= BORING PART (colors) END =================== */
	/* ========================================================== */

	/* Keep options in alphabetical order. */

	INIT_OPT_TREE("ui", N_("Dialog settings"),
		"dialogs", 0,
		N_("Dialogs-specific appearance and behaviour settings.")),


	INIT_OPT_INT("ui.dialogs", N_("Minimal height of listbox widget"),
		"listbox_min_height", 0, 1, 20, 10,
		N_("Minimal height of the listbox widget (used e.g. for "
		"bookmarks or global history).")),

	INIT_OPT_BOOL("ui.dialogs", N_("Drop shadows"),
		"shadows", 0, 0,
		N_("Make dialogs drop shadows (the shadows are solid, you "
		"can adjust their color by ui.colors.*.dialog.shadow). "
		"You may also want to eliminate the wide borders by "
		"adjusting setup.h.")),

	INIT_OPT_BOOL("ui.dialogs", N_("Underline menu hotkeys"),
		"underline_hotkeys", 0, 0,
		N_("Whether to underline hotkeys in menus to make them more "
		"visible. Requires that underlining is enabled for the "
		"terminal.")),

	INIT_OPT_BOOL("ui.dialogs", N_("Underline button shortcuts"),
		"underline_button_shortcuts", 0, 0,
		N_("Whether to underline button shortcuts to make them more "
		"visible. Requires that underlining is enabled for the "
		"terminal.")),


	INIT_OPT_TREE("ui", N_("Timer options"),
		"timer", 0,
		N_("Timed action after certain interval of user inactivity. "
		"Someone can even find this useful, although you may not "
		"believe that.")),

#ifdef CONFIG_LEDS
	INIT_OPT_INT("ui.timer", N_("Enable"),
		"enable", 0, 0, 2, 0,
		N_("Whether to enable the timer or not:\n"
		"0 is don't count down anything\n"
		"1 is count down, but don't show the timer\n"
		"2 is count down and show the timer near LEDs")),
#else
	INIT_OPT_INT("ui.timer", N_("Enable"),
		"enable", 0, 0, 2, 0,
		N_("Whether to enable the timer or not:\n"
		"0 is don't count down anything\n"
		"1 is count down, but don't show the timer\n"
		"2 is count down and show the timer near LEDs (DISABLED)")),
#endif

	INIT_OPT_INT("ui.timer", N_("Duration"),
		"duration", 0, 1, 86400, 86400,
		N_("Inactivity timeout in seconds. The maximum of one day "
		"should be enough for just everyone (TM).")),

	INIT_OPT_STRING("ui.timer", N_("Action"),
		"action", 0, "",
		N_("Keybinding action to be triggered when timer reaches "
		"zero.")),


	INIT_OPT_TREE("ui", N_("Window tabs"),
		"tabs", 0,
		N_("Window tabs settings.")),

	INIT_OPT_INT("ui.tabs", N_("Display tabs bar"),
		"show_bar", 0, 0, 2, 1,
		N_("Show tabs bar on the screen:\n"
		"0 means never\n"
		"1 means only if two or more tabs are open\n"
		"2 means always")),

	INIT_OPT_BOOL("ui.tabs", N_("Tab bar at top"),
		"top", 0, 0,
		N_("Whether display tab bar at top like other browsers do.")),

	INIT_OPT_BOOL("ui.tabs", N_("Wrap-around tabs cycling"),
		"wraparound", 0, 1,
		N_("When moving right from the last tab, jump to the first "
		"one, and vice versa.")),

	INIT_OPT_BOOL("ui.tabs", N_("Confirm tab closing"),
		"confirm_close", 0, 0,
		N_("When closing a tab show confirmation dialog.")),



	INIT_OPT_LANGUAGE("ui", N_("Language"),
		"language", 0,
		N_("Language of user interface. 'System' means that the "
		"language will be extracted from the environment "
		"dynamically.")),

	INIT_OPT_BOOL("ui", N_("Display menu bar always"),
		"show_menu_bar_always", 0, 0,
		N_("Always show menu bar on the screen.")),

	INIT_OPT_BOOL("ui", N_("Display status bar"),
		"show_status_bar", 0, 1,
		N_("Show status bar on the screen.")),

	INIT_OPT_BOOL("ui", N_("Display title bar"),
		"show_title_bar", 0, 1,
		N_("Show title bar on the screen.")),

	INIT_OPT_BOOL("ui", N_("Display goto dialog in new tabs"),
		"startup_goto_dialog", 0, 1,
		N_("Pop up goto dialog in newly created tabs when there's "
		"no homepage set. This means also showing goto dialog on "
		"startup.")),

	INIT_OPT_BOOL("ui", N_("Show a message box when file is saved successfully"),
		"success_msgbox", 0, 1,
		N_("When you pressed a [ Save ] button in some manager, "
		"this option will make sure that a box confirming success "
		"of the operation will pop up.")),


	INIT_OPT_TREE("ui", N_("Sessions"),
		"sessions", OPT_SORT,
		N_("Sessions settings.")),

	INIT_OPT_BOOL("ui.sessions", N_("Keep session active"),
		"keep_session_active", 0, 0,
		N_("Keep the session active even if the last terminal "
		"exits.")),

	INIT_OPT_BOOL("ui.sessions", N_("Auto save session"),
		"auto_save", 0, 0,
		N_("Automatically save the session when quitting.\n"
		"\n"
		"This feature requires bookmark support.")),

	INIT_OPT_BOOL("ui.sessions", N_("Auto restore session"),
		"auto_restore", 0, 0,
		N_("Automatically restore the session at start.\n"
		"\n"
		"This feature requires bookmark support.")),

	INIT_OPT_STRING("ui.sessions", N_("Auto save and restore session folder name"),
		"auto_save_foldername", 0, "Auto saved session",
		N_("Name of the bookmarks folder used for auto saving and "
		"restoring session. The name has to be unique. Any folders "
		"with the same name will be deleted.\n"
		"\n"
		"This only makes sense with bookmark support.")),

	INIT_OPT_STRING("ui.sessions", N_("Homepage URI"),
		"homepage", 0, WWW_HOME_URL,
		N_("The URI to load either at startup time when no URI was "
		"given on the command line or when requested by the "
		"goto-url-home action. Set to \"\" if the environment "
		"variable WWW_HOME should be used as homepage URI instead.")),

#ifdef HAVE_STRFTIME
	INIT_OPT_STRING("ui", N_("Date format"),
		"date_format", 0, "%b %e %H:%M",
		N_("Date format to use in dialogs. See strftime(3).")),
#endif

	INIT_OPT_BOOL("ui", N_("Set window title"),
		"window_title", 0, 1,
		N_("Set the window title when running in a windowing "
		"environment in an xterm-like terminal. This way the "
		"document's title is shown on the window titlebar.")),



	/* Compatibility alias: added by pasky at 2004-07-22, 0.9.CVS. */
	INIT_OPT_ALIAS("", "secure_file_saving", 0, "infofiles.secure_save"),

	NULL_OPTION_INFO,
};

/* vim: set filetype=c : */
