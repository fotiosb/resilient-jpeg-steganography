/*
 * jpeg.h
 * Copyright (C) Micheal Smith 2010 <xulfer at cheapbsd.net>
 *
 * jpeg.h is free software copyrighted by Micheal Smith.
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

#ifndef _JPEG_H
#define _JPEG_H

#include <math.h>
#include <sys/types.h>

//#include "jpeg-v4/jinclude.h"
//#include "jsteg.h"

#ifdef __cplusplus
extern          "C" {
#endif

	enum colors {
		RED = 0,
		GREEN,
		BLUE,
		Y = 0,
		CB,
		CR
	};
	
        //extern compress_info_ptr      my_cinfo;

        //extern int                      spike[2048][2048];
	extern int**                    spike;
	
	//extern int                      rowAnchorSpike[2048][2048];
	extern int**                    rowAnchorSpike;
	
	//extern int (*spike)[];

        //extern float                  Steg_Encode_Col_Step;

	/*
	 * Primary struct for storing / accessing
	 * image data between steg functions.
	 * Fotios: this is where bitmap data is stored after JPEG file is decompressed
	 */
	struct image_data {
		long            width;
		long            height;
		off_t           datalen;
		int             colorspace;
		short           components;
		short           precision;
		void          **data;
	};

	int             get_xy(const char *, long *, long *);
	
	void            steg_decode_init( char*,
	                                  short,
					  short,
					  short,
					  short,
					  short,
					  short,
					  short,
					  short,
					  short );
	
	void            steg_init( char*,
				   short,
				   short,
				   short,
				   short,
				   short,
				   short,
				   short,
				   short,
				   short,
				   short,
				   short,
				   short,
				   short,
				   short,
				   short,
				   short,
				   char* );
	
	void            steg_cleanup(void);
	struct image_data *steg_decompress(const char *);
	int             steg_compress(const char *, int);
	int             steg_compress2(int);
	int             annotate_jpeg(struct image_data *, const char *);
	
	////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////
	void            find_chroma_spike_cols3(int);
	void            find_chroma_spikes(int, int);
	void            find_chroma_spikes2();
        void            find_chroma_spike_cols3b(int, int);
	void            find_chroma_spike_rows3b(int, int);
	void            find_chroma_spike_cols4();	

	void            decodeStegMessage3();
	void            decodeStegMessage4();
	void            processMessageStrings();
	
	void            getAnchorStep();
	void            stepThrough();
	void            getColAnchorStep();
	void            getRowAnchorStep();
	void            stepThroughCols();
	void            stepThroughRows();
	////////////////////////////////////////////////////////////////////
	
	////////////////////////////////////////////////////////////////////
	typedef struct
	{
	  char str[16];
	  int len;
	  int times;
	} strStruct;
	////////////////////////////////////////////////////////////////////
	
        ////////////////////////////////////////////////////////////////////
	typedef struct
	{
	  int col;
	  int spikeCount;
	} flipAnchorColSpikeStruct;
	
	void sort_fa_by_count(int);
	
	typedef struct
	{
	  int row;
	  int spikeCount;
	} flipAnchorColSpikeStruct2;
	
	void sort_fa_by_count2(int);	
	////////////////////////////////////////////////////////////////////
	
	////////////////////////////////////////////////////////////////////
	typedef struct
	{
	  int  col;
	  int  blockWidth;
	  int  spikeUpCount;
	  int  spikeDownCount;
	  long spikeTotalCount;
	  int  spikeUpAverageHeight;
	  int  spikeDownAverageHeight;
	} spikeBlock;

	typedef struct
	{
	  int  row;
	  int  blockWidth;
	  int  spikeUpCount;
	  int  spikeDownCount;
	  long spikeTotalCount;
	  int  spikeUpAverageWidth;
	  int  spikeDownAverageWidth;
	} spikeBlock2;	
	
	void populateSpikeBlockArray(int);
        void getAnchorSpikeBlocks();
        void getMaxCountNonOverlappingSpikeBlocks();
	
	void populateSpikeBlockArray2(int);
        void getAnchorSpikeBlocks2();
        void getMaxCountNonOverlappingSpikeBlocks2();	
	////////////////////////////////////////////////////////////////////
	

#ifdef __cplusplus
}
#endif
#endif
