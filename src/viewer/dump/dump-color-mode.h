/* Color modes for dumping to a file.
 *
 * dump.c includes this file multiple times, once for each color mode.
 * Each time, it defines at most one of the following macros:
 *
 * - DUMP_COLOR_MODE_16
 * - DUMP_COLOR_MODE_256
 * - DUMP_COLOR_MODE_TRUE
 *
 * This file then defines a dumping function specifically for that
 * color mode.  This is supposedly faster than runtime checks.  */

#ifdef DUMP_COLOR_MODE_16
static int
dump_to_file_16(struct document *document, int fd)
#elif defined(DUMP_COLOR_MODE_256)
static int
dump_to_file_256(struct document *document, int fd)
#elif defined(DUMP_COLOR_MODE_TRUE)
static int
dump_to_file_true_color(struct document *document, int fd)
#else
int
dump_to_file(struct document *document, int fd)
#endif
{
	int y;
	int bptr = 0;
	unsigned char *buf = mem_alloc(D_BUF);
#ifdef DUMP_COLOR_MODE_16
	unsigned char color = 0;
	int width = get_opt_int("document.dump.width");
#elif defined(DUMP_COLOR_MODE_256)
	unsigned char foreground = 0;
	unsigned char background = 0;
	int width = get_opt_int("document.dump.width");
#elif defined(DUMP_COLOR_MODE_TRUE)
	static unsigned char color[6] = {255, 255, 255, 0, 0, 0};
	unsigned char *foreground = &color[0];
	unsigned char *background = &color[3];
	int width = get_opt_int("document.dump.width");
#endif	/* DUMP_COLOR_MODE_TRUE */

	if (!buf) return -1;

#ifdef CONFIG_UTF8
	if (is_cp_utf8(document->options.cp))
		goto utf8;
#endif /* CONFIG_UTF8 */

	for (y = 0; y < document->height; y++) {
		int white = 0;
		int x;

#ifdef DUMP_COLOR_MODE_16
		write_color_16(color, fd, buf, &bptr);
#elif defined(DUMP_COLOR_MODE_256)
		write_color_256("38", foreground, fd, buf, &bptr);
		write_color_256("48", background, fd, buf, &bptr);
#elif defined(DUMP_COLOR_MODE_TRUE)
		write_true_color("38", foreground, fd, buf, &bptr);
		write_true_color("48", background, fd, buf, &bptr);
#endif	/* DUMP_COLOR_MODE_TRUE */

		for (x = 0; x < document->data[y].length; x++) {
			unsigned char c;
			unsigned char attr = document->data[y].chars[x].attr;
#ifdef DUMP_COLOR_MODE_16
			unsigned char color1 = document->data[y].chars[x].color[0];

			if (color != color1) {
				color = color1;
				if (write_color_16(color, fd, buf, &bptr))
					goto fail;
			}
#elif defined(DUMP_COLOR_MODE_256)
			unsigned char color1 = document->data[y].chars[x].color[0];
			unsigned char color2 = document->data[y].chars[x].color[1];

			if (foreground != color1) {
				foreground = color1;
				if (write_color_256("38", foreground, fd, buf, &bptr))
					goto fail;
			}

			if (background != color2) {
				background = color2;
				if (write_color_256("48", background, fd, buf, &bptr))
					goto fail;
			}
#elif defined(DUMP_COLOR_MODE_TRUE)
			unsigned char *new_foreground = &document->data[y].chars[x].color[0];
			unsigned char *new_background = &document->data[y].chars[x].color[3];

			if (memcmp(foreground, new_foreground, 3)) {
				foreground = new_foreground;
				if (write_true_color("38", foreground, fd, buf, &bptr))
					goto fail;
			}

			if (memcmp(background, new_background, 3)) {
				background = new_background;
				if (write_true_color("48", background, fd, buf, &bptr))
					goto fail;
			}
#endif	/* DUMP_COLOR_MODE_TRUE */

			c = document->data[y].chars[x].data;

			if ((attr & SCREEN_ATTR_FRAME)
			    && c >= 176 && c < 224)
				c = frame_dumb[c - 176];

			if (c <= ' ') {
				/* Count spaces. */
				white++;
				continue;
			}

			/* Print spaces if any. */
			while (white) {
				if (write_char(' ', fd, buf, &bptr))
					goto fail;
				white--;
			}

			/* Print normal char. */
			if (write_char(c, fd, buf, &bptr))
				goto fail;
		}
#if defined(DUMP_COLOR_MODE_16) || defined(DUMP_COLOR_MODE_256) || defined(DUMP_COLOR_MODE_TRUE)
		for (;x < width; x++) {
			if (write_char(' ', fd, buf, &bptr))
				goto fail;
		}
#endif	/* DUMP_COLOR_MODE_16 || DUMP_COLOR_MODE_256 || DUMP_COLOR_MODE_TRUE */

		/* Print end of line. */
		if (write_char('\n', fd, buf, &bptr))
			goto fail;
	}
#ifdef CONFIG_UTF8
	goto ref;
utf8:
	for (y = 0; y < document->height; y++) {
		int white = 0;
		int x;

#ifdef DUMP_COLOR_MODE_16
		write_color_16(color, fd, buf, &bptr);
#elif defined(DUMP_COLOR_MODE_256)
		write_color_256("38", foreground, fd, buf, &bptr);
		write_color_256("48", background, fd, buf, &bptr);
#elif defined(DUMP_COLOR_MODE_TRUE)
		write_true_color("38", foreground, fd, buf, &bptr);
		write_true_color("48", background, fd, buf, &bptr);
#endif	/* DUMP_COLOR_MODE_TRUE */

		for (x = 0; x < document->data[y].length; x++) {
			unicode_val_T c;
			unsigned char attr = document->data[y].chars[x].attr;
#ifdef DUMP_COLOR_MODE_16
			unsigned char color1 = document->data[y].chars[x].color[0];

			if (color != color1) {
				color = color1;
				if (write_color_16(color, fd, buf, &bptr))
					goto fail;
			}
#elif defined(DUMP_COLOR_MODE_256)
			unsigned char color1 = document->data[y].chars[x].color[0];
			unsigned char color2 = document->data[y].chars[x].color[1];

			if (foreground != color1) {
				foreground = color1;
				if (write_color_256("38", foreground, fd, buf, &bptr))
					goto fail;
			}

			if (background != color2) {
				background = color2;
				if (write_color_256("48", background, fd, buf, &bptr))
					goto fail;
			}
#elif defined(DUMP_COLOR_MODE_TRUE)
			unsigned char *new_foreground = &document->data[y].chars[x].color[0];
			unsigned char *new_background = &document->data[y].chars[x].color[3];

			if (memcmp(foreground, new_foreground, 3)) {
				foreground = new_foreground;
				if (write_true_color("38", foreground, fd, buf, &bptr))
					goto fail;
			}

			if (memcmp(background, new_background, 3)) {
				background = new_background;
				if (write_true_color("48", background, fd, buf, &bptr))
					goto fail;
			}
#endif	/* DUMP_COLOR_MODE_TRUE */

			c = document->data[y].chars[x].data;

			if ((attr & SCREEN_ATTR_FRAME)
			    && c >= 176 && c < 224)
				c = frame_dumb[c - 176];
			else {
				unsigned char *utf8_buf = encode_utf8(c);

				while (*utf8_buf) {
					if (write_char(*utf8_buf++,
						fd, buf, &bptr)) goto fail;
				}

				x += unicode_to_cell(c) - 1;

				continue;
			}

			if (c <= ' ') {
				/* Count spaces. */
				white++;
				continue;
			}

			/* Print spaces if any. */
			while (white) {
				if (write_char(' ', fd, buf, &bptr))
					goto fail;
				white--;
			}

			/* Print normal char. */
			if (write_char(c, fd, buf, &bptr))
				goto fail;
		}
#if defined(DUMP_COLOR_MODE_16) || defined(DUMP_COLOR_MODE_256) || defined(DUMP_COLOR_MODE_TRUE)
		for (;x < width; x++) {
			if (write_char(' ', fd, buf, &bptr))
				goto fail;
		}
#endif	/* DUMP_COLOR_MODE_16 || DUMP_COLOR_MODE_256 || DUMP_COLOR_MODE_TRUE */

		/* Print end of line. */
		if (write_char('\n', fd, buf, &bptr))
			goto fail;
	}
ref:
#endif /* CONFIG_UTF8 */

	if (hard_write(fd, buf, bptr) != bptr) {
fail:
		mem_free(buf);
		return -1;
	}

	if (dump_references(document, fd, buf))
		goto fail;

	mem_free(buf);
	return 0;
}
