#ifndef EL__BFU_STYLE_H
#define EL__BFU_STYLE_H

struct color_pair;
struct terminal;

/** Get suitable BFU color for the specific terminal
 *
 * Get a color pair (foreground- and background color) for a specific
 * BFU widget "style". Depending on the terminal settings a color
 * suitable for either mono terminals or color terminals is returned.
 * The returned color is derived by looking up the specified stylename
 * under the option tree of "ui.colors.color" or the "ui.colors.mono",
 * and using the values of the "text" and "background" color options as
 * the values of the color pair.
 *
 * @param term		Terminal for which the color will be used.
 * @param stylename	The name of the BFU color.
 * @return		A color pair matching the stylename or NULL.
 */
struct color_pair *
get_bfu_color(struct terminal *term, unsigned char *stylename);

/** Cleanup after the BFU style cache
 *
 * Free all resources used by the BFU style cache.
 */
void done_bfu_colors(void);

#endif
