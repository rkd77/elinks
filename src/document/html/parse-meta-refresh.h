#ifndef EL__DOCUMENT_HTML_PARSE_META_REFRESH_H
#define EL__DOCUMENT_HTML_PARSE_META_REFRESH_H

/** Parses a \<meta http-equiv="refresh" content="..."> element.
 *
 * @param[in] content
 *   The value of the content attribute, with entities already expanded.
 * @param[out] delay
 *   How many seconds to wait before refreshing.
 * @param[out] url
 *   The URI to load when refreshing, or NULL to reload the same document.
 *   The caller must free the string with mem_free() unless it's NULL.
 *
 * @return
 *   0 if successful, or negative on error.
 *   On error, *@a url is NULL.  */
int html_parse_meta_refresh(const unsigned char *content,
			    unsigned long *delay,
			    unsigned char **url);

#endif
