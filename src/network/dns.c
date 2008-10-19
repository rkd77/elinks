/* Domain Name System Resolver Department */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_NETDB_H
#include <netdb.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* Go and say 'thanks' to BSD. */
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "main/select.h"
#include "network/dns.h"
#include "osdep/osdep.h"
#include "protocol/uri.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/time.h"


struct dnsentry {
	LIST_HEAD(struct dnsentry);

	struct sockaddr_storage *addr;	/* Pointer to array of addresses. */
	int addrno;			/* Adress array length. */
	timeval_T creation_time;	/* Creation time; let us do timeouts. */
	unsigned char name[1];		/* Associated host; XXX: Must be last. */
};

struct dnsquery {
#ifdef THREAD_SAFE_LOOKUP
	struct dnsquery *next_in_queue;	/* Got queued? */
#endif
	dns_callback_T done;		/* Used for reporting back DNS result. */
	void *data;			/* Private callback data. */

	/* The @done callback is called with these members. Thus, when
	 * free()ing, *always* set pointer to NULL ! */
	struct sockaddr_storage *addr;	/* Reference to array of addresses. */
	int addrno;			/* Reference to array len. */

	/* As with the two members above, when stopping a DNS query *always* set
	 * this pointer to NULL. */
	struct dnsquery **queryref;	/* Reference to callers DNS member. */

#ifndef NO_ASYNC_LOOKUP
	int h;				/* One end of the async thread pipe. */
#endif
	unsigned char name[1];		/* Associated host; XXX: Must be last. */
};


#ifdef THREAD_SAFE_LOOKUP
static struct dnsquery *dns_queue = NULL;
#endif

static INIT_LIST_OF(struct dnsentry, dns_cache);

static void done_dns_lookup(struct dnsquery *query, enum dns_result res);


/* DNS cache management: */

static struct dnsentry *
find_in_dns_cache(unsigned char *name)
{
	struct dnsentry *dnsentry;

	foreach (dnsentry, dns_cache)
		if (!c_strcasecmp(dnsentry->name, name)) {
			move_to_top_of_list(dns_cache, dnsentry);
			return dnsentry;
		}

	return NULL;
}

static void
add_to_dns_cache(unsigned char *name, struct sockaddr_storage *addr, int addrno)
{
	int namelen = strlen(name);
	struct dnsentry *dnsentry;
	int size;

	assert(addrno > 0);

	dnsentry = mem_calloc(1, sizeof(*dnsentry) + namelen);
	if (!dnsentry) return;

	size = addrno * sizeof(*dnsentry->addr);
	dnsentry->addr = mem_alloc(size);
	if (!dnsentry->addr) {
		mem_free(dnsentry);
		return;
	}

	/* calloc() sets NUL char for us. */
	memcpy(dnsentry->name, name, namelen);
	memcpy(dnsentry->addr, addr, size);;

	dnsentry->addrno = addrno;

	timeval_now(&dnsentry->creation_time);
	add_to_list(dns_cache, dnsentry);
}

static void
del_dns_cache_entry(struct dnsentry *dnsentry)
{
	del_from_list(dnsentry);
	mem_free_if(dnsentry->addr);
	mem_free(dnsentry);
}


/* Synchronous DNS lookup management: */

enum dns_result
do_real_lookup(unsigned char *name, struct sockaddr_storage **addrs, int *addrno,
	       int in_thread)
{
#ifdef CONFIG_IPV6
	struct addrinfo hint, *ai, *ai_cur;
#else
	struct hostent *hostent = NULL;
#endif
	int i;

	if (!name || !addrs || !addrno)
		return DNS_ERROR;

#ifdef CONFIG_IPV6
	/* I had a strong preference for the following, but the glibc is really
	 * obsolete so I had to rather use much more complicated getaddrinfo().
	 * But we duplicate the code terribly here :|. */
	/* hostent = getipnodebyname(name, AF_INET6, AI_ALL | AI_ADDRCONFIG, NULL); */
	memset(&hint, 0, sizeof(hint));
	hint.ai_family = AF_UNSPEC;
	hint.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(name, NULL, &hint, &ai) != 0) return DNS_ERROR;

#else
	/* Seems there are problems on Mac, so we first need to try
	 * gethostbyaddr(), but there are problems with gethostbyaddr on Cygwin,
	 * so we do not use gethostbyaddr there. */
#if defined(HAVE_GETHOSTBYADDR) && !defined(HAVE_SYS_CYGWIN_H)
	{
		struct in_addr inp;

		if (is_ip_address(name, strlen(name)) && inet_aton(name, &inp))
			hostent = gethostbyaddr(&inp, sizeof(inp), AF_INET);
	}
	if (!hostent)
#endif
	{
		hostent = gethostbyname(name);
		if (!hostent) return DNS_ERROR;
	}
#endif

#ifdef CONFIG_IPV6
	for (i = 0, ai_cur = ai; ai_cur; i++, ai_cur = ai_cur->ai_next);
#else
	for (i = 0; hostent->h_addr_list[i] != NULL; i++);
#endif

	/* We cannot use mem_*() in thread ("It will chew memory on OS/2 and
	 * BeOS because there are no locks around the memory debugging code."
	 * -- Mikulas).  So we don't if in_thread != 0. */
	*addrs = in_thread ? calloc(i, sizeof(**addrs))
			   : mem_calloc(i, sizeof(**addrs));
	if (!*addrs) return DNS_ERROR;
	*addrno = i;

#ifdef CONFIG_IPV6
	for (i = 0, ai_cur = ai; ai_cur; i++, ai_cur = ai_cur->ai_next) {
		/* Don't use struct sockaddr_in6 here: because we
		 * called getaddrinfo with AF_UNSPEC, the address
		 * might not be for IP at all.  */
		struct sockaddr_storage *addr = &(*addrs)[i];

		/* RFC 3493 says struct sockaddr_storage is supposed
		 * to be "Large enough to accommodate all supported
		 * protocol-specific address structures."  So if
		 * getaddrinfo supports an address that does not fit
		 * in struct sockaddr_storage, then it is a bug in the
		 * library.  In this case, fail the whole lookup, to
		 * make the bug more likely to be noticed.  */
		assert(ai_cur->ai_addrlen <= sizeof(*addr));
		if_assert_failed {
			freeaddrinfo(ai);
			if (in_thread)
				free(*addrs);
			else
				mem_free(*addrs);
			*addrs = NULL;
			*addrno = 0;
			return DNS_ERROR;
		}

		memcpy(addr, ai_cur->ai_addr, ai_cur->ai_addrlen);
	}

	freeaddrinfo(ai);

#else
	for (i = 0; hostent->h_addr_list[i] != NULL; i++) {
		struct sockaddr_in *addr = (struct sockaddr_in *) &(*addrs)[i];

		addr->sin_family = hostent->h_addrtype;
		memcpy(&addr->sin_addr.s_addr, hostent->h_addr_list[i], hostent->h_length);
	}
#endif

	return DNS_SUCCESS;
}


/* Asynchronous DNS lookup management: */

#ifndef NO_ASYNC_LOOKUP
static enum dns_result
write_dns_data(int h, void *data, size_t datalen)
{
	size_t done = 0;

	do {
		int w = safe_write(h, data + done, datalen - done);

		if (w < 0) return DNS_ERROR;
		done += w;
	} while (done < datalen);

	assert(done == datalen);

	return DNS_SUCCESS;
}

static void
async_dns_writer(void *data, int h)
{
	unsigned char *name = (unsigned char *) data;
	struct sockaddr_storage *addrs;
	int addrno, i;

	if (do_real_lookup(name, &addrs, &addrno, 1) == DNS_ERROR)
		return;

	/* We will do blocking I/O here, however it's only local communication
	 * and it's supposed to be just a flash talk, so it shouldn't matter.
	 * And it would be incredibly more complicated and messy (and mainly
	 * useless) to do this in non-blocking way. */
	if (set_blocking_fd(h) < 0) return;

	if (write_dns_data(h, &addrno, sizeof(addrno)) == DNS_ERROR)
		return;

	for (i = 0; i < addrno; i++) {
		struct sockaddr_storage *addr = &addrs[i];

		if (write_dns_data(h, addr, sizeof(*addr)) == DNS_ERROR)
			return;
	}

	/* We're in thread, thus we must do plain free(). */
	free(addrs);
}

static enum dns_result
read_dns_data(int h, void *data, size_t datalen)
{
	size_t done = 0;

	do {
		ssize_t r = safe_read(h, data + done, datalen - done);

		if (r <= 0) return DNS_ERROR;
		done += r;
	} while (done < datalen);

	assert(done == datalen);

	return DNS_SUCCESS;
}

static void
async_dns_reader(struct dnsquery *query)
{
	enum dns_result result = DNS_ERROR;
	int i;

	/* We will do blocking I/O here, however it's only local communication
	 * and it's supposed to be just a flash talk, so it shouldn't matter.
	 * And it would be incredibly more complicated and messy (and mainly
	 * useless) to do this in non-blocking way. */
	if (set_blocking_fd(query->h) < 0) goto done;

	if (read_dns_data(query->h, &query->addrno, sizeof(query->addrno)) == DNS_ERROR)
		goto done;

	query->addr = mem_calloc(query->addrno, sizeof(*query->addr));
	if (!query->addr) goto done;

	for (i = 0; i < query->addrno; i++) {
		struct sockaddr_storage *addr = &query->addr[i];

		if (read_dns_data(query->h, addr, sizeof(*addr)) == DNS_ERROR)
			goto done;
	}

	result = DNS_SUCCESS;

done:
	if (result == DNS_ERROR)
		mem_free_set(&query->addr, NULL);

	done_dns_lookup(query, result);
}

static void
async_dns_error(struct dnsquery *query)
{
	done_dns_lookup(query, DNS_ERROR);
}

static int
init_async_dns_lookup(struct dnsquery *dnsquery, int force_async)
{
	if (!force_async && !get_opt_bool("connection.async_dns")) {
		dnsquery->h = -1;
		return 0;
	}

	dnsquery->h = start_thread(async_dns_writer, dnsquery->name,
				   strlen(dnsquery->name) + 1);
	if (dnsquery->h == -1)
		return 0;

	set_handlers(dnsquery->h, (select_handler_T) async_dns_reader, NULL,
		     (select_handler_T) async_dns_error, dnsquery);

	return 1;
}

static void
done_async_dns_lookup(struct dnsquery *dnsquery)
{
	if (dnsquery->h == -1) return;

	clear_handlers(dnsquery->h);
	close(dnsquery->h);
	dnsquery->h = -1;
}
#else
#define init_async_dns_lookup(dnsquery, force)	(0)
#define done_async_dns_lookup(dnsquery)		/* Nada. */
#endif /* NO_ASYNC_LOOKUP */


static enum dns_result
do_lookup(struct dnsquery *query, int force_async)
{
	enum dns_result result;

	/* DBG("starting lookup for %s", query->name); */

	/* Async lookup */
	if (init_async_dns_lookup(query, force_async))
		return DNS_ASYNC;

	/* Sync lookup */
	result = do_real_lookup(query->name, &query->addr, &query->addrno, 0);
	done_dns_lookup(query, result);

	return result;
}

static enum dns_result
do_queued_lookup(struct dnsquery *query)
{
#ifdef THREAD_SAFE_LOOKUP
	query->next_in_queue = NULL;

	if (dns_queue) {
		/* DBG("queuing lookup for %s", q->name); */
		assertm(!dns_queue->next_in_queue, "DNS queue corrupted");
		dns_queue->next_in_queue = query;
		dns_queue = query;
		return DNS_ERROR;
	}

	dns_queue = query;
#endif
	/* DBG("direct lookup"); */
	return do_lookup(query, 0);
}


static void
done_dns_lookup(struct dnsquery *query, enum dns_result result)
{
	struct dnsentry *dnsentry;

	/* DBG("end lookup %s (%d)", query->name, res); */

	/* do_lookup() might start a new async thread */
	done_async_dns_lookup(query);

#ifdef THREAD_SAFE_LOOKUP
	if (query->next_in_queue) {
		/* DBG("processing next in queue: %s", query->next_in_queue->name); */
		do_lookup(query->next_in_queue, 1);
	} else {
		dns_queue = NULL;
	}
#endif

	/* Make sure the query is unregister _before_ calling any callbacks. */
	*query->queryref = NULL;

	/* If the callback was cleared skip to the freeing part. */
	if (!query->done)
		goto done;

	dnsentry = find_in_dns_cache(query->name);
	if (dnsentry) {
		/* If the query failed, use the existing DNS cache entry even if
		 * it is too old. */
		if (result == DNS_ERROR) {
			query->done(query->data, dnsentry->addr, dnsentry->addrno);
			goto done;
		}

		del_dns_cache_entry(dnsentry);
	}

	if (result == DNS_SUCCESS)
		add_to_dns_cache(query->name, query->addr, query->addrno);

	query->done(query->data, query->addr, query->addrno);

done:
	mem_free_set(&query->addr, NULL);
	mem_free(query);
}

static enum dns_result
init_dns_lookup(unsigned char *name, void **queryref,
		dns_callback_T done, void *data)
{
	struct dnsquery *query;
	int namelen = strlen(name);

	query = mem_calloc(1, sizeof(*query) + namelen);
	if (!query) {
		done(data, NULL, 0);
		return DNS_ERROR;
	}

	query->done = done;
	query->data = data;

	/* calloc() sets NUL char for us. */
	memcpy(query->name, name, namelen);

	query->queryref = (struct dnsquery **) queryref;
	*(query->queryref) = query;

	return do_queued_lookup(query);
}


enum dns_result
find_host(unsigned char *name, void **queryref,
	  dns_callback_T done, void *data, int no_cache)
{
	struct dnsentry *dnsentry;

	assert(queryref);
	*queryref = NULL;

	if (no_cache)
		return init_dns_lookup(name, queryref, done, data);

	/* Check if the DNS name is in the cache. If the cache entry is too old
	 * do a new lookup. However, old cache entries will be used as a
	 * fallback if the new lookup fails. */
	dnsentry = find_in_dns_cache(name);
	if (dnsentry) {
		timeval_T age, now, max_age;

		assert(dnsentry && dnsentry->addrno > 0);

		timeval_from_seconds(&max_age, DNS_CACHE_TIMEOUT);
		timeval_now(&now);
		timeval_sub(&age, &dnsentry->creation_time, &now);

		if (timeval_cmp(&age, &max_age) <= 0) {
			done(data, dnsentry->addr, dnsentry->addrno);
			return DNS_SUCCESS;
		}
	}

	return init_dns_lookup(name, queryref, done, data);
}

void
kill_dns_request(void **queryref)
{
	struct dnsquery *query = *queryref;

	assert(query);

	query->done = NULL;
	done_dns_lookup(query, DNS_ERROR);
}

void
shrink_dns_cache(int whole)
{
	struct dnsentry *dnsentry, *next;

	if (whole) {
		foreachsafe (dnsentry, next, dns_cache)
			del_dns_cache_entry(dnsentry);

	} else {
		timeval_T now, max_age;

		timeval_from_seconds(&max_age, DNS_CACHE_TIMEOUT);
		timeval_now(&now);

		foreachsafe (dnsentry, next, dns_cache) {
			timeval_T age;

			timeval_sub(&age, &dnsentry->creation_time, &now);

			if (timeval_cmp(&age, &max_age) > 0)
				del_dns_cache_entry(dnsentry);
		}
	}
}
