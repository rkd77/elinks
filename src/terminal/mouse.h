#ifndef EL__TERMINAL_MOUSE_H
#define EL__TERMINAL_MOUSE_H

struct interlink_event;
struct itrm;

/* The mouse reporting button byte looks like:
 *
 * -ss??bbb
 *  ||  |||
 *  ||  +++---> buttons:
 *  ||          0 left
 *  ||          1 middle
 *  ||          2 right
 *  ||          3 release OR wheel up [rxvt]
 *  ||          4 wheel down [rxvt]
 *  ||
 *  ++--------> style:
 *              1 normalclick (makes sure the whole thing is >= ' ')
 *              2 doubleclick
 *              3 mouse wheel move [xterm]
 *                then in bb is 0 for up and 1 for down
 *
 * What we translate it to:
 * -da??bbb
 *  ||  |||
 *  ||  +++---> buttons:
 *  ||          0 left
 *  ||          1 middle
 *  ||          2 right
 *  ||          3 wheel up
 *  ||          4 wheel down
 *  ||
 *  |+--------> action:
 *  |           0 press (B_DOWN)
 *  |           1 release (B_UP)
 *  |
 *  +---------> drag flag (valid only for B_UP)
 *
 * (TODO: doubleclick facility? --pasky)
 *
 * Let me introduce to the wonderful world of X terminals and their handling of
 * mouse wheel now. First, one sad fact: reasonable mouse wheel reporting
 * support has only xterm itself (from everything I tried) - it is relatively
 * elegant, it shouldn't confuse existing applications too much and it is
 * generally non-conflicting with the old behaviour.
 *
 * All other terminals are doing absolutely terrible things, making it hell on
 * the hearth to support mouse wheels. rxvt will send the buttons as succeeding
 * button numbers (3, 4) in normal sequences, but that has a little problem -
 * button 3 is also alias for button release; welcome to the wonderful world of
 * X11.  But at least, rxvt will send only "press" sequence, but no release
 * sequence (it really doesn't make sense for wheels anyway, does it?). That
 * has the advantage that you can do some heuristic (see below) to at least
 * partially pace with this terrible thing.
 *
 * But now, let's see another nice pair of wonderful terminals: aterm and
 * Eterm. They emit same braindead sequence for wheel up/down as rxvt, but
 * that's not all. Yes, you guessed it - they send even the release sequence
 * (immediatelly after press sequence) for the wheels. So, you will see
 * something like:
 *
 * old glasses - <button1 press> <release> <release> <release>
 * new glasses - <button1 press> <wheelup press> <wheelup press> <wheelup press>
 * smartglasses1-<button1 press> <release> <wheelup press> <wheelup press>
 * smartglasses2-<button1 press> <release> <wheelup press> <release>
 *
 * But with smartglasses2, you will have a problem with rxvt when someone will
 * move the wheel multiple times - only half of the times it will be recorded.
 * Poof. No luck :-(.
 *
 * [smartglasses1]:
 * When user presses some button and then moves the wheel, action for button
 * release will be done and when he will release the button, action for the
 * wheel will be done. That's unfortunately inevitable in order to work under
 * rxvt :-(.
 */

struct term_event_mouse {
	int x, y;
	unsigned int button;
};
#define interlink_event_mouse term_event_mouse

#define BM_BUTT		7
#define B_LEFT		0
#define B_MIDDLE	1
#define B_RIGHT		2
#define B_WHEEL_UP	3
#define B_WHEEL_DOWN	4

#define BM_ACT		32
#define B_DOWN		0
#define B_UP		32

#define BM_DRAG		64
#define B_DRAG		64

#define mouse_get_action(mouse_)		((mouse_)->button & BM_ACT)
#define mouse_action_is(mouse_, value)	(mouse_get_action(mouse_) == (value))

#define mouse_get_button(mouse_)		((mouse_)->button & BM_BUTT)
#define mouse_button_is(mouse_, value)	(mouse_get_button(mouse_) == (value))
#define mouse_wheeling(mouse_)		(mouse_get_button(mouse_) >= B_WHEEL_UP)

#define mouse_is_in_box(mouse_, box) \
	is_in_box(box, (mouse_)->x, (mouse_)->y)

#define set_mouse(mouse_, x_, y_, button_) do { \
	(mouse_)->x = (x_); \
	(mouse_)->y = (y_); \
	(mouse_)->button = (button_); \
} while (0)

void send_mouse_init_sequence(int h);
void send_mouse_done_sequence(int h);
void disable_mouse(void);
void enable_mouse(void);
void toggle_mouse(void);
int decode_terminal_mouse_escape_sequence(struct itrm *itrm, struct interlink_event *ev, int el, int v);


#endif
