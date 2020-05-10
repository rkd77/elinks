/* Cookie path matching */

#include <string.h>

int
is_path_prefix(unsigned char *cookiepath, unsigned char *requestpath)
{
	int dl = strlen(cookiepath);
	int sl = strlen(requestpath);

	if (dl > sl) return 0;

	if (memcmp(cookiepath, requestpath, dl)) return 0;

	if (dl == sl) return 1;

	if (cookiepath[dl - 1] == '/') return 1;

	return (requestpath[dl] == '/');
}
