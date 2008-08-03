#ifndef EL__SESSION_SESSION_H
#define EL__SESSION_SESSION_H

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "main/timer.h" /* timer_id_T */
#include "network/state.h"
#include "session/download.h"
#include "session/history.h"
#include "util/lists.h"
#include "viewer/text/vs.h"

struct document_view;
struct link;
struct location;
struct session_status;
struct term_event;
struct terminal_info;
struct terminal;
struct uri;
struct window;

/** Used by delayed_open() and delayed_goto_uri_frame(). */
struct delayed_open {
	struct session *ses;
	struct uri *uri;
	unsigned char *target;
};

enum remote_session_flags {
	SES_REMOTE_NEW_TAB = 1,
	SES_REMOTE_NEW_WINDOW = 2,
	SES_REMOTE_CURRENT_TAB = 4,
	SES_REMOTE_PROMPT_URL = 8,
	SES_REMOTE_PING = 16,
	SES_REMOTE_ADD_BOOKMARK = 32,
	SES_REMOTE_INFO_BOX = 64,
};

/** This is generic frame descriptor, meaningful mainly for ses_*_frame*(). */
struct frame {
	LIST_HEAD(struct frame);

	unsigned char *name;
	int redirect_cnt;

	struct view_state vs;
};

/** Use for keyboard prefixes. */
struct kbdprefix {
	/** This is the repeat count being inserted by user so far.
	 * It is stored intermediately per-session. */
	int repeat_count;

#ifdef CONFIG_MARKS
	/** If the previous key was a mark prefix, this describes what kind
	 * of action are we supposed to do when we receive the next key. */
	enum { KP_MARK_NOTHING, KP_MARK_SET, KP_MARK_GOTO } mark;
#endif
};

struct session;

/** This describes, what are we trying to do right now. We pass this around so
 * that we can use generic scheduler routines and when the control will get
 * back to our subsystem, we will know what are we up to. */
enum task_type {
	TASK_NONE,
	TASK_FORWARD,
	TASK_IMGMAP,
	TASK_RELOAD,
	TASK_HISTORY,
};

struct session_task {
	enum task_type type;
	/* TODO: union --pasky */
	struct {
		unsigned char *frame;
		struct location *location;
	} target;
};

struct session_status {
	unsigned int show_tabs_bar:1;
	unsigned int show_status_bar:1;
	unsigned int show_title_bar:1;

	int force_show_status_bar:2;
	int force_show_title_bar:2;

	unsigned int set_window_title:1;
	unsigned char *last_title;
#ifdef CONFIG_ECMASCRIPT
	unsigned char *window_status;
#endif

#ifdef CONFIG_LEDS
	unsigned int show_leds:1;
	struct led_panel leds;
	struct led *ssl_led;
	struct led *insert_mode_led;
	struct led *ecmascript_led;
	struct led *popup_led;
#endif
	/** Has the tab been visited yet. */
	unsigned int visited:1;

	/** Is processing file requests. */
	unsigned int processing_file_requests:1;
	unsigned int show_tabs_bar_at_top:1;
};

enum insert_mode {
	INSERT_MODE_LESS,
	INSERT_MODE_ON,
	INSERT_MODE_OFF,
};

enum navigate_mode {
	NAVIGATE_LINKWISE,
	NAVIGATE_CURSOR_ROUTING,
};

/** This is one of the building stones of ELinks architecture --- this structure
 * carries information about the specific ELinks session. Each tab (thus, at
 * least one per terminal, in the normal case) has its own session. Session
 * describes mainly the current browsing and control state, from the currently
 * viewed document through the browsing history of this session to the status
 * bar information. */
struct session {
	LIST_HEAD(struct session);


	/** @name The vital session data
	 * @{ */

	struct window *tab;


	/** @} @name Browsing history
	 * @{ */

	struct ses_history history;


	/** @} @name The current document
	 * @{ */

	LIST_OF(struct file_to_load) more_files;

	struct download loading;
	struct uri *loading_uri;

	enum cache_mode reloadlevel;
	int redirect_cnt;

	struct document_view *doc_view;
	LIST_OF(struct document_view) scrn_frames;

	struct uri *download_uri;

	/** The URI which is the referrer to the current loaded document
	 * or NULL if there are no referrer.
	 *
	 * The @c referrer member's sole purpose is to have the information
	 * handy when loading URIs. It is not 'filtered' in anyway at this
	 * level only at the lower ones. */
	struct uri *referrer;


	/** @} @name The current action-in-progress selector
	 * @{ */

	struct session_task task;


	/** @} @name The current browsing state
	 * @{ */

	int search_direction;
	struct kbdprefix kbdprefix;
	int exit_query;
	timer_id_T display_timer;

	/** The text input form insert mode. It is a tristate controlled by the
	 * boolean document.browse.forms.insert_mode option. When disabled we
	 * use modeless insertion and we always insert stuff into the text
	 * input field. When enabled it is possible to switch insertion on and
	 * off using ::ACT_EDIT_ENTER and *_CANCEL. */
	enum insert_mode insert_mode;

	enum navigate_mode navigate_mode;

	unsigned char *search_word;
	unsigned char *last_search_word;


	/** The possibly running type queries (what-to-do-with-that-file?) */
	LIST_OF(struct type_query) type_queries;

	/** The info for status displaying */
	struct session_status status;

	/** @} */
};

extern LIST_OF(struct session) sessions;
extern enum remote_session_flags remote_session_flags;

/** This returns a pointer to the current location inside of the given session.
 * That's nice for encapsulation and already paid out once ;-). */
#define cur_loc(x) ((x)->history.current)

/** Return if we have anything being loaded in this session already.
 * @relates session */
static inline int
have_location(struct session *ses) {
	return !!cur_loc(ses);
}

/** Swaps the current session referrer with the new one passed as @a referrer.
 * @a referrer may be NULL.
 * @relates session */
void set_session_referrer(struct session *ses, struct uri *referrer);

void
print_error_dialog(struct session *ses, struct connection_state state,
		   struct uri *uri, enum connection_priority priority);

void process_file_requests(struct session *);

struct string *encode_session_info(struct string *info,
				   LIST_OF(struct string_list_item) *url_list);

/** @returns zero if the info was remote sessions or if it failed to
 * create any sessions. */
int decode_session_info(struct terminal *term, struct terminal_info *info);

/** Registers a base session and returns its id. Value <= 0 means error. */
int
add_session_info(struct session *ses, struct uri *uri, struct uri *referrer,
		 enum cache_mode cache_mode, enum task_type task);

void done_saved_session_info(void);

struct session *init_session(struct session *ses, struct terminal *term,
	     struct uri *uri, int in_background);

void doc_loading_callback(struct download *, struct session *);

void abort_loading(struct session *, int);
void reload(struct session *, enum cache_mode);
void load_frames(struct session *, struct document_view *);

struct frame *ses_find_frame(struct session *, unsigned char *);

void free_files(struct session *);
void display_timer(struct session *ses);

/** session_is_loading() is like !!get_current_download() but doesn't take
 * session.req_sent into account.
 * @relates session */
int session_is_loading(struct session *ses);
struct download *get_current_download(struct session *ses);

/** Information about the current document */
unsigned char *get_current_url(struct session *, unsigned char *, size_t);
unsigned char *get_current_title(struct session *, unsigned char *, size_t);

struct link *get_current_session_link(struct session *ses);
struct link *get_current_link_in_view(struct document_view *doc_view);
unsigned char *get_current_link_url(struct session *, unsigned char *, size_t);
unsigned char *get_current_link_name(struct session *, unsigned char *, size_t);

extern LIST_OF(struct questions_entry) questions_queue;
void add_questions_entry(void (*callback)(struct session *, void *), void *data);
void check_questions_queue(struct session *ses);

unsigned char *get_homepage_url(void);

/** Returns current keyboard repeat count and reset it. */
int eat_kbd_repeat_count(struct session *ses);

#endif
