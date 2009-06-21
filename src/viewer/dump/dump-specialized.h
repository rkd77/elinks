/* Fully specialized functions for dumping to a file.
 *
 * This include file defines a function that dumps the document to a
 * file.  The function is specialized to one color mode and one kind
 * of charset.  This is supposedly faster than runtime checks.  The
 * file that includes this file must define several macros to select
 * the specialization.
 *
 * The following macro must be defined:
 *
 * - DUMP_FUNCTION_SPECIALIZED: The name of the function that this
 *   file should define.
 *
 * One of the following macros must be defined:
 *
 * - DUMP_COLOR_MODE_NONE
 * - DUMP_COLOR_MODE_16
 * - DUMP_COLOR_MODE_256
 * - DUMP_COLOR_MODE_TRUE
 *
 * The following macro may be defined:
 *
 * - DUMP_CHARSET_UTF8
 */

static int
DUMP_FUNCTION_SPECIALIZED(struct document *document, struct dump_output *out)
{
	int y;
#ifdef DUMP_COLOR_MODE_16
	unsigned char color = 0;
	const int width = get_opt_int("document.dump.width");
#elif defined(DUMP_COLOR_MODE_256)
	unsigned char foreground = 0;
	unsigned char background = 0;
	const int width = get_opt_int("document.dump.width");
#elif defined(DUMP_COLOR_MODE_TRUE)
	static const unsigned char color[6] = {255, 255, 255, 0, 0, 0};
	const unsigned char *foreground = &color[0];
	const unsigned char *background = &color[3];
	const int width = get_opt_int("document.dump.width");
#endif	/* DUMP_COLOR_MODE_TRUE */

	for (y = 0; y < document->height; y++) {
#ifdef DUMP_COLOR_MODE_NONE
		int white = 0;
#endif
		int x;

#ifdef DUMP_COLOR_MODE_16
		write_color_16(color, out);
#elif defined(DUMP_COLOR_MODE_256)
		write_color_256("38", foreground, out);
		write_color_256("48", background, out);
#elif defined(DUMP_COLOR_MODE_TRUE)
		write_true_color("38", foreground, out);
		write_true_color("48", background, out);
#endif	/* DUMP_COLOR_MODE_TRUE */

		for (x = 0; x < document->data[y].length; x++) {
#ifdef DUMP_CHARSET_UTF8
			unicode_val_T c;
			const unsigned char *utf8_buf;
#else  /* !DUMP_CHARSET_UTF8 */
			unsigned char c;
#endif  /* !DUMP_CHARSET_UTF8 */
			const unsigned char attr
				= document->data[y].chars[x].attr;
#ifdef DUMP_COLOR_MODE_16
			const unsigned char color1
				= document->data[y].chars[x].color[0];
#elif defined(DUMP_COLOR_MODE_256)
			const unsigned char color1
				= document->data[y].chars[x].color[0];
			const unsigned char color2
				= document->data[y].chars[x].color[1];
#elif defined(DUMP_COLOR_MODE_TRUE)
			const unsigned char *const new_foreground
				= &document->data[y].chars[x].color[0];
			const unsigned char *const new_background
				= &document->data[y].chars[x].color[3];
#endif	/* DUMP_COLOR_MODE_TRUE */

			c = document->data[y].chars[x].data;

#ifdef DUMP_CHARSET_UTF8
			if (c == UCS_NO_CHAR) {
				/* This is the second cell of
				 * a double-cell character.  */
				continue;
			}
#endif	/* DUMP_CHARSET_UTF8 */

#ifdef DUMP_COLOR_MODE_16
			if (color != color1) {
				color = color1;
				if (write_color_16(color, out))
					return -1;
			}

#elif defined(DUMP_COLOR_MODE_256)
			if (foreground != color1) {
				foreground = color1;
				if (write_color_256("38", foreground, out))
					return -1;
			}

			if (background != color2) {
				background = color2;
				if (write_color_256("48", background, out))
					return -1;
			}

#elif defined(DUMP_COLOR_MODE_TRUE)
			if (memcmp(foreground, new_foreground, 3)) {
				foreground = new_foreground;
				if (write_true_color("38", foreground, out))
					return -1;
			}

			if (memcmp(background, new_background, 3)) {
				background = new_background;
				if (write_true_color("48", background, out))
					return -1;
			}
#endif	/* DUMP_COLOR_MODE_TRUE */

			if ((attr & SCREEN_ATTR_FRAME)
			    && c >= 176 && c < 224)
				c = frame_dumb[c - 176];

#ifdef DUMP_CHARSET_UTF8
			if (!isscreensafe_ucs(c)) c = ' ';
#else
			if (!isscreensafe(c)) c = ' ';
#endif

#ifdef DUMP_COLOR_MODE_NONE
			if (c == ' ') {
				/* Count spaces. */
				white++;
				continue;
			}

			/* Print spaces if any. */
			while (white) {
				if (write_char(' ', out))
					return -1;
				white--;
			}
#endif	/* DUMP_COLOR_MODE_NONE */

			/* Print normal char. */
#ifdef DUMP_CHARSET_UTF8
			utf8_buf = encode_utf8(c);
			while (*utf8_buf) {
				if (write_char(*utf8_buf++, out)) return -1;
			}

#else  /* !DUMP_CHARSET_UTF8 */
			if (write_char(c, out))
				return -1;
#endif	/* !DUMP_CHARSET_UTF8 */
		}

#ifndef DUMP_COLOR_MODE_NONE
		for (;x < width; x++) {
			if (write_char(' ', out))
				return -1;
		}
#endif	/* !DUMP_COLOR_MODE_NONE */

		/* Print end of line. */
		if (write_char('\n', out))
			return -1;
	}

	if (dump_output_flush(out))
		return -1;

	return 0;
}
