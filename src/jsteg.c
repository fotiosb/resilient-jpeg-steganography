/*
 * jsteg.c
 * Copyright (C) Micheal Smith 2010 <xulfer at cheapbsd.net>
 *
 * jsteg.c is free software copyrighted by Micheal Smith.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name ``Micheal Smith'' nor the name of any other
 *    contributor may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * llist.h IS PROVIDED BY Micheal Smith ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Micheal Smith OR ANY OTHER CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include "jpeg-v4/jinclude.h"
#include "jsteg.h"

#define DUP_SEPERATOR '|'

static void    *
jsteg_dup_buffer(void *buf, size_t bufsz, long max, long *newsz)
{
	char           *nbuf, *dst, *src;
	long            pos = 0;

	if (bufsz < max)
		nbuf = malloc(max);
	else
		return buf;

	dst = (char *) nbuf;

	while ((pos + bufsz + 1) < max) {
		src = (char *) buf;
		while (*src != '\0')
			*dst++ = *src++;
		*dst++ = DUP_SEPERATOR;
		pos += bufsz + 1;
	}

	*newsz = pos;

	return nbuf;
}

int
jsteg_inject_buffer(void *buf, size_t bufsz)
{
	bitloadbuffer(buf, bufsz);
	return 0;
}

int
jsteg_extract_buffer(void *buf, size_t bufsz)
{
	bitsavebuffer(buf, bufsz);
	return 0;
}

void
jsteg_compress(void *buf, const char *filename, size_t bufsz, long max)
{
	void           *dupbuf = buf;
	long            w, h, newsz = (long)bufsz;  /* default: use bufsz if max==0 */

	if (max != 0)
		dupbuf = jsteg_dup_buffer(buf, bufsz, max, &newsz);

	jsteg_inject_buffer(dupbuf, newsz);
	steg_compress(filename, 0);
	free(dupbuf);
}

/*
 * This allocates memory for the buffer, and returns
 * it via the buf argument.  This must be free'd by the
 * caller.
 */
struct image_data *
jsteg_decompress(void **buf, const char *filename)
{
	long            w, h, max;
	struct image_data *image;
	unsigned char  *nbuf;

	get_xy(filename, &w, &h);
	max = jsteg_max_size_buf(w, h);

	nbuf = malloc(max + 2);  /* +2: one for null terminator at index max, one guard byte */
	*buf = nbuf;

	jsteg_extract_buffer(nbuf, max);
	image = steg_decompress(filename);

	nbuf[max] = '\0';  /* null-terminate within the allocated buffer */

	return image;
}

long
jsteg_max_size(struct image_data *image)
{
	return (long) ((image->width * image->height) / DCTSIZE2);
}

long
jsteg_max_size_buf(long w, long h)
{
	return (long) ((w * h) / DCTSIZE2);
}
