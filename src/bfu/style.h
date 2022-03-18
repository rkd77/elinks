#ifndef EL__BFU_STYLE_H
#define EL__BFU_STYLE_H

#ifdef __cplusplus
extern "C" {
#endif

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
get_bfu_color(struct terminal *term, const char *stylename);

unsigned int get_bfu_color_node(struct terminal *term, const char *stylename);

unsigned char get_bfu_background_color256_node(unsigned int node_number);
unsigned char get_bfu_foreground_color256_node(unsigned int node_number);
unsigned char *get_bfu_background_color_true_node(unsigned int node_number);
unsigned char *get_bfu_foreground_color_true_node(unsigned int node_number);

/** Cleanup after the BFU style cache
 *
 * Free all resources used by the BFU style cache.
 */
void done_bfu_colors(void);

#ifdef __cplusplus
}
#endif

#endif
