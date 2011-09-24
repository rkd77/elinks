/* BitTorrent piece cache */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "main/select.h"
#include "network/connection.h"
#include "osdep/osdep.h"
#include "protocol/bittorrent/bencoding.h"
#include "protocol/bittorrent/common.h"
#include "protocol/bittorrent/connection.h"
#include "protocol/bittorrent/dialogs.h"
#include "protocol/bittorrent/peerwire.h"
#include "protocol/bittorrent/piececache.h"
#include "protocol/bittorrent/tracker.h"
#include "util/bitfield.h"
#include "util/error.h"
#include "util/file.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/random.h"
#include "util/string.h"


/* Used as a 'not defined' value for piece indexes. */
#define BITTORRENT_PIECE_UNDEF		UINT_MAX

/* Used as a 'not interesting' value for piece rarities. */
#define BITTORRENT_PIECE_RARITY_UNDEF	USHRT_MAX

/* A shorthand to reduce long lines. */
#define find_local_bittorrent_peer_request(peer, request) \
	get_bittorrent_peer_request(&(peer)->local, (request)->piece, \
				    (request)->offset, (request)->length)

static inline void
set_bittorrent_piece_cache_remaining(struct bittorrent_piece_cache *cache,
				     uint32_t piece, int remaining)
{
	cache->entries[piece].remaining = remaining > 0 ? 1 : 0;
	cache->remaining_pieces += remaining;
	cache->loading_pieces   += -remaining;
}

static inline void
set_bittorrent_piece_cache_completed(struct bittorrent_piece_cache *cache,
				     uint32_t piece)
{
	cache->entries[piece].completed = 1;
	cache->entries[piece].remaining = 0;
	cache->loading_pieces--;
	cache->completed_pieces++;
	set_bitfield_bit(cache->bitfield, piece);
}

static void
handle_bittorrent_mode_changes(struct bittorrent_connection *bittorrent)
{
	struct bittorrent_piece_cache *cache = bittorrent->cache;

	if (cache->completed_pieces == bittorrent->meta.pieces
	    || (cache->partial && cache->partial_pieces == cache->completed_pieces)) {
		struct bittorrent_peer_connection *peer;

		bittorrent->mode = BITTORRENT_MODE_SEEDER;

		/* Loose all interest ... */
		foreach (peer, bittorrent->peers)
			set_bittorrent_peer_not_interested(peer);

		if (cache->notify_complete)
			notify_bittorrent_download_complete(bittorrent);

		if (!cache->partial) {
			bittorrent->tracker.event = BITTORRENT_EVENT_COMPLETED;
			send_bittorrent_tracker_request(bittorrent->conn);
		}

	} else if (bittorrent->mode == BITTORRENT_MODE_PIECELESS) {
		int cutoff;

		cutoff = get_opt_int("protocol.bittorrent.rarest_first_cutoff",
		                     NULL);
		if (cache->completed_pieces >= cutoff)
			bittorrent->mode = BITTORRENT_MODE_NORMAL;
	}
}


/* ************************************************************************** */
/* Piece cache free list management: */
/* ************************************************************************** */

/* Search the free list for a request that is available from the peer. */
static struct bittorrent_peer_request *
find_bittorrent_free_list_peer_request(struct bittorrent_piece_cache *cache,
				       struct bittorrent_peer_connection *peer)
{
	struct bittorrent_peer_request *request;
	uint32_t piece = BITTORRENT_PIECE_UNDEF;

	foreach (request, cache->free_list) {
		/* Skip consecutive request for pieces that the peer does not
		 * have. */
		if (request->piece == piece)
			continue;

		piece = request->piece;

		/* Got piece? */
		if (!test_bitfield_bit(peer->bitfield, request->piece))
			continue;

		del_from_list(request);
		return request;
	}

	return NULL;
}

static inline int
randomize(size_t scale)
{
	double random = (double) rand() / RAND_MAX;
	int index = random * (scale - 1);

	return index;
}

/* Pseudo-randomly select a piece that is available from the peer. */
static uint32_t
find_random_in_bittorrent_piece_cache(struct bittorrent_piece_cache *cache,
				      struct bittorrent_peer_connection *peer)
{
	uint32_t pieces[] = {
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
	};
	int places = sizeof_array(pieces);
	uint32_t piece;

	assert(peer->bitfield->bitsize == peer->bittorrent->meta.pieces);

	seed_rand_once();

	foreachback_bitfield_set (piece, peer->bitfield) {
		assertm(cache->entries[piece].rarity,
			"Piece cache out of sync");

		if (!cache->entries[piece].remaining)
			continue;

		/* A little stupid, but in the first few iterations it tries to
		 * equally fill out the array with available pieces to quickly
		 * get a reasonable variety of pieces to choose from. It then
		 * changes to randomly position available pieces in the array.
		 * Assuming a piece table size of 10 it would behave in the
		 * following way:
		 *
		 *		  0 1 2 3 4 5 6 7 8 9
		 *	-----------------------------
		 *	 1. found * * * * * * * * * *
		 *	 2. found *   *   *   *   *
		 *	 3. found *     *       *
		 *	 4. found *       *       *
		 *	 5. found *         *
		 *	 6. found *           *
		 *	 7. found *             *
		 *	 8. found *               *
		 *	 9. found *                 *
		 *	10. found *
		 *	11. found random insertion
		 */
		/* FIXME: Maybe use the rarity index to get a ``better'' random
		 * piece which can be downloaded from multiple peers as opposed
		 * to the purpose of the rarest first strategy. */
		if (places >= 2) {
			int skip = sizeof_array(pieces) / places;
			int pos;

			for (pos = 0; pos < sizeof_array(pieces); pos += skip)
				pieces[pos] = piece;

			places /= 2;

		} else {
			pieces[randomize(sizeof_array(pieces))] = piece;
		}
	}

	return pieces[randomize(sizeof_array(pieces))];
}

/* Search for the rarest piece that is available from the peer. */
static uint32_t
find_rarest_in_bittorrent_piece_cache(struct bittorrent_piece_cache *cache,
				      struct bittorrent_peer_connection *peer)
{
	uint32_t pieces[] = {
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
		BITTORRENT_PIECE_UNDEF, BITTORRENT_PIECE_UNDEF,
	};
	int places = sizeof_array(pieces);
	uint16_t piece_rarity = BITTORRENT_PIECE_RARITY_UNDEF;
	uint32_t piece;

	assert(peer->bitfield->bitsize == peer->bittorrent->meta.pieces);

	seed_rand_once();

	/* Try to randomize the piece picking using the strategy from the random
	 * piece selection. */
	foreachback_bitfield_set (piece, peer->bitfield) {
		struct bittorrent_piece_cache_entry *entry;
		int pos, skip = sizeof_array(pieces) / places;

		entry = &cache->entries[piece];

		assertm(entry->rarity, "Piece cache out of sync");

		if (!entry->remaining || entry->rarity > piece_rarity)
			continue;

		if (entry->rarity < piece_rarity) {
			places = sizeof_array(pieces);
			piece_rarity = entry->rarity;

		} else if (places < 2) {
			pieces[randomize(sizeof_array(pieces))] = piece;
			continue;
		}

		skip = sizeof_array(pieces) / places;

		for (pos = 0; pos < sizeof_array(pieces); pos += skip)
			pieces[pos] = piece;

		places /= 2;
	}

	return pieces[randomize(sizeof_array(pieces))];
}

/* Search for a clonable request that is available from the peer. */
static struct bittorrent_peer_request *
find_clonable_bittorrent_peer_request(struct bittorrent_peer_connection *peer)
{
	struct bittorrent_piece_cache *cache = peer->bittorrent->cache;
	struct bittorrent_peer_connection *active_peer;
	struct bittorrent_peer_request *clone = NULL;
	uint16_t clone_rarity = BITTORRENT_PIECE_RARITY_UNDEF;

	foreach (active_peer, peer->bittorrent->peers) {
		struct bittorrent_peer_request *request = NULL;

		foreach (request, active_peer->local.requests) {
			uint16_t request_rarity;

			/* Check the the peer doesn't already have the complete
			 * piece or request. */
			if (!test_bitfield_bit(peer->bitfield, request->piece)
			    || find_local_bittorrent_peer_request(peer, request))
				continue;

			/* Prefer to clone requests which have not been cloned
			 * yet. */
			if (!request->cloned)
				return request;

			request_rarity = cache->entries[request->piece].rarity;

			/* If there are only already cloned requests left
			 * clone the rarest. */
			if (request_rarity >= clone_rarity)
				continue;

			clone	     = request;
			clone_rarity = request_rarity;
		}
	}

	return clone;
}

static struct bittorrent_peer_request *
clone_bittorrent_peer_request(struct bittorrent_peer_request *request)
{
	struct bittorrent_peer_request *clone;

	clone = mem_alloc(sizeof(*clone));
	if (!clone) return NULL;

	/* Both are now clones ... */
	request->cloned = 1;

	copy_struct(clone, request);

	/* ... but the new clone has not yet been requested. */
	clone->requested = 0;

	return clone;
}

/* Split the piece into multiple requests and add them to the free list
 * except for the first request which is returned. */
static struct bittorrent_peer_request *
add_piece_to_bittorrent_free_list(struct bittorrent_piece_cache *cache,
				  struct bittorrent_connection *bittorrent,
				  uint32_t piece)
{
	struct bittorrent_peer_request *request, *next;
	uint32_t request_length, piece_length, piece_offset;
	INIT_LIST_OF(struct bittorrent_peer_request, requests);
	uint16_t blocks = 0;

	assert(piece <= bittorrent->meta.pieces);

	piece_offset	= 0;
	piece_length	= get_bittorrent_piece_length(&bittorrent->meta, piece);
	request_length	= get_opt_int("protocol.bittorrent.peerwire.request_length", NULL);

	if (request_length > piece_length)
		request_length = piece_length;

	assertm(piece_length, "Default piece length unset");

	/* First initialize and add all pieces to the local request list. */
	do {
		request = mem_calloc(1, sizeof(*request));
		if (!request) break;

		request->piece  = piece;
		request->offset = piece_offset;
		/* The last block might have to be smaller. */
		request->length = piece_offset + request_length > piece_length
				? piece_length - piece_offset
				: request_length;
		request->block  = blocks++;

		piece_offset += request->length;
		add_to_list_end(requests, request);

	} while (piece_offset < piece_length);

	/* Make sure that requests was created for the entire piece. */
	if (piece_offset < piece_length) {
		free_list(requests);
		return NULL;
	}

	assert(piece_offset == piece_length);
	assertm(blocks, "Piece was not divided into blocks");
	assert(!cache->entries[piece].blocks);

	cache->entries[piece].blocks = init_bitfield(blocks);
	if (!cache->entries[piece].blocks) {
		free_list(requests);
		return NULL;
	}

	set_bittorrent_piece_cache_remaining(cache, piece, -1);

	assertm(cache->remaining_pieces < bittorrent->meta.pieces,
		"Cache count underflow");

	/* Actually move the requests to the free list, except the last one. */
	foreachsafe (request, next, requests) {
		del_from_list(request);
		if (list_empty(requests))
			return request;
		add_to_list_end(cache->free_list, request);
	}

	/* XXX: Should never happen! */
	INTERNAL("Bad request list appending.");
	return NULL;
}

struct bittorrent_peer_request *
find_bittorrent_peer_request(struct bittorrent_peer_connection *peer)
{
	struct bittorrent_connection *bittorrent = peer->bittorrent;
	struct bittorrent_piece_cache *cache = bittorrent->cache;
	struct bittorrent_peer_request *request;
	uint32_t piece;

	/* First try to browse the free list ... */

	if (list_empty(cache->free_list)) {
		/* Enter the end game mode when all remaining pieces have been
		 * requested. */
		if (cache->remaining_pieces == 0)
			bittorrent->mode = BITTORRENT_MODE_END_GAME;

	} else {
		request = find_bittorrent_free_list_peer_request(cache, peer);
		if (request) return request;
	}

	/* Are there remaining pieces? */

	switch (bittorrent->mode) {
	case BITTORRENT_MODE_PIECELESS:
		/* Find a random piece available from the peer. */
		piece = find_random_in_bittorrent_piece_cache(cache, peer);
		break;

	case BITTORRENT_MODE_NORMAL:
		/* Find the rarest piece available from the peer. */
		piece = find_rarest_in_bittorrent_piece_cache(cache, peer);
		break;

	case BITTORRENT_MODE_END_GAME:
		assert(cache->remaining_pieces == 0);
		request = find_clonable_bittorrent_peer_request(peer);
		if (!request) return NULL;

		return clone_bittorrent_peer_request(request);

	case BITTORRENT_MODE_SEEDER:
	default:
		INTERNAL("Not able to find peer request");
		return NULL;
	}

	if (piece == BITTORRENT_PIECE_UNDEF)
		return NULL;

	request = add_piece_to_bittorrent_free_list(cache, bittorrent, piece);

	/* Check for end game once again to update it sooner than later. */
	if (cache->remaining_pieces == 0
	    && list_empty(cache->free_list))
		bittorrent->mode = BITTORRENT_MODE_END_GAME;

	return request;
}

/* Find all dublicate request and clear the cloned flag if only one is found. */
static void
clear_cloned_bittorrent_peer_request(struct bittorrent_connection *bittorrent,
				     struct bittorrent_peer_request *request)
{
	struct bittorrent_peer_request *single_clone = NULL;
	struct bittorrent_peer_connection *peer;

	foreach (peer, bittorrent->peers) {
		struct bittorrent_peer_request *clone;

		clone = find_local_bittorrent_peer_request(peer, request);
		if (!clone) continue;

		/* More than one has the request pending so no cloned flags
		 * needs to be cleared. */
		if (single_clone)
			return;

		single_clone = clone;
	}

	if (single_clone)
		single_clone->cloned = 0;
}

static void
add_request_to_bittorrent_piece_cache(struct bittorrent_connection *bittorrent,
				      struct bittorrent_peer_request *request)
{
	del_from_list(request);
	if (!request->cloned) {
		/* fixme: ensure that the free list is sorted by piece the
		 * number of remaining request so that downloading of complete
		 * pieces are prioritized. */
		request->requested = 0;
		add_to_list(bittorrent->cache->free_list, request);
		return;
	}

	clear_cloned_bittorrent_peer_request(bittorrent, request);
	mem_free(request);
}

void
add_requests_to_bittorrent_piece_cache(struct bittorrent_peer_connection *peer,
				       struct bittorrent_peer_status *status)
{
	struct bittorrent_peer_request *request, *next;

	foreachsafe (request, next, status->requests) {
		add_request_to_bittorrent_piece_cache(peer->bittorrent, request);
	}
}


/* ************************************************************************** */
/* Piece cache rarity index management: */
/* ************************************************************************** */

void
update_bittorrent_piece_cache(struct bittorrent_peer_connection *peer,
			      uint32_t piece)
{
	struct bittorrent_piece_cache *cache = peer->bittorrent->cache;

	assert(piece <= peer->bittorrent->meta.pieces);

	if (!cache->entries[piece].completed
	    && !cache->entries[piece].rarity)
		cache->unavailable_pieces--;

	cache->entries[piece].rarity++;
	assertm(cache->entries[piece].rarity <= list_size(&peer->bittorrent->peers),
		"Piece rarity overflow");
}

void
update_bittorrent_piece_cache_from_bitfield(struct bittorrent_peer_connection *peer)
{
	uint32_t piece;

	assert(peer->bitfield->bitsize == peer->bittorrent->meta.pieces);

	foreach_bitfield_set (piece, peer->bitfield) {
		update_bittorrent_piece_cache(peer, piece);
	}
}

void
remove_bittorrent_peer_from_piece_cache(struct bittorrent_peer_connection *peer)
{
	struct bittorrent_piece_cache *cache = peer->bittorrent->cache;
	unsigned int piece;

	assert(peer->bitfield);

	foreach_bitfield_set (piece, peer->bitfield) {
		cache->entries[piece].rarity--;
		assertm(cache->entries[piece].rarity <= list_size(&peer->bittorrent->peers),
			"Piece rarity underflow");

		if (!cache->entries[piece].completed
		    && !cache->entries[piece].rarity)
			cache->unavailable_pieces++;
	}
}


/* ************************************************************************** */
/* Piece cache disk management: */
/* ************************************************************************** */

/* Recursively create directories in a path. The last element in the path is
 * taken to be a filename, and simply ignored */
static enum bittorrent_state
create_bittorrent_path(unsigned char *path)
{
	int ret = mkalldirs(path);

	return (ret ? BITTORRENT_STATE_ERROR : BITTORRENT_STATE_OK);
}

/* Complementary to the above rmdir()s each directory in the path. */
static void
remove_bittorrent_path(struct bittorrent_meta *meta, unsigned char *path)
{
	unsigned char *root = strstr(path, meta->name);
	int pos;

	assert(meta->type == BITTORRENT_MULTI_FILE);

	if (!root || root == path)
		return;

	for (pos = strlen(root); pos >= 0; pos--) {
		unsigned char separator = root[pos];
		int ret;

		if (!dir_sep(separator))
			continue;

		root[pos] = 0;

		ret = rmdir(path);

		path[pos] = separator;

		if (ret < 0) return;
	}
}

static unsigned char *
get_bittorrent_file_name(struct bittorrent_meta *meta, struct bittorrent_file *file)
{
	unsigned char *name;

	name = expand_tilde(get_opt_str("document.download.directory", NULL));
	if (!name) return NULL;

	add_to_strn(&name, "/");
	add_to_strn(&name, meta->name);

	if (meta->type == BITTORRENT_MULTI_FILE) {
		add_to_strn(&name, "/");
		add_to_strn(&name, file->name);
	}

	return name;
}

enum bittorrent_translation {
	BITTORRENT_READ,
	BITTORRENT_WRITE,
	BITTORRENT_SEEK,
};

static int
open_bittorrent_file(struct bittorrent_meta *meta, struct bittorrent_file *file,
		     enum bittorrent_translation trans, off_t offset)
{
	unsigned char *name = get_bittorrent_file_name(meta, file);
	off_t seek_result;
	int fd;
	int flags = (trans == BITTORRENT_WRITE ? O_WRONLY : O_RDONLY);

	assert(trans != BITTORRENT_SEEK);

	if (!name) return -1;

	fd = open(name, flags, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		/* 99% of the time the file will already exist so special case
		 * the directory and file creation. */
		if (errno == ENOENT
		    && trans == BITTORRENT_WRITE
		    && create_bittorrent_path(name) == BITTORRENT_STATE_OK)
			fd = open(name, flags | O_CREAT, S_IRUSR | S_IWUSR);
	}

	mem_free(name);

	if (fd == -1) return -1;

	if (trans == BITTORRENT_READ) {
		struct stat stat;

		if (fstat(fd, &stat) < -1 || stat.st_size < offset) {
			close(fd);
			return -1;
		}
	}

	seek_result = lseek(fd, offset, SEEK_SET);
	if (seek_result == (off_t) -1 || seek_result != offset) {
		close(fd);
		return -1;
	}

	return fd;
}

static enum bittorrent_state
bittorrent_file_piece_translation(struct bittorrent_meta *meta,
				  struct bittorrent_piece_cache *cache,
				  struct bittorrent_piece_cache_entry *entry,
				  uint32_t piece,
				  enum bittorrent_translation trans)
{
	struct bittorrent_file *file;
	uint32_t piece_length = get_bittorrent_piece_length(meta, piece);
	uint32_t piece_offset = 0;
	off_t torrent_offset = (off_t) meta->piece_length * piece;
	off_t current_offset = 0;
	enum bittorrent_state state = BITTORRENT_STATE_OK;
	int prev_selected = -1;

	if (trans == BITTORRENT_READ) {
		assert(!entry->data);

		entry->data = mem_mmap_alloc(piece_length);
		if (!entry->data)
			return BITTORRENT_STATE_OUT_OF_MEM;
	}

	foreach (file, meta->files) {
		unsigned char *data;
		off_t file_offset, file_length;
		uint32_t data_length;
		ssize_t length;
		int fd;

		if (piece_offset >= piece_length)
			break;

		if (current_offset + file->length < torrent_offset) {
			current_offset += file->length;
			continue;
		}

		assert(current_offset <= torrent_offset + piece_length);

		/* Figure out where in the file we need access. */
		file_offset = torrent_offset - current_offset;
		if (file_offset < 0)
			file_offset = 0;

		assertm(file_offset <= file->length, "%ld %ld", file_offset, file->length);

		/* How much needs to be read or written. */
		data_length = piece_length - piece_offset;
		file_length = file->length - file_offset;
		if (data_length > file_length)
			data_length = file_length;

		if (trans == BITTORRENT_SEEK) {
			/* Mark all pieces which need to be downloaded. */
			if (!entry->selected)
				entry->selected = (file->selected == 1);

			/* Lock entries that span both selected and unselected
			 * files in memory. */
			if (prev_selected != -1
			    && prev_selected != file->selected)
				entry->locked = 1;

			prev_selected = file->selected;

			/* Prepare the next iteration. */
			piece_offset   += data_length;
			current_offset += file->length;
			continue;
		}

		/* Jump on unselected files. */
		if (!file->selected) {
			if (trans != BITTORRENT_READ
			    || !entry->completed) {
				/* Prepare the next iteration. */
				piece_offset   += data_length;
				current_offset += file->length;
				continue;
			}

			/* Allow reading of pieces which is resumable. */
			assert(entry->completed && trans == BITTORRENT_READ);
		}

		/* Open at the desired offset and possibly create the file and
		 * parent directories. */
		fd = open_bittorrent_file(meta, file, trans, file_offset);
		if (fd == -1) {
			/* Try to gracefully handle bogus paths; empty file
			 * names and directory names. */
			if (!file->name[0]
			    || dir_sep(file->name[strlen(file->name) - 1])) {
				current_offset += file->length;
				continue;
			}

			if (trans == BITTORRENT_WRITE) {
				state = BITTORRENT_STATE_ERROR;
				break;
			}

			state = BITTORRENT_STATE_FILE_MISSING;
			break;
		}

		assert(entry->data);

		data = &entry->data[piece_offset];

		/* Do it! ;-) */
		if (trans == BITTORRENT_READ) {
			length = safe_read(fd, data, data_length);
		} else {
			length = safe_write(fd, data, data_length);
		}

		/* Cleanup. */
		close(fd);

		/* Check if the operation failed. */
		if (length != data_length)
			return BITTORRENT_STATE_ERROR;

		/* Prepare the next iteration. */
		piece_offset   += data_length;
		current_offset += file->length;
	}

	if (state != BITTORRENT_STATE_OK || piece_offset != piece_length) {
		if (piece_offset != piece_length)
			state = BITTORRENT_STATE_ERROR;

		assert(trans != BITTORRENT_SEEK);

		mem_mmap_free(entry->data, piece_length);
		entry->data = NULL;

		return state;
	}

	if (trans != BITTORRENT_SEEK) {
		/* Whether we just read or wrote the piece, it is now
		 * disposable. */
		add_to_list(cache->queue, entry);
	}

	return BITTORRENT_STATE_OK;
}


/* ************************************************************************** */
/* Piece cache block data management: */
/* ************************************************************************** */

static void
cancel_cloned_bittorrent_peer_requests(struct bittorrent_connection *bittorrent,
				       struct bittorrent_peer_request *request)
{
	struct bittorrent_peer_connection *peer;

	foreach (peer, bittorrent->peers) {
		struct bittorrent_peer_request *clone;

		clone = find_local_bittorrent_peer_request(peer, request);
		if (!clone)
			continue;

		/* Only cancel it if it was actually requested. */
		if (clone->requested)
			cancel_bittorrent_peer_request(peer, clone);

		del_from_list(clone);
		mem_free(clone);
	}
}

enum bittorrent_state
add_to_bittorrent_piece_cache(struct bittorrent_peer_connection *peer,
			      uint32_t piece, uint32_t offset,
			      unsigned char *data, uint32_t datalen,
			      int *write_errno)
{
	struct bittorrent_connection *bittorrent = peer->bittorrent;
	struct bittorrent_meta *meta = &bittorrent->meta;
	struct bittorrent_piece_cache *cache = bittorrent->cache;
	struct bittorrent_piece_cache_entry *entry;
	struct bittorrent_peer_request *request;
	enum bittorrent_state state;
	uint32_t piece_length;

	assert(piece < bittorrent->meta.pieces);
	assert(offset + datalen <= get_bittorrent_piece_length(meta, piece));
	assert(datalen);

	entry = &cache->entries[piece];

	/* The blocks bitfield is always allocated when downloading a piece. */
	if (!entry->blocks)
		return BITTORRENT_STATE_OK;

	/* Only accept data from pending requests. */
	request = get_bittorrent_peer_request(&peer->local, piece, offset, datalen);
	if (!request)
		return BITTORRENT_STATE_OK;

	piece_length = get_bittorrent_piece_length(meta, piece);
	if (!entry->data) {
		entry->data = mem_mmap_alloc(piece_length);
		if (!entry->data) {
			add_request_to_bittorrent_piece_cache(peer->bittorrent, request);
			return BITTORRENT_STATE_OK;
		}
	}

	memcpy(&entry->data[offset], data, datalen);

	set_bitfield_bit(entry->blocks, request->block);
	assertm(get_bitfield_set_count(entry->blocks),
		"Bit %u in bitfield size %u",
		request->block, entry->blocks->bitsize);

	update_bittorrent_connection_stats(bittorrent, request->length, 0, 0);

	/* Remove it from the request queue so it won't be found when canceling
	 * cloned requests. */
	del_from_list(request);
	if (request->cloned)
		cancel_cloned_bittorrent_peer_requests(bittorrent, request);
	mem_free(request);

	if (!bitfield_is_set(entry->blocks))
		return BITTORRENT_STATE_OK;

	mem_free_set(&entry->blocks, NULL);

	if (!bittorrent_piece_is_valid(meta, piece, entry->data, piece_length)) {
		/* Bad hash ... try againing! */
		cache->rejected_pieces++;
		mem_mmap_free(entry->data, piece_length);
		entry->data = NULL;
		update_bittorrent_connection_stats(bittorrent, -(off_t)piece_length, 0, 0);
		set_bittorrent_piece_cache_remaining(cache, piece, 1);
		if (bittorrent->mode == BITTORRENT_MODE_END_GAME)
			bittorrent->mode = BITTORRENT_MODE_NORMAL;

		return BITTORRENT_STATE_OK;
	}

	set_bittorrent_piece_cache_completed(cache, piece);
	set_bittorrent_peer_have(peer, piece);

	/* Write to disk. */
	errno = 0;
	state = bittorrent_file_piece_translation(&bittorrent->meta, cache,
						  entry, piece, BITTORRENT_WRITE);
	if (state != BITTORRENT_STATE_OK) {
		if (errno)
			*write_errno = errno;

		return state;
	}

	/* Handle mode changes ... */
	handle_bittorrent_mode_changes(bittorrent);

	return BITTORRENT_STATE_OK;
}

unsigned char *
get_bittorrent_piece_cache_data(struct bittorrent_connection *bittorrent,
				uint32_t piece)
{
	struct bittorrent_piece_cache *cache = bittorrent->cache;
	struct bittorrent_piece_cache_entry *entry;
	enum bittorrent_state state;

	assert(piece < bittorrent->meta.pieces);

	entry = &cache->entries[piece];

	if (!entry->completed)
		return NULL;

	if (entry->data) {
		move_to_top_of_list(cache->queue, entry);
		return entry->data;
	}

	/* Load the piece data from disk. */
	state = bittorrent_file_piece_translation(&bittorrent->meta, cache,
						  entry, piece, BITTORRENT_READ);
	if (state != BITTORRENT_STATE_OK)
		return NULL;

	return entry->data;
}


/* ************************************************************************** */
/* Asynchronous and threaded piece cache resuming: */
/* ************************************************************************** */

static void
done_bittorrent_resume(struct bittorrent_piece_cache *cache)
{
	if (cache->resume_fd == -1)
		return;

	clear_handlers(cache->resume_fd);
	close(cache->resume_fd);
	cache->resume_fd = -1;
}

/* Mark all pieces which desn't need to be downloaded and lock entries that span
 * both selected and unselected files in memory. */
static void
prepare_partial_bittorrent_download(struct bittorrent_connection *bittorrent)
{
	struct bittorrent_piece_cache *cache = bittorrent->cache;
	off_t est_length = 0;
	off_t completed = 0;
	uint32_t piece = 0;

	for (piece = 0; piece < bittorrent->meta.pieces; piece++) {
		struct bittorrent_piece_cache_entry *entry;
		uint32_t piece_length;
		enum bittorrent_state state;

		entry = &cache->entries[piece];

		state = bittorrent_file_piece_translation(&bittorrent->meta,
							  cache, entry, piece,
							  BITTORRENT_SEEK);
		assert(state == BITTORRENT_STATE_OK);
		assertm(!entry->locked || entry->selected,
			"All locked pieces should be selected");

		if (entry->locked || !entry->selected)
			cache->partial = 1;

		if (entry->locked)
			cache->locked_pieces++;

		piece_length = get_bittorrent_piece_length(&bittorrent->meta, piece);
		if (entry->selected) {
			cache->partial_pieces++;
			est_length += piece_length;
			if (entry->completed)
				completed += piece_length;

		} else if (entry->remaining) {
			/* Update the number of remaining pieces. */
			set_bittorrent_piece_cache_remaining(cache, piece, -1);
		}
	}

	if (cache->partial) {
		bittorrent->conn->est_length = est_length;
		bittorrent->conn->from = completed;
	}
}

static void
end_bittorrent_resume(struct bittorrent_connection *bittorrent)
{
	struct bittorrent_piece_cache *cache = bittorrent->cache;

	done_bittorrent_resume(bittorrent->cache);

	prepare_partial_bittorrent_download(bittorrent);
	handle_bittorrent_mode_changes(bittorrent);

	/* The resuming will decrement the loading count to -pieces. */
	cache->loading_pieces = 0;
	cache->unavailable_pieces = bittorrent->meta.pieces - cache->completed_pieces;

	bittorrent_resume_callback(bittorrent);
}

static void
bittorrent_resume_writer(void *data, int fd)
{
	struct bittorrent_piece_cache cache;
	struct bittorrent_meta meta;
	struct bittorrent_const_string metafile;
	uint32_t piece;

	memcpy(&metafile.length, data, sizeof(metafile.length));
	metafile.source = (const unsigned char *) data + sizeof(metafile.length);

	if (parse_bittorrent_metafile(&meta, &metafile) != BITTORRENT_STATE_OK) {
		done_bittorrent_meta(&meta);
		return;
	}

	memset(&cache, 0, sizeof(cache));
	init_list(cache.queue);

	if (set_blocking_fd(fd) < 0) {
		done_bittorrent_meta(&meta);
		return;
	}

	for (piece = 0; piece < meta.pieces; piece++){
		struct bittorrent_piece_cache_entry entry;
		enum bittorrent_state state;
		uint32_t length;
		char complete;
		int written;

		length = get_bittorrent_piece_length(&meta, piece);
		memset(&entry, 0, sizeof(entry));

		/* Attempt to read this piece from cache. This is used to resume
		 * a download */
		state = bittorrent_file_piece_translation(&meta, &cache, &entry,
							  piece, BITTORRENT_READ);
		if (state == BITTORRENT_STATE_OK
		    && bittorrent_piece_is_valid(&meta, piece, entry.data, length)) {
			complete = 1;
		} else {
			complete = 0;
		}

		if (entry.data) {
			if (state == BITTORRENT_STATE_OK)
				del_from_list(&entry);
			mem_mmap_free(entry.data, length);
		}

		written = safe_write(fd, &complete, 1);
		if (written < 0)
			break;
	}

	done_bittorrent_meta(&meta);
}

static void
bittorrent_resume_reader(struct bittorrent_connection *bittorrent)
{
	struct bittorrent_piece_cache *cache = bittorrent->cache;
	char completed[MAX_STR_LEN];
	ssize_t size, pos;

	set_connection_timeout(bittorrent->conn);

	size = safe_read(cache->resume_fd, completed, sizeof(completed));
	if (size <= 0) {
		end_bittorrent_resume(bittorrent);
		return;
	}

	for (pos = 0; pos < size; pos++) {
		uint32_t piece = cache->resume_pos++;
		uint32_t length;

		if (piece >= bittorrent->meta.pieces) {
			end_bittorrent_resume(bittorrent);
			return;
		}

		assert(piece < bittorrent->meta.pieces);

		if (!completed[pos])
			continue;

		length = get_bittorrent_piece_length(&bittorrent->meta, piece);

		set_bittorrent_piece_cache_completed(cache, piece);
		cache->remaining_pieces--;

		bittorrent->left -= length;
		bittorrent->conn->from += length;
	}
}

static void
start_bittorrent_resume(struct bittorrent_connection *bittorrent,
			struct bittorrent_const_string *meta)
{
	struct bittorrent_piece_cache *cache = bittorrent->cache;
	struct string info;

	assert(cache && cache->resume_fd == -1);

	if (!init_string(&info)) return;

	add_bytes_to_string(&info, (void *) &meta->length, sizeof(meta->length));
	add_bytes_to_string(&info, meta->source, meta->length);

	cache->resume_fd = start_thread(bittorrent_resume_writer, info.source,
					info.length);
	done_string(&info);

	if (cache->resume_fd == -1)
		return;

	if (set_blocking_fd(cache->resume_fd) < 0) {
		close(cache->resume_fd);
		cache->resume_fd = -1;
		return;
	}

	set_handlers(cache->resume_fd,
		     (select_handler_T) bittorrent_resume_reader, NULL,
		     (select_handler_T) end_bittorrent_resume, bittorrent);
}


/* ************************************************************************** */
/* Piece cache life-cycle: */
/* ************************************************************************** */

/* Periodically called to shrink the cache. */
/* The strategy is to  keep the n most recently accessed pieces in memory, chunk
 * the rest n depends on amount of memory available */
void
update_bittorrent_piece_cache_state(struct bittorrent_connection *bittorrent)
{
	struct bittorrent_piece_cache *cache = bittorrent->cache;
	struct bittorrent_piece_cache_entry *entry, *next;
	off_t cache_size = get_opt_int("protocol.bittorrent.piece_cache_size",
	                               NULL);
	off_t current_size = 0;

	if (!cache_size) return;

	foreachsafe (entry, next, cache->queue) {
		uint32_t piece_length, piece;

		assert(entry->data && entry->completed);

		piece = entry - cache->entries;

		assert(piece < bittorrent->meta.pieces);
		assert(entry == &cache->entries[piece]);

		piece_length = get_bittorrent_piece_length(&bittorrent->meta, piece);
		current_size += piece_length;

		/* Allow at least one piece! */
		if (current_size < cache_size || entry->locked)
			continue;

		del_from_list(entry);
		mem_mmap_free(entry->data, piece_length);
		entry->data = NULL;
	}
}

enum bittorrent_state
init_bittorrent_piece_cache(struct bittorrent_connection *bittorrent,
			    struct bittorrent_const_string *metafile)
{
	struct bittorrent_piece_cache *cache;
	uint32_t pieces = bittorrent->meta.pieces;
	size_t cache_entry_size = sizeof(*cache->entries) * pieces;
	uint32_t piece;

	cache = mem_calloc(1, sizeof(*cache) + cache_entry_size);
	if (!cache) return BITTORRENT_STATE_OUT_OF_MEM;

	cache->bitfield  = init_bitfield(pieces);
	cache->resume_fd = -1;

	if (!cache->bitfield) {
		mem_free(cache);
		return BITTORRENT_STATE_OUT_OF_MEM;
	}

	init_list(cache->queue);
	init_list(cache->free_list);

	bittorrent->cache = cache;

	/* Do the initialization of the connection stats; bytes left and
	 * estimated length in particular. Placed here so that downloaded
	 * resuming can mangle bytes left if it has to. */
	update_bittorrent_connection_stats(bittorrent, 0, 0, 0);

 	for (piece = 0; piece < pieces; piece++)
 		set_bittorrent_piece_cache_remaining(cache, piece, 1);

 	start_bittorrent_resume(bittorrent, metafile);

	assert(list_empty(cache->queue));

 	return cache->resume_fd == -1
 		? BITTORRENT_STATE_OK : BITTORRENT_STATE_CACHE_RESUME;
}

static void
delete_bittorrent_files(struct bittorrent_connection *bittorrent)
{
	struct bittorrent_meta *meta = &bittorrent->meta;
	struct bittorrent_file *file;

	foreach (file, meta->files) {
		unsigned char *name;

		if (!file->selected)
			continue;

		name = get_bittorrent_file_name(meta, file);
		if (!name) continue;

		unlink(name);

		if (meta->type == BITTORRENT_MULTI_FILE)
			remove_bittorrent_path(meta, name);

		mem_free(name);
	}
}

void
done_bittorrent_piece_cache(struct bittorrent_connection *bittorrent)
{
	struct bittorrent_piece_cache *cache = bittorrent->cache;
	uint32_t piece;

	done_bittorrent_resume(cache);

	for (piece = 0; piece < bittorrent->meta.pieces; piece++) {
		struct bittorrent_piece_cache_entry *entry;
		size_t length;

		entry = &cache->entries[piece];

		assertm(entry->rarity == 0, "Rarity out of sync (%u for %u)",
			entry->rarity, piece);

		mem_free_if(entry->blocks);
		if (!entry->data)
			continue;

		length = get_bittorrent_piece_length(&bittorrent->meta, piece);
		mem_mmap_free(entry->data, length);
	}

	if (cache->delete_files)
		delete_bittorrent_files(bittorrent);

	free_list(cache->free_list);
	mem_free_if(cache->bitfield);
	mem_free(cache);
}
