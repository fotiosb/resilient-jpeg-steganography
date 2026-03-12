/*
 * jsteg.h
 * Copyright (C) Micheal Smith 2010 <xulfer at cheapbsd.net>
 *
 * jsteg.h is free software copyrighted by Micheal Smith.
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

#ifndef _JSTEG_H
#define _JSTEG_H

#include "jpeg-v4/bitsource.h"
#include "jpeg-v4/bitsink.h"
#include "jpeg.h"

#define FILL_ZERO   0x01
#define FILL_DUP    0x02

#ifdef	__cplusplus
extern          "C" {
#endif

	int             jsteg_inject_buffer(void *, size_t);
	int             jsteg_extract_buffer(void *, size_t);
	long            jsteg_max_size(struct image_data *);
	long            jsteg_max_size_buf(long w, long h);
	void            jsteg_compress(void *, const char *, size_t, long);
	struct image_data *jsteg_decompress(void **, const char *);

#ifdef __cplusplus
}
#endif
#endif
