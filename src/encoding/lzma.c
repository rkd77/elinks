/* lzma encoding (ENCODING_LZMA) backend */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "encoding/LzmaDecode.h"
#include "encoding/encoding.h"
#include "encoding/lzma.h"
#include "util/memory.h"

#define LZMAMAXOUTPUT 2097152

struct lzma_enc_data {
	unsigned char *output;
	off_t current;
	off_t outSize;
};

static void
lzma_cleanup(struct lzma_enc_data *data)
{
	mem_free_if(data->output);
	mem_free(data);
}

static int
lzma_open(struct stream_encoded *stream, int fd)
{
	CLzmaDecoderState state;
	struct stat buf;
	struct lzma_enc_data *data;
	ssize_t nb;
	size_t inSize, inProcessed, outProcessed;
	int res;
	unsigned char *input, *inData;
	unsigned int i;

	if (fstat(fd, &buf)) return -1;
	if (!S_ISREG(buf.st_mode)) return -1;
	if (buf.st_size < LZMA_PROPERTIES_SIZE + 8) return -1;
	data = mem_calloc(1, sizeof(*data));
	if (!data) return -1;
	input = mem_alloc(buf.st_size);
	if (!input) {
		mem_free(data);
		return -1;
	}
	nb = safe_read(fd, input, buf.st_size);
	close(fd);
	if (nb != buf.st_size) {
		mem_free(input);
		lzma_cleanup(data);
		return -1;
	}
	if (LzmaDecodeProperties(&state.Properties, input,
	    LZMA_PROPERTIES_SIZE) != LZMA_RESULT_OK) {
	    mem_free(input);
	    lzma_cleanup(data);
	    return -1;
	}
	state.Probs = (CProb *)mem_alloc(LzmaGetNumProbs(
		             &state.Properties) * sizeof(CProb));
	if (!state.Probs) {
		mem_free(input);
		lzma_cleanup(data);
		return -1;
	}
	inSize = buf.st_size - LZMA_PROPERTIES_SIZE - 8;
	inData = input + LZMA_PROPERTIES_SIZE;
	data->outSize = 0;

	/* The size is 8 bytes long, but who wants such big files */
	for (i = 0; i < 4; i++) {
		unsigned char b = inData[i];
		data->outSize += (unsigned int)(b) << (i * 8);
	}
	if (data->outSize == 0xffffffff) data->outSize = LZMAMAXOUTPUT;

	data->output = mem_alloc(data->outSize);
	if (!data->output) {
		mem_free(state.Probs);
		mem_free(input);
		lzma_cleanup(data);
		return -1;
	}
	inData += 8;
	res = LzmaDecode(&state, inData, inSize, &inProcessed,
		data->output, data->outSize, &outProcessed);
	if (res) {
		mem_free(state.Probs);
		mem_free(input);
		lzma_cleanup(data);
		return -1;
	}
	data->outSize = outProcessed;
	data->current = 0;
	mem_free(input);
	mem_free(state.Probs);
	stream->data = data;

	return 0;
}

static int
lzma_read(struct stream_encoded *stream, unsigned char *buf, int len)
{
	struct lzma_enc_data *data = (struct lzma_enc_data *) stream->data;

	if (data->current + len > data->outSize)
		len = data->outSize - data->current;

	if (len < 0) return -1;
	memcpy(buf, data->output + data->current, len);
	data->current += len;

	return len;
}

static unsigned char *
lzma_decode(struct stream_encoded *stream, unsigned char *data, int len,
	     int *new_len)
{
	*new_len = len;
	return data;
}

static unsigned char *
lzma_decode_buffer(unsigned char *data, int len, int *new_len)
{
	CLzmaDecoderState state;
	size_t inSize, inProcessed, outProcessed;
	int res, outSize;
	unsigned char *inData;
	unsigned char *output;
	unsigned int i;

	if (len < LZMA_PROPERTIES_SIZE + 8) return NULL;
	if (LzmaDecodeProperties(&state.Properties, data,
	    LZMA_PROPERTIES_SIZE) != LZMA_RESULT_OK) {
	    return NULL;
	}
	state.Probs = (CProb *)mem_alloc(LzmaGetNumProbs(
		             &state.Properties) * sizeof(CProb));
	if (!state.Probs) {
		return NULL;
	}
	inSize = len - LZMA_PROPERTIES_SIZE - 8;
	inData = data + LZMA_PROPERTIES_SIZE;
	outSize = 0;

	for (i = 0; i < 4; i++) {
		unsigned char b = inData[i];
		outSize += (unsigned int)(b) << (i * 8);
	}
	if (outSize == 0xffffffff) outSize = LZMAMAXOUTPUT;

	output = mem_alloc(outSize);
	if (!output) {
		mem_free(state.Probs);
		return NULL;
	}
	inData += 8;
	res = LzmaDecode(&state, inData, inSize, &inProcessed, output,
		outSize, &outProcessed);
	if (res) {
		mem_free(state.Probs);
		mem_free(output);
		return NULL;
	}
	*new_len = outProcessed;

	return output;
}

static void
lzma_close(struct stream_encoded *stream)
{
	struct lzma_enc_data *data = (struct lzma_enc_data *) stream->data;

	lzma_cleanup(data);
}

static unsigned char *lzma_extensions[] = { ".lzma", NULL };

struct decoding_backend lzma_decoding_backend = {
	"lzma",
	lzma_extensions,
	lzma_open,
	lzma_read,
	lzma_decode,
	lzma_decode_buffer,
	lzma_close,
};
