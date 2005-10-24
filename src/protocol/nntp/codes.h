
#ifndef EL__PROTOCOL_NNTP_CODES_H
#define	EL__PROTOCOL_NNTP_CODES_H

/* RFC 977 - Section 2.4.2.  Status Responses
 *
 * These are status reports from the server and indicate the response to
 * the last command received from the client.
 *
 * Status response lines begin with a 3 digit numeric code which is
 * sufficient to distinguish all responses.  Some of these may herald
 * the subsequent transmission of text.
 *
 * The first digit of the response broadly indicates the success,
 * failure, or progress of the previous command.
 *
 *	1xx - Informative message
 *	2xx - Command ok
 *	3xx - Command ok so far, send the rest of it.
 *	4xx - Command was correct, but couldn't be performed for
 *	      some reason.
 *	5xx - Command unimplemented, or incorrect, or a serious
 *	           program error occurred.
 *
 *  The next digit in the code indicates the function response category.
 *
 *	x0x - Connection, setup, and miscellaneous messages
 *	x1x - Newsgroup selection
 *	x2x - Article selection
 *	x3x - Distribution functions
 *	x4x - Posting
 *	x8x - Nonstandard (private implementation) extensions
 *	x9x - Debugging output
 */

/* Check if the code is within the range of valid response codes */
#define check_nntp_code_valid(code)	(100 <= (code) && (code) <= 599)

/* Based loosely on codes from slrn. They are renamed and some was ignored. */
enum nntp_code {
	/* Internal: */
	NNTP_CODE_NONE			=   0, /* No code received. */
	NNTP_CODE_INVALID		=  -1, /* Bad code received. */

	/* Information: */
	NNTP_CODE_100_HELP_TEXT		= 100, /* Help text on way */
	NNTP_CODE_180_AUTH		= 180, /* Authorization capabilities */
	NNTP_CODE_199_DEBUG		= 199, /* Debug output */

	/* Success:*/
	NNTP_CODE_200_HELLO		= 200, /* Hello; you can post */
	NNTP_CODE_201_HELLO_NOPOST	= 201, /* Hello; you can't post */
	NNTP_CODE_202_SLAVE_STATUS	= 202, /* Slave status noted */
	NNTP_CODE_205_GOODBYE		= 205, /* Closing connection */
	NNTP_CODE_211_GROUP_SELECTED	= 211, /* Group selected */
	NNTP_CODE_215_FOLLOW_GROUPS	= 215, /* Newsgroups follow */
	NNTP_CODE_220_FOLLOW_ARTICLE	= 220, /* Article (head & body) follows */
	NNTP_CODE_221_FOLLOW_HEAD	= 221, /* Head follows */
	NNTP_CODE_222_FOLLOW_BODY	= 222, /* Body follows */
	NNTP_CODE_223_FOLLOW_NOTEXT	= 223, /* No text sent -- stat, next, last */
	NNTP_CODE_224_FOLLOW_XOVER	= 224, /* Overview data follows */
	NNTP_CODE_230_FOLLOW_NEWNEWS	= 230, /* New articles by message-id follow */
	NNTP_CODE_231_FOLLOW_NEWGROUPS	= 231, /* New newsgroups follow */
	NNTP_CODE_235_ARTICLE_RECEIVED	= 235, /* Article transferred successfully */
	NNTP_CODE_240_ARTICLE_POSTED	= 240, /* Article posted successfully */
	NNTP_CODE_281_AUTH_ACCEPTED	= 281, /* Authorization (user/pass) ok */

	/* Continuation: */
	NNTP_CODE_335_ARTICLE_TRANSFER	= 335, /* Send article to be transferred */
	NNTP_CODE_340_ARTICLE_POSTING	= 340, /* Send article to be posted */
	NNTP_CODE_380_AUTHINFO_USER	= 380, /* Authorization is required (send user) */
	NNTP_CODE_381_AUTHINFO_PASS	= 381, /* More authorization data required (send password) */

	/* Notice: */
	NNTP_CODE_400_GOODBYE		= 400, /* Have to hang up for some reason */
	NNTP_CODE_411_GROUP_UNKNOWN	= 411, /* No such newsgroup */
	NNTP_CODE_412_GROUP_UNSET	= 412, /* Not currently in newsgroup */
	NNTP_CODE_420_ARTICLE_UNSET	= 420, /* No current article selected */
	NNTP_CODE_421_ARTICLE_NONEXT	= 421, /* No next article in this group */
	NNTP_CODE_422_ARTICLE_NOPREV	= 422, /* No previous article in this group */
	NNTP_CODE_423_ARTICLE_NONUMBER	= 423, /* No such article (number) in this group */
	NNTP_CODE_430_ARTICLE_NOID	= 430, /* No such article (id) at all */
	NNTP_CODE_435_ARTICLE_NOSEND	= 435, /* Already got that article, don't send */
	NNTP_CODE_436_ARTICLE_TRANSFER	= 436, /* Transfer failed */
	NNTP_CODE_437_ARTICLE_REJECTED	= 437, /* Article rejected, don't resend */
	NNTP_CODE_440_POSTING_DENIED	= 440, /* Posting not allowed */
	NNTP_CODE_441_POSTING_FAILED	= 441, /* Posting failed */
	NNTP_CODE_480_AUTH_REQUIRED	= 480, /* Authorization required for command */
	NNTP_CODE_482_AUTH_REJECTED	= 482, /* Authorization data rejected */

	/* Error: */
	NNTP_CODE_500_COMMAND_UNKNOWN	= 500, /* Command not recognized */
	NNTP_CODE_501_COMMAND_SYNTAX	= 501, /* Command syntax error */
	NNTP_CODE_502_ACCESS_DENIED	= 502, /* Access to server denied */
	NNTP_CODE_503_PROGRAM_FAULT	= 503, /* Program fault, command not performed */
	NNTP_CODE_580_AUTH_FAILED	= 580, /* Authorization Failed */
};

#endif
