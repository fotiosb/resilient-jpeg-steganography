/*
 * jpeg.c
 * Copyright (C) Micheal Smith 2010 <xulfer at cheapbsd.net>
 * Copyright (C) Fotios Basagiannis 2011 <fotios at fotios.org>
 *
 * jpeg.c is free software copyrighted by Micheal Smith.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include <gd.h>

#include "jpeg-v4/jinclude.h"
#include "jsteg.h"
#include "jpeg.h"

static jmp_buf                   setjmp_buffer;
static FILE                      *debug_file;
static external_methods_ptr      emethods;
static struct image_data         *img_data = NULL;
static long                      row_idx;
static char                      *watermark = NULL;
static char                      *Steg_Encode_Message = NULL;

//Fotios
short                            watermark_font_size;
short                            Steg_Encode_Old_Overlay_On;
short                            Y_BUMP; //percent
short                            Y_BUMP_REACTIVE;
short                            Watermark_X_Step;
short                            Watermark_Y_Step;

short 				 Steg_Encode_Anti_Crop_On;
short 				 Steg_Anti_Crop_Percent;

short                            Steg_Encode_Col_Chroma_Shift_On;
short                            Steg_Encode_Col_Chroma_Shift;
short                            Steg_Encode_Col_Chroma_Shift_Cb_On;
short                            Steg_Encode_Col_Chroma_Shift_Cr_On;
short                            Steg_Encode_Col_Chroma_Shift_Down;

static short                     Steg_Set_Decode;

short                            Cb_Shift;
short                            Cr_Shift;
short                            Steg_Encode_Col_Step_factor;	
	
short                            Steg_Decode_Specific_Diff;

short                            Steg_Decode_Bound;

short                            Steg_Decode_Sensitivity;

short                            Steg_Decode_Anti_Crop_On;

short                            Steg_Decode_No_Crop_Hint;

short                            Steg_Decode_Ignore_Y;

short                            Steg_Decode_Y_Bound;

//////////
//Compress_info_struct             my_cinfo_struct;
//compress_info_ptr                my_cinfo = &my_cinfo_struct;
my_compress_info_struct            my_cinfo_struct;
my_compress_info_struct*           my_cinfo = &my_cinfo_struct;

//int**                            spike;

//int                              Steg_Encode_Col_Step;

//char                           Steg_Message[1024] = {'\0'};
char*                            Steg_Message = NULL;
//////////

char*                            bitS = NULL;

/*
 * Static function declarations
 */
static int      image_to_ycc(struct image_data *);
static int      image_to_rgb(struct image_data *);


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void find_chroma_spike_cols3(int step_factor)
{
  long  width  = my_cinfo->image_width;
  long  height = my_cinfo->image_height;
  
  long  midHeight = height / 2;
  
  float step   = (float)width / (float)step_factor;
  
  bitS        = (char*) malloc(128);
  *bitS       = '\0';
  char* b1    = bitS;
  char* b2    = bitS + 28;
  
  /*
  FILE *ivalFile1 = fopen("/mnt/www/fotios/domino.bz/steg/steg/dimensions.txt", "w");
  fprintf(ivalFile1, "image width is %ld and height is %ld\n", width, height);
  fclose(ivalFile1);
  */
      
  int row = 0;
  int col = 0;
  
  int topSpikeUpPixelCounter;
  int topSpikeDownPixelCounter;
  int bottomSpikeUpPixelCounter;
  int bottomSpikeDownPixelCounter;  
  
  //////////////////////////////////////////////
  
  int flag = 1;
  
  int i = 1;
  
  while (col < width)
  {
    topSpikeUpPixelCounter      = 0;
    topSpikeDownPixelCounter    = 0;
    bottomSpikeUpPixelCounter   = 0;
    bottomSpikeDownPixelCounter = 0; 
    
    col = (int)round((step * (float)i++));
    
    if (flag)
    {
      for(row = 0; row < midHeight; row++)
      {
	/*
	if (spike[row][col-3] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col-3] == 2)
	  topSpikeDownPixelCounter++;	
	*/
        
	if (spike[row][col-2] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col-2] == 2)
	  topSpikeDownPixelCounter++;
        
	if (spike[row][col-1] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col-1] == 2)
	  topSpikeDownPixelCounter++;
	
	if (spike[row][col] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col] == 2)
	  topSpikeDownPixelCounter++;
	
	if (spike[row][col+1] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col+1] == 2)
	  topSpikeDownPixelCounter++;	
        
	if (spike[row][col+2] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col+2] == 2)
	  topSpikeDownPixelCounter++;
	
        /*
	if (spike[row][col+3] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col+3] == 2)
	  topSpikeDownPixelCounter++;
	*/
      }

      if (topSpikeUpPixelCounter > topSpikeDownPixelCounter) //spike up
      {
	  *b1++ = '1';     
      }
      else if (topSpikeDownPixelCounter > topSpikeUpPixelCounter)
      {
	  *b1++ = '0';
      }
      
      
      for(; row < height; row++)
      {
	/*
	if (spike[row][col-3] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col-3] == 2)
	  bottomSpikeDownPixelCounter++;	
	*/
        
	if (spike[row][col-2] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col-2] == 2)
	  bottomSpikeDownPixelCounter++;
        
	if (spike[row][col-1] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col-1] == 2)
	  bottomSpikeDownPixelCounter++;
	
	if (spike[row][col] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col] == 2)
	  bottomSpikeDownPixelCounter++;
	
	if (spike[row][col+1] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col+1] == 2)
	  bottomSpikeDownPixelCounter++;	
        
	if (spike[row][col+2] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col+2] == 2)
	  bottomSpikeDownPixelCounter++;
	
        /*
	if (spike[row][col+3] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col+3] == 2)
	  bottomSpikeDownPixelCounter++;
	*/
      }

      if (bottomSpikeUpPixelCounter > bottomSpikeDownPixelCounter) //spike up
      {
	  *b2++ = '1';     
      }
      else if (bottomSpikeDownPixelCounter > bottomSpikeUpPixelCounter)
      {
	  *b2++ = '0';
      }      
    
      flag = 0;
    }
    else
      flag = 1;
    
  }//while
  
  *b2 = '\0';
  
  /*
  FILE* ivalFile2 = fopen("/mnt/www/fotios/domino.bz/steg/steg/bitS.txt", "w");
  fprintf(ivalFile2, "%s\n", bitS);
  fclose(ivalFile2); 
  */
}

void find_chroma_spike_rows3(int step_factor)
{
  long  width  = my_cinfo->image_width;
  long  height = my_cinfo->image_height;
  
  long  midWidth = width / 2;
  
  float step   = (float)height / (float)step_factor;
  
  bitS        = (char*) malloc(128);
  *bitS       = '\0';
  char* b1    = bitS;
  char* b2    = bitS + 28;
  
  /*
  FILE *ivalFile1 = fopen("/mnt/www/fotios/domino.bz/steg/steg/dimensions.txt", "w");
  fprintf(ivalFile1, "image width is %ld and height is %ld\n", width, height);
  fclose(ivalFile1);
  */
      
  int row = 0;
  int col = 0;
  
  int topSpikeUpPixelCounter;
  int topSpikeDownPixelCounter;
  int bottomSpikeUpPixelCounter;
  int bottomSpikeDownPixelCounter;  
  
  //////////////////////////////////////////////
  
  int flag = 1;
  
  int i = 1;
  
  while (row < height)
  {
    topSpikeUpPixelCounter      = 0;
    topSpikeDownPixelCounter    = 0;
    bottomSpikeUpPixelCounter   = 0;
    bottomSpikeDownPixelCounter = 0; 
    
    row = (int)round((step * (float)i++));
    
    if (flag)
    {
      for(col = 0; col < midWidth; col++)
      {
	/*
	if (spike[row][col-3] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col-3] == 2)
	  topSpikeDownPixelCounter++;	
	*/
        
	if (spike[row-2][col] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row-2][col] == 2)
	  topSpikeDownPixelCounter++;
        
	if (spike[row-1][col] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row-1][col] == 2)
	  topSpikeDownPixelCounter++;
	
	if (spike[row][col] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col] == 2)
	  topSpikeDownPixelCounter++;
	
	if (spike[row+1][col] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row+1][col] == 2)
	  topSpikeDownPixelCounter++;	
        
	if (spike[row+2][col] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row+2][col] == 2)
	  topSpikeDownPixelCounter++;
	
        /*
	if (spike[row][col+3] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col+3] == 2)
	  topSpikeDownPixelCounter++;
	*/
      }

      if (topSpikeUpPixelCounter > topSpikeDownPixelCounter) //spike up
      {
	  *b1++ = '1';     
      }
      else if (topSpikeDownPixelCounter > topSpikeUpPixelCounter)
      {
	  *b1++ = '0';
      }
      
      
      for(; col < width; col++)
      {
	/*
	if (spike[row][col-3] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col-3] == 2)
	  bottomSpikeDownPixelCounter++;	
	*/
        
	if (spike[row-2][col] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row-2][col] == 2)
	  bottomSpikeDownPixelCounter++;
        
	if (spike[row-1][col] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row-1][col] == 2)
	  bottomSpikeDownPixelCounter++;
	
	if (spike[row][col] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col] == 2)
	  bottomSpikeDownPixelCounter++;
	
	if (spike[row+1][col] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row+1][col] == 2)
	  bottomSpikeDownPixelCounter++;	
        
	if (spike[row+2][col] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row+2][col] == 2)
	  bottomSpikeDownPixelCounter++;
	
        /*
	if (spike[row][col+3] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col+3] == 2)
	  bottomSpikeDownPixelCounter++;
	*/
      }

      if (bottomSpikeUpPixelCounter > bottomSpikeDownPixelCounter) //spike up
      {
	  *b2++ = '1';     
      }
      else if (bottomSpikeDownPixelCounter > bottomSpikeUpPixelCounter)
      {
	  *b2++ = '0';
      }      
    
      flag = 0;
    }
    else
      flag = 1;
    
  }//while
  
  *b2 = '\0';
  
  /*
  FILE* ivalFile2 = fopen("/mnt/www/fotios/domino.bz/steg/steg/bitS.txt", "w");
  fprintf(ivalFile2, "%s\n", bitS);
  fclose(ivalFile2); 
  */
}

///////////////////////////////////////////////////////////////////////////////////////////

void find_chroma_spikes(int step_factor, int anti_crop_percent)
{
  long  width  = my_cinfo->image_width;
  long  height = my_cinfo->image_height;
  
  if (width >= height)
    find_chroma_spike_cols3b(step_factor, anti_crop_percent);
  else
    find_chroma_spike_rows3b(step_factor, anti_crop_percent); 
}

void find_chroma_spikes2()
{
  long  width  = my_cinfo->image_width;
  long  height = my_cinfo->image_height;
  
  if (width >= height)
    find_chroma_spike_cols3(56);
  else
    find_chroma_spike_rows3(56); 
}


///////////////////////////////////////////////////////////////////////////////////////////

void find_chroma_spike_cols3b(int step_factor, int anti_crop_percent)
{
  long  width  = my_cinfo->image_width;
  long  height = my_cinfo->image_height;
  
  long  midHeight = height / 2;
  
  
  bitS        = (char*) malloc(128);
  *bitS       = '\0';
  char* b1    = bitS;
  char* b2    = bitS + 28;
  
  float frame_width  = (float)( (float)width * (float)anti_crop_percent / 100.00 );
  float step         = ((float)width - 2.0 * frame_width) / (float)step_factor;   
  int   first_col    = (int)round(frame_width + 2.0 * step);
  int   last_col     = width - (int)round(frame_width);
  
  /*
  FILE *ivalFile1 = fopen("/mnt/www/fotios/domino.bz/steg/steg/dimensions.txt", "w");
  fprintf(ivalFile1, "image width is %ld and height is %ld\n", width, height);
  fclose(ivalFile1);
  */
      
  int row = 0;
  int col = first_col;
  
  int topSpikeUpPixelCounter;
  int topSpikeDownPixelCounter;
  int bottomSpikeUpPixelCounter;
  int bottomSpikeDownPixelCounter;  
  
  //////////////////////////////////////////////
  
  int flag = 1;
  
  int i = 1;
  
  while (col < last_col)
  {
    topSpikeUpPixelCounter      = 0;
    topSpikeDownPixelCounter    = 0;
    bottomSpikeUpPixelCounter   = 0;
    bottomSpikeDownPixelCounter = 0; 
    
    col = first_col + (int)round((step * (float)i++));
    
    if (flag)
    {
      for(row = 0; row < midHeight; row++)
      {
	/*
	if (spike[row][col-3] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col-3] == 2)
	  topSpikeDownPixelCounter++;	
	*/
        
	if (spike[row][col-2] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col-2] == 2)
	  topSpikeDownPixelCounter++;
        
	if (spike[row][col-1] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col-1] == 2)
	  topSpikeDownPixelCounter++;
	
	if (spike[row][col] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col] == 2)
	  topSpikeDownPixelCounter++;
	
	if (spike[row][col+1] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col+1] == 2)
	  topSpikeDownPixelCounter++;	
        
	if (spike[row][col+2] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col+2] == 2)
	  topSpikeDownPixelCounter++;
	
        /*
	if (spike[row][col+3] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col+3] == 2)
	  topSpikeDownPixelCounter++;
	*/
      }

      if (topSpikeUpPixelCounter > topSpikeDownPixelCounter) //spike up
      {
	  *b1++ = '1';     
      }
      else if (topSpikeDownPixelCounter > topSpikeUpPixelCounter)
      {
	  *b1++ = '0';
      }
      
      
      for(; row < height; row++)
      {
	/*
	if (spike[row][col-3] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col-3] == 2)
	  bottomSpikeDownPixelCounter++;	
	*/
        
	if (spike[row][col-2] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col-2] == 2)
	  bottomSpikeDownPixelCounter++;
        
	if (spike[row][col-1] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col-1] == 2)
	  bottomSpikeDownPixelCounter++;
	
	if (spike[row][col] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col] == 2)
	  bottomSpikeDownPixelCounter++;
	
	if (spike[row][col+1] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col+1] == 2)
	  bottomSpikeDownPixelCounter++;	
        
	if (spike[row][col+2] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col+2] == 2)
	  bottomSpikeDownPixelCounter++;
	
        /*
	if (spike[row][col+3] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col+3] == 2)
	  bottomSpikeDownPixelCounter++;
	*/
      }

      if (bottomSpikeUpPixelCounter > bottomSpikeDownPixelCounter) //spike up
      {
	  *b2++ = '1';     
      }
      else if (bottomSpikeDownPixelCounter > bottomSpikeUpPixelCounter)
      {
	  *b2++ = '0';
      }      
    
      flag = 0;
    }
    else
      flag = 1;
    
  }//while
  
  *b2 = '\0';
  
  /*
  FILE* ivalFile2 = fopen("/mnt/www/fotios/domino.bz/steg/steg/bitS.txt", "w");
  fprintf(ivalFile2, "%s\n", bitS);
  fclose(ivalFile2); 
  */
}

///////////////////////////////////////////////////////////////////////////////

void find_chroma_spike_rows3b(int step_factor, int anti_crop_percent)
{
  long  width  = my_cinfo->image_width;
  long  height = my_cinfo->image_height;
  
  long  midWidth = width / 2;
  
  
  bitS        = (char*) malloc(128);
  *bitS       = '\0';
  char* b1    = bitS;
  char* b2    = bitS + 28;
  
  float frame_width  = (float)( (float)height * (float)anti_crop_percent / 100.00 );
  float step         = ((float)height - 2.0 * frame_width) / (float)step_factor;   
  int   first_row    = (int)round(frame_width + 2.0 * step);
  int   last_row     = height - (int)round(frame_width);
  
  /*
  FILE *ivalFile1 = fopen("/mnt/www/fotios/domino.bz/steg/steg/dimensions.txt", "w");
  fprintf(ivalFile1, "image width is %ld and height is %ld\n", width, height);
  fclose(ivalFile1);
  */
      
  int row = first_row;
  int col = 0;
  
  int topSpikeUpPixelCounter;
  int topSpikeDownPixelCounter;
  int bottomSpikeUpPixelCounter;
  int bottomSpikeDownPixelCounter;  
  
  //////////////////////////////////////////////
  
  int flag = 1;
  
  int i = 1;
  
  while (row < last_row)
  {
    topSpikeUpPixelCounter      = 0;
    topSpikeDownPixelCounter    = 0;
    bottomSpikeUpPixelCounter   = 0;
    bottomSpikeDownPixelCounter = 0; 
    
    row = first_row + (int)round((step * (float)i++));
    
    if (flag)
    {
      for(col = 0; col < midWidth; col++)
      {
	/*
	if (spike[row][col-3] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col-3] == 2)
	  topSpikeDownPixelCounter++;	
	*/
        
	if (spike[row-2][col] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row-2][col] == 2)
	  topSpikeDownPixelCounter++;
        
	if (spike[row-1][col] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row-1][col] == 2)
	  topSpikeDownPixelCounter++;
	
	if (spike[row][col] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col] == 2)
	  topSpikeDownPixelCounter++;
	
	if (spike[row+1][col] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row+1][col] == 2)
	  topSpikeDownPixelCounter++;	
        
	if (spike[row+2][col] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row+2][col] == 2)
	  topSpikeDownPixelCounter++;
	
        /*
	if (spike[row][col+3] == 1)
	  topSpikeUpPixelCounter++;
	else if (spike[row][col+3] == 2)
	  topSpikeDownPixelCounter++;
	*/
      }

      if (topSpikeUpPixelCounter > topSpikeDownPixelCounter) //spike up
      {
	  *b1++ = '1';     
      }
      else if (topSpikeDownPixelCounter > topSpikeUpPixelCounter)
      {
	  *b1++ = '0';
      }
      
      
      for(; col < width; col++)
      {
	/*
	if (spike[row][col-3] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col-3] == 2)
	  bottomSpikeDownPixelCounter++;	
	*/
        
	if (spike[row-2][col] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row-2][col] == 2)
	  bottomSpikeDownPixelCounter++;
        
	if (spike[row-1][col] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row-1][col] == 2)
	  bottomSpikeDownPixelCounter++;
	
	if (spike[row][col] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col] == 2)
	  bottomSpikeDownPixelCounter++;
	
	if (spike[row+1][col] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row+1][col] == 2)
	  bottomSpikeDownPixelCounter++;	
        
	if (spike[row+2][col] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row+2][col] == 2)
	  bottomSpikeDownPixelCounter++;
	
        /*
	if (spike[row][col+3] == 1)
	  bottomSpikeUpPixelCounter++;
	else if (spike[row][col+3] == 2)
	  bottomSpikeDownPixelCounter++;
	*/
      }

      if (bottomSpikeUpPixelCounter > bottomSpikeDownPixelCounter) //spike up
      {
	  *b2++ = '1';     
      }
      else if (bottomSpikeDownPixelCounter > bottomSpikeUpPixelCounter)
      {
	  *b2++ = '0';
      }      
    
      flag = 0;
    }
    else
      flag = 1;
    
  }//while
  
  *b2 = '\0';
  
  /*
  FILE* ivalFile2 = fopen("/mnt/www/fotios/domino.bz/steg/steg/bitS.txt", "w");
  fprintf(ivalFile2, "%s\n", bitS);
  fclose(ivalFile2); 
  */
}
//////////////////////////////////////////////////////////////////////////////////////////

static flipAnchorColSpikeStruct fa[25];
static flipAnchorColSpikeStruct2 fa2[25];

static char* bitS2 = NULL;

void find_chroma_spike_cols4()
{
  long  width  = my_cinfo->image_width;
  long  height = my_cinfo->image_height;
  
  //////////////////////////////////////////
      
  int row = 0;
  int col = 0;
  
  int topSpikeUpPixelCounter;
  int topSpikeDownPixelCounter;
  int bottomSpikeUpPixelCounter;
  int bottomSpikeDownPixelCounter; 
  
  int spikeUpPixelCounter      = 0;
  int spikeDownPixelCounter    = 0;
  int spikeUpPixelCounterMax   = 0;
  int spikeDownPixelCounterMax = 0;
  
  long spikeUpPixelCounterTotal   = 0L;
  long spikeDownPixelCounterTotal = 0L;
  
  int spikeUpPixelCounterAverage   = 0;
  int spikeDownPixelCounterAverage = 0;
  
  int  spikeCount          = 0;
  int  spikeCountMax       = 0;
  long spikeCountTotal     = 0L;
  int  spikeCountAverage   = 0;
  int  spikeCountThreshold = 0;
  
  //int anchorCol[100]; 
  //int j = 0;
  
  int anchorCol1 = 0;
  int anchorCol2 = 0;
  
  ///////////////////////////////////////////////
  
  //first count shift ups and shift downs for each column and determine average of both
  for (col = 0; col < width; col++)
  {
    spikeUpPixelCounter   = 0;
    spikeDownPixelCounter = 0;
    
    //detect anchor spikes
    for (row = 0; row < height; row++)
    {
      if (spike[row][col] == 1) 
	spikeUpPixelCounter++;
      else if (spike[row][col] == 2)
	spikeDownPixelCounter++;
    }
    
    if (spikeUpPixelCounter > spikeUpPixelCounterMax)
      spikeUpPixelCounterMax = spikeUpPixelCounter;
    
    if (spikeDownPixelCounter > spikeDownPixelCounterMax)
      spikeDownPixelCounterMax = spikeDownPixelCounter;    
    
    spikeUpPixelCounterTotal   += spikeUpPixelCounter;
    spikeDownPixelCounterTotal += spikeDownPixelCounter;
    
    spikeCount       = spikeUpPixelCounter + spikeDownPixelCounter;
    spikeCountTotal += spikeCount;
    
    if (spikeCount > spikeCountMax)
      spikeCountMax = spikeCount;
  }
  
  spikeUpPixelCounterAverage   = spikeUpPixelCounterTotal   / width;
  spikeDownPixelCounterAverage = spikeDownPixelCounterTotal / width;
  
  spikeCountAverage = spikeCountTotal / width;
  
  float divFactor = 5.0;
  
  if (height >= 1200)
    divFactor = 3.0;
  else if (height >= 1100)
    divFactor = 3.0;
  else if (height >= 1000)
    divFactor = 5.0;
  else if (height >= 900)
    divFactor = 5.0;
  else if (height >= 800)
    divFactor = 5.0;
  else if (height >= 700)
    divFactor = 5.0;
  else if (height >= 600)
    divFactor = 6.0;
  else if (height >= 500)
    divFactor = 7.0;
  else if (height >= 400)
    divFactor = 8.0;  
  else
    divFactor = 9.0;    
  
  spikeCountThreshold = spikeCountAverage + (int)round((float)(spikeCountMax - spikeCountAverage) / divFactor);
  
  //FILE *ivalFile0 = fopen("/TEST/findCol.txt", "w");
  
  //First find the first col with a count above threshold and
  //up-count and down-count no less than 30% of the total count and
  //next two cols the same
  for (col = 0; col < width; col++)
  {
    int spikeUpPixelCounter0   = 0;
    int spikeDownPixelCounter0 = 0;    
    int spikeCount0            = 0;
    
    int spikeUpPixelCounter1   = 0;
    int spikeDownPixelCounter1 = 0;        
    int spikeCount1            = 0;

    int spikeUpPixelCounter2   = 0;
    int spikeDownPixelCounter2 = 0;        
    int spikeCount2            = 0;    
    
    for (row = 0; row < height; row++)
    {
      if (spike[row][col] == 1)
	spikeUpPixelCounter0++;
      else if (spike[row][col] == 2)
	spikeDownPixelCounter0++;
      
      if (spike[row][col+1] == 1)
	spikeUpPixelCounter1++;
      else if (spike[row][col+1] == 2)
	spikeDownPixelCounter1++;
      
      if (spike[row][col+2] == 1)
	spikeUpPixelCounter1++;
      else if (spike[row][col+2] == 2)
	spikeDownPixelCounter1++;    
    }
    
    spikeCount0 = spikeUpPixelCounter0 + spikeDownPixelCounter0;
    spikeCount1 = spikeUpPixelCounter1 + spikeDownPixelCounter1;
    spikeCount2 = spikeUpPixelCounter2 + spikeDownPixelCounter2;
    
    //This is an anchor spike-count only if both up and down counts are high enough
    if ( spikeCount0            > spikeCountThreshold && 
         spikeUpPixelCounter0   > spikeCount0 / 4     && 
         spikeDownPixelCounter0 > spikeCount0 / 4     &&
         
         spikeCount1            > spikeCountThreshold  
         //spikeUpPixelCounter1   > spikeCount1 / 3     && 
         //spikeDownPixelCounter1 > spikeCount1 / 3     &&
         
         //spikeCount2            > spikeCountThreshold  
         //spikeUpPixelCounter2   > spikeCount2 / 3     && 
         //spikeDownPixelCounter2 > spikeCount2 / 3        
       )
    {
      //fprintf(ivalFile0, "Found first anchor col at %d\n", col);
      
      //then check counts this and 5 consecutive cols and record one with highest count
      for (int i = col; i < col + 6; i++)
      {
	spikeCount = 0;
	
	for(int j = 0; j < height; j++)
	{
	  if ( spike[j][i] == 1 || spike[j][i] == 2 )
	    spikeCount++;
	}
	
	fa[i-col].col = i;
	fa[i-col].spikeCount = spikeCount;
      }
      
      sort_fa_by_count(6);
      anchorCol1 = fa[0].col;
      
      break;
    }
  }
  
  //fprintf(ivalFile0, "anchorCol1 is %d\n", anchorCol1);
  
  //Now find col with highest shift count between 7 and 20 cols away from anchorCol1
  for (int i = anchorCol1 + 7; i < anchorCol1 + 20; i++)
  {
    spikeCount = 0;
    
    for(int j = 0; j < height; j++)
    {
      if ( spike[j][i] == 1 || spike[j][i] == 2 )
	spikeCount++;   
    }
    
    fa[i-anchorCol1-7].col = i;
    fa[i-anchorCol1-7].spikeCount = spikeCount;
  }
  
  sort_fa_by_count(13);
  anchorCol2 = fa[0].col;
  
  //fprintf(ivalFile0, "anchorCol2 is %d\n", anchorCol2);
  
  //fclose(ivalFile0);
  
  
  //////////////////////////////////////////////
  //then using startCol and calculated colStep implement functionality as in find_chroma_spike_cols3
  
  bitS        = (char*) malloc(4096);
  *bitS       = '\0';
  char* b1    = bitS;
  char* b2    = bitS + 28;
  
  bitS2      = (char*) malloc(65536);
  *bitS2     = '\0';  
  
  //Perform many possible decodings using the range colStep-5 to colStep+5 in 0.1 or 0.2 or 0.5 increments
  
  float colStep = (float)(anchorCol2 - anchorCol1);
  int startCol = 0;
  
  long  bandHeight    = (long)((float)(4.0 / 9.0) * (float)height);
  float fuzzyFactor   = 5.0;
  float stepIncrement = 0.01;
  
  //FILE* ivalFile2 = fopen("/TEST/bitS.txt", "w");  
  //FILE* ivalFile3 = fopen("/TEST/colBits.txt", "w");
  
  for (startCol = anchorCol1 - fuzzyFactor; startCol < anchorCol1 + fuzzyFactor; startCol++)
  {
    long iRoundColStep  = 0L;
    float tColStep      = 0.0;
    
    while (tColStep < colStep + fuzzyFactor)
    {
      tColStep = colStep - fuzzyFactor + (float)(iRoundColStep++) * stepIncrement;
      
      b1    = bitS;
      b2    = bitS + 28;
      
      col = 0;
      row = 0;
      
      int flag = 1;
      
      int i = 2; //2 colSteps after anchorBar's first col
      
      long bitCount = 0L;
      
      //while (col < width && bitCount < 56)
      while (1)
      {
	topSpikeUpPixelCounter      = 0;
	topSpikeDownPixelCounter    = 0;
	bottomSpikeUpPixelCounter   = 0;
	bottomSpikeDownPixelCounter = 0; 
	
	col = startCol + (int)round( tColStep * (float)(i++) );
	
	//if (col > width - startCol - (int)round(tColStep) )
	if (col > width - startCol)
	{
	  //*b2 = '\0';
	  break;
	}
	
	if (flag)
	{
	  for(row = 0; row < bandHeight; row++)
	  {
	    /*
	    if (spike[row][col-3] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col-3] == 2)
	      topSpikeDownPixelCounter++;	
	    */
	    
	    if (spike[row][col-2] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col-2] == 2)
	      topSpikeDownPixelCounter++;
	    
	    if (spike[row][col-1] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col-1] == 2)
	      topSpikeDownPixelCounter++;
	    
	    if (spike[row][col] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col] == 2)
	      topSpikeDownPixelCounter++;
	    
	    if (spike[row][col+1] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col+1] == 2)
	      topSpikeDownPixelCounter++;
	    
	    if (spike[row][col+2] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col+2] == 2)
	      topSpikeDownPixelCounter++;
	    
	    /*
	    if (spike[row][col+3] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col+3] == 2)
	      topSpikeDownPixelCounter++;
	    */
	  }

	  if (topSpikeUpPixelCounter >= topSpikeDownPixelCounter) //spike up
	  {
	      *b1++ = '1'; 
	  }
	  else if (topSpikeDownPixelCounter > topSpikeUpPixelCounter)
	  {
	      *b1++ = '0';
	  }
	  
	  bitCount++;
	  
	  //fprintf(ivalFile3, "%d: col %d top bit is %c\n", bitCount, col, *(b1-1));
	  
	  for(row = height - bandHeight; row < height; row++)
	  {
	    /*
	    if (spike[row][col-3] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col-3] == 2)
	      bottomSpikeDownPixelCounter++;	
	    */
	    
	    if (spike[row][col-2] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col-2] == 2)
	      bottomSpikeDownPixelCounter++;
	    
	    if (spike[row][col-1] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col-1] == 2)
	      bottomSpikeDownPixelCounter++;
	    
	    if (spike[row][col] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col] == 2)
	      bottomSpikeDownPixelCounter++;
	    
	    if (spike[row][col+1] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col+1] == 2)
	      bottomSpikeDownPixelCounter++;	
	    
	    if (spike[row][col+2] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col+2] == 2)
	      bottomSpikeDownPixelCounter++;
	    
	    /*
	    if (spike[row][col+3] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col+3] == 2)
	      bottomSpikeDownPixelCounter++;
	    */
	  }
	  
	  if (bottomSpikeUpPixelCounter >= bottomSpikeDownPixelCounter) //spike up
	  {
	      *b2++ = '1';     
	  }
	  else if (bottomSpikeDownPixelCounter > bottomSpikeUpPixelCounter)
	  {
	      *b2++ = '0';
	  }
	  
	  bitCount++;
	  
	  //fprintf(ivalFile3, "%d: col %d bottom bit is %c\n", bitCount, col, *(b2-1));	
	
	  flag = 0;
	}
	else
	  flag = 1;
	
      }//while col
      
      *b2 = '\0';
      
      //Pack all 56-bit strings into bitS2
      //if (bitCount > 55 && bitCount < 57)
      if (bitCount == 56)
      {
	strcat(bitS2, bitS);  
	strcat(bitS2, "#");
      }
      
      //fprintf(ivalFile3, "--------\n");
      //fprintf(ivalFile2, "bitS(%d) = %s\n", strlen(bitS), bitS);
      
    }//while tColStep
  
  }//for startCol
  
  //fclose(ivalFile2);
  //fclose(ivalFile3);
  
  /*
  FILE* ivalFile4 = fopen("/TEST/bitS2.txt", "w");
  fprintf(ivalFile4, "%s\n", bitS2);
  fclose(ivalFile4);
  */
  
  free(bitS);
}

////////////////////////////////////////////////////////////////////////////////

//Converts a '#'-delimited bit-string into '#'-delimited 7-bit ASCII char strings
void decodeStegMessage4()
{  
  //Steg_Message = bitS;
  //return;
  
  Steg_Message  = (char*) malloc(16384);
  *Steg_Message = '\0';
  
  //strcat(Steg_Message, "<br>");

  //sprintf(Steg_Message, "bitS is %d chars long and is: %s", strlen(bitS), bitS);
  //free(bitS);
  //return;
  
  //FILE* ivalFile = fopen("/TEST/steg_message.txt", "w");
  
  //now convert 7-bit sequences to chars
  char* b = bitS2;
  //char* s = Steg_Message + 4;
  char* s = Steg_Message;
  
  //int i = 0;
  int flag = 1;
  //int charCount = 0;
  
  while (*b)
  { 
    char val = 0;
    short factor = 64;
    
    for (int i = 0; i < 7; i++)
    {
      if (*b == '#')
      {
	*s++ = *b++;
	
	/*
	*s = '<'; 
	*(s+1) = 'b';
	*(s+2) = 'r';
	*(s+3) = '>';
	s += 4;
	*/
	
	//b++;
	
	//fprintf(ivalFile, "\n");
	
	//flag = 1;
	break;
      }      
      else if (*b)
      {	
	if (*b++ == '1') 
	  val += factor;
	
	factor /= 2;
      }
      else
      {
	flag = 0;
	break;
      }
    }
    
    if (flag && *(b-1) != '#')
    {
      if (val > 32 && val < 127)
	*s++ = val;
      else
	*s++ = ' ';
      //fprintf(ivalFile, "%c", *(s-1));
    }
    //else
      //break;
      
    //Steg_Message[j++] = (char) val;  
  }
  
  *s = '\0';
  
  //fclose(ivalFile);
  
  //free(bitS);
  free(bitS2);
}


//Build statistically significant chars string, keep unique strings and 
//find most repeated unique string
void processMessageStrings()
{
  //strStruct* pss = (strStruct*) malloc(1024 * sizeof(strStruct));
  strStruct pss[1024];
  
  char statString1[9] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};
  char statString2[9] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};  
  
  char* s = Steg_Message;
  
  int charCount = 0;
  int i = 0;
  
  //FILE* ivalFile1 = fopen("/TEST/sscanf.txt", "w");
  
  //tokenize Steg_Message and put substrings in string struct array
  while ( sscanf(s, "%15[^#]%n", pss[i].str, &charCount) )
  {
    //fprintf(ivalFile1, "%s\n", pss[i].str);
    
    pss[i].len = charCount;
    pss[i++].times = 0;

    s += charCount; /* advance the pointer by the number of characters read */

    if (*s != '#') break; /* didn't find an expected delimiter */

    while (*s == '#') s++; //skip the delimiter or consecutive delimiters (empty strings)  
  }
  
  //fclose(ivalFile1);
  
  int arrMax = i;
  
  ////////////////////////////////////////////////////////////
  int ascCount[127];
  
  //Count the frequency of each char for string indexes 0-7
  for (int k = 0; k < 8; k++)
  {
    for (int j = 0; j < 127; j++)
    {
      ascCount[j] = 0;
    }    
    
    for (int j = 0; j < arrMax; j++)
    {
      if (pss[j].len >= 8)
	ascCount[(int)(pss[j].str[k])]++;
    }
    
    int maxPos = 0;
    int tmp = ascCount[0];
    
    for (int j = 1; j < 127; j++)
    {
      if (ascCount[j] > tmp)
      {
	tmp = ascCount[j];
	maxPos = j;
      }
    }
    
    statString1[k] = (char)maxPos;
  }
  //////////////////////////////////////////////////////////
  
  //keep unique strings only and increment their 'times' var  
  char tmp_str[16];
  //int rmStringCount = 0;
  
  for (int j = 0; j < arrMax; j++)
  {
    strcpy(tmp_str, pss[j].str);
    
    for (int k = j+1; k < arrMax; k++)
    {
      if ( !strcmp(tmp_str, pss[k].str) ) //if strings are the same
      {
	//rmStringCount++;
	strcpy(pss[k].str, "");
	pss[k].len = 0;
	pss[j].times++;
      }
    }
  }
  
  ///////////////////////////////////////////////////////////
  //Count the frequency of each char for string indexes 0-7
  for (int k = 0; k < 8; k++)
  {
    for (int j = 0; j < 127; j++)
    {
      ascCount[j] = 0;
    }    
    
    for (int j = 0; j < arrMax; j++)
    {
      if (pss[j].len >= 8)
	ascCount[(int)(pss[j].str[k])]++;
    }
    
    int maxPos = 0;
    int tmp = ascCount[0];
    
    for (int j = 1; j < 127; j++)
    {
      if (ascCount[j] > tmp)
      {
	tmp = ascCount[j];
	maxPos = j;
      }
    }
    
    statString2[k] = (char)maxPos;
  }
  ////////////////////////////////////////////////////////////
  
  //Now sort the struct array in descending order
  strStruct temp_ss;
  
  int flag = 1;
  
  while (flag)
  {
    flag = 0;
    
    for (int j = 0; j < arrMax - 1; j++)
    { 
      if (pss[j].times < pss[j+1].times)
      {
	temp_ss  = pss[j];
	pss[j]   = pss[j+1];
	pss[j+1] = temp_ss;
	
	flag    = 1;
      }
    }
  }
  
  /////////////////////////////////////////////////////////////////////////////////
  *Steg_Message = '\0';
  
  int flag2 = 1;
  
  //Now pack the sorted array into a Steg_Message skipping all zero-length strings
  for(int i = 0; i < arrMax; i++)
  {
    if (pss[i].len)
    {
      if (flag2)
      {
	flag2 = 0;
	strcat(Steg_Message, pss[i].str);
	strcat(Steg_Message, "<br>");
	
	//Add statString2 as second string
	strcat(Steg_Message, statString2);
	strcat(Steg_Message, "<br>");
	
	//Add statString1 as third string
	strcat(Steg_Message, statString1);
	strcat(Steg_Message, "<br><hr><br>");		
      }
      else
      {
	//Add all other unique strings here
	strcat(Steg_Message, pss[i].str);
	strcat(Steg_Message, "<br>");
      }
    }
  }
  
  //free(pss);
}


void sort_fa_by_count(int arrMax)
{
  flipAnchorColSpikeStruct temp_sb;
  
  int flag = 1;
  
  while (flag)
  {
    flag = 0;
    
    for (int i = 0; i < arrMax - 1; i++)
    { 
      if (fa[i].spikeCount < fa[i+1].spikeCount)
      {
	temp_sb = fa[i];
	fa[i]   = fa[i+1];
	fa[i+1] = temp_sb;
	
	flag    = 1;
      }
    }
  }
}


void sort_fa_by_count2(int arrMax)
{
  flipAnchorColSpikeStruct2 temp_sb;
  
  int flag = 1;
  
  while (flag)
  {
    flag = 0;
    
    for (int i = 0; i < arrMax - 1; i++)
    { 
      if (fa2[i].spikeCount < fa2[i+1].spikeCount)
      {
	temp_sb = fa2[i];
	fa2[i]   = fa2[i+1];
	fa2[i+1] = temp_sb;
	
	flag    = 1;
      }
    }
  }
}


void decodeStegMessage3()
{  
  //Steg_Message = bitS;
  //return;
  
  Steg_Message  = (char*) malloc(128); 
  *Steg_Message = '\0';

  //sprintf(Steg_Message, "bitS is %d chars long and is: %s", strlen(bitS), bitS);
  //free(bitS);
  //return;
  
  //now convert 7-bit sequences to chars
  char* b = bitS;
  char* s = Steg_Message;
  
  int i = 0;
  int flag = 1;
  
  while (*b)
  { 
    char val = 0;
    short factor = 64;
    
    for (i = 0; i < 7; i++)
    {
      if (*b)
      {
	if (*b++ == '1') val += factor;
	factor /= 2;
      }
      else
      {
	flag = 0;
	break;
      }
    }
    
    if (flag)
      *s++ = val;
    else
      break;
    //Steg_Message[j++] = (char) val;  
  }
  
  *s = '\0';
  
  /*
  FILE* ivalFile = fopen("/TEST/steg_message.txt", "w");
  fprintf(ivalFile, "%s\n", Steg_Message);
  fclose(ivalFile);
  */
  
  free(bitS);
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

static int         sbCount                   = 0; //holds number of elements in sb array
static int         asbCount                  = 0; //holds number of elements in nasb array
static int         blockSpikeCountThreshold  = 0;

static int         sbCount2                  = 0; //holds number of elements in sb array
static int         asbCount2                 = 0; //holds number of elements in nasb array
static int         blockSpikeCountThreshold2 = 0;

static spikeBlock* sb                        = NULL;
static spikeBlock* asb                       = NULL;
static spikeBlock* nosb                      = NULL;

static spikeBlock2* sb2                      = NULL;
static spikeBlock2* asb2                     = NULL;
static spikeBlock2* nosb2                    = NULL;

////////////////////////////////////////////////////////////////////////////////////////

void getAnchorStep()
{
  long  width  = my_cinfo->image_width;
  long  height = my_cinfo->image_height; 
  
  if (width >= height)
    getColAnchorStep();
  else
    getRowAnchorStep();
}

void stepThrough()
{
  long  width  = my_cinfo->image_width;
  long  height = my_cinfo->image_height; 
  
  if (width >= height)
    stepThroughCols();
  else
    stepThroughRows();  
}


static int anchorCol1 = 0;
static int anchorCol2 = 0;

void getColAnchorStep()
{
  long  width  = my_cinfo->image_width;
  long  height = my_cinfo->image_height; 
  
  int spikeCount = 0;
  
  populateSpikeBlockArray(5);
  getAnchorSpikeBlocks(); //to asb
  getMaxCountNonOverlappingSpikeBlocks(); //to nosb
  
  anchorCol1 = nosb[0].col - 1;
  //anchorCol2 = nosb[1].col - 1;
  
  //Now find col with highest shift count between 7 and 20 cols away from anchorCol1
  for (int i = anchorCol1 + 7; i < anchorCol1 + 20; i++)
  {
    spikeCount = 0;
    
    for(int j = 0; j < height; j++)
    {
      if ( spike[j][i] == 1 || spike[j][i] == 2 )
	spikeCount++;   
    }
    
    fa[i-anchorCol1-7].col = i;
    fa[i-anchorCol1-7].spikeCount = spikeCount;
  }
  
  sort_fa_by_count(13);
  anchorCol2 = fa[0].col;  
  
  //free stuff here
  free(sb); free(asb); free(nosb);
  
  //debug
  /*
  FILE* ivalFile = fopen("/TEST/step.txt", "w");
  fprintf(ivalFile, "width = %d, height = %d, anchorCol1 = %d, anhorCol2 = %d, colStep = %d\n", 
	             width, height, anchorCol1, anchorCol2, anchorCol2 - anchorCol1);
  fclose(ivalFile);
  */
}


static int anchorRow1 = 0;
static int anchorRow2 = 0;

void getRowAnchorStep()
{
  long  width  = my_cinfo->image_width;
  long  height = my_cinfo->image_height;  
  
  int spikeCount = 0;
  
  populateSpikeBlockArray2(5);
  getAnchorSpikeBlocks2(); //to asb2
  getMaxCountNonOverlappingSpikeBlocks2(); //to nosb2
  
  anchorRow1 = nosb2[0].row - 1;
  //anchorRow2 = nosb2[1].row - 1;
  
  //Now find row with highest shift count between 7 and 20 rows away from anchorRow1
  for (int i = anchorRow1 + 7; i < anchorRow1 + 20; i++)
  {
    spikeCount = 0;
    
    for(int j = 0; j < width; j++)
    {
      if ( spike[i][j] == 1 || spike[i][j] == 2 )
	spikeCount++;   
    }
    
    fa2[i-anchorRow1-7].row = i;
    fa2[i-anchorRow1-7].spikeCount = spikeCount;
  }
  
  sort_fa_by_count2(13);
  anchorRow2 = fa2[0].row;
  
  //free stuff here
  free(sb); free(asb); free(nosb);
  
  //debug
  /*
  FILE* ivalFile = fopen("/TEST/step.txt", "w");
  fprintf(ivalFile, "width = %d, height = %d, anchorCol1 = %d, anhorCol2 = %d, colStep = %d\n", 
	             width, height, anchorCol1, anchorCol2, anchorCol2 - anchorCol1);
  fclose(ivalFile);
  */
}

void stepThroughCols()
{
  long  width  = my_cinfo->image_width;
  long  height = my_cinfo->image_height;
  
  //////////////////////////////////////////
      
  int row = 0;
  int col = 0;
  
  int topSpikeUpPixelCounter;
  int topSpikeDownPixelCounter;
  int bottomSpikeUpPixelCounter;
  int bottomSpikeDownPixelCounter; 
  
  int spikeUpPixelCounter      = 0;
  int spikeDownPixelCounter    = 0;
  int spikeUpPixelCounterMax   = 0;
  int spikeDownPixelCounterMax = 0;
  
  long spikeUpPixelCounterTotal   = 0L;
  long spikeDownPixelCounterTotal = 0L;
  
  int spikeUpPixelCounterAverage   = 0;
  int spikeDownPixelCounterAverage = 0;
  
  int  spikeCount          = 0;
  int  spikeCountMax       = 0;
  long spikeCountTotal     = 0L;
  int  spikeCountAverage   = 0;
  int  spikeCountThreshold = 0;
  
  //////////////////////////////////////////////
  //then using startCol and calculated colStep implement functionality as in find_chroma_spike_cols3  
  bitS        = (char*) malloc(4096);
  *bitS       = '\0';
  char* b1    = bitS;
  char* b2    = bitS + 28;
  
  bitS2      = (char*) malloc(65536);
  *bitS2     = '\0';  
  
  //Perform many possible decodings using the range colStep-5 to colStep+5 in 0.1 or 0.2 or 0.5 increments
  
  float colStep = (float)(anchorCol2 - anchorCol1);
  int startCol = 0;
  
  /////////////////////////////////////////////////////////////////
  long  bandHeight    = (long)((float)(4.0 / 9.0) * (float)height); //4.0 / 9.0
  float fuzzyFactor   = 5.0; //5.0
  float stepIncrement = 0.1;  //0.01
  /////////////////////////////////////////////////////////////////
  
  //FILE* ivalFile2 = fopen("/TEST/bitS.txt", "w");  
  //FILE* ivalFile3 = fopen("/TEST/colBits.txt", "w");
  
  for (startCol = anchorCol1 - fuzzyFactor; startCol < anchorCol1 + fuzzyFactor; startCol++)
  {
    long iRoundColStep  = 0L;
    float tColStep      = 0.0;
    
    while (tColStep < colStep + fuzzyFactor)
    {
      tColStep = colStep - fuzzyFactor + (float)(iRoundColStep++) * stepIncrement;
      
      b1    = bitS;
      b2    = bitS + 28;
      
      col = 0;
      row = 0;
      
      int flag = 1;
      
      int i = 2; //2 colSteps after anchorBar's first col
      
      long bitCount = 0L;
      
      //while (col < width && bitCount < 56)
      while (1)
      {
	topSpikeUpPixelCounter      = 0;
	topSpikeDownPixelCounter    = 0;
	bottomSpikeUpPixelCounter   = 0;
	bottomSpikeDownPixelCounter = 0; 
	
	col = startCol + (int)round( tColStep * (float)(i++) );
	
	//if (col > width - startCol - (int)round(tColStep) )
	if (col > width - startCol)
	{
	  //*b2 = '\0';
	  break;
	}
	
	if (flag)
	{
	  for(row = 0; row < bandHeight; row++)
	  {
	    /*
	    if (spike[row][col-3] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col-3] == 2)
	      topSpikeDownPixelCounter++;	
	    */
	    
	    if (spike[row][col-2] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col-2] == 2)
	      topSpikeDownPixelCounter++;
	    
	    if (spike[row][col-1] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col-1] == 2)
	      topSpikeDownPixelCounter++;
	    
	    if (spike[row][col] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col] == 2)
	      topSpikeDownPixelCounter++;
	    
	    if (spike[row][col+1] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col+1] == 2)
	      topSpikeDownPixelCounter++;
	    
	    if (spike[row][col+2] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col+2] == 2)
	      topSpikeDownPixelCounter++;
	    
	    /*
	    if (spike[row][col+3] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col+3] == 2)
	      topSpikeDownPixelCounter++;
	    */
	  }

	  if (topSpikeUpPixelCounter >= topSpikeDownPixelCounter) //spike up
	  {
	      *b1++ = '1'; 
	  }
	  else if (topSpikeDownPixelCounter > topSpikeUpPixelCounter)
	  {
	      *b1++ = '0';
	  }
	  
	  bitCount++;
	  
	  //fprintf(ivalFile3, "%d: col %d top bit is %c\n", bitCount, col, *(b1-1));
	  
	  for(row = height - bandHeight; row < height; row++)
	  {
	    /*
	    if (spike[row][col-3] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col-3] == 2)
	      bottomSpikeDownPixelCounter++;	
	    */
	    
	    if (spike[row][col-2] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col-2] == 2)
	      bottomSpikeDownPixelCounter++;
	    
	    if (spike[row][col-1] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col-1] == 2)
	      bottomSpikeDownPixelCounter++;
	    
	    if (spike[row][col] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col] == 2)
	      bottomSpikeDownPixelCounter++;
	    
	    if (spike[row][col+1] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col+1] == 2)
	      bottomSpikeDownPixelCounter++;	
	    
	    if (spike[row][col+2] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col+2] == 2)
	      bottomSpikeDownPixelCounter++;
	    
	    /*
	    if (spike[row][col+3] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col+3] == 2)
	      bottomSpikeDownPixelCounter++;
	    */
	  }
	  
	  if (bottomSpikeUpPixelCounter >= bottomSpikeDownPixelCounter) //spike up
	  {
	      *b2++ = '1';     
	  }
	  else if (bottomSpikeDownPixelCounter > bottomSpikeUpPixelCounter)
	  {
	      *b2++ = '0';
	  }
	  
	  bitCount++;
	  
	  //fprintf(ivalFile3, "%d: col %d bottom bit is %c\n", bitCount, col, *(b2-1));	
	
	  flag = 0;
	}
	else
	  flag = 1;
	
      }//while col
      
      *b2 = '\0';
      
      //if (bitCount > 55 && bitCount < 57)
      if (bitCount == 56)
      {
	strcat(bitS2, bitS);  
	strcat(bitS2, "#");
      }
      
      //fprintf(ivalFile3, "--------\n");
      //fprintf(ivalFile2, "bitS(%d) = %s\n", strlen(bitS), bitS);
      
    }//while tColStep
  
  }//for startCol
  
  //fclose(ivalFile2);
  //fclose(ivalFile3);
  
  /*
  FILE* ivalFile4 = fopen("/TEST/bitS2.txt", "w");
  fprintf(ivalFile4, "%s\n", bitS2);
  fclose(ivalFile4);
  */
  
  free(bitS);  
}


void stepThroughRows()
{
  long  width  = my_cinfo->image_width;
  long  height = my_cinfo->image_height;
  
  //////////////////////////////////////////
      
  int row = 0;
  int col = 0;
  
  int topSpikeUpPixelCounter;
  int topSpikeDownPixelCounter;
  int bottomSpikeUpPixelCounter;
  int bottomSpikeDownPixelCounter; 
  
  int spikeUpPixelCounter      = 0;
  int spikeDownPixelCounter    = 0;
  int spikeUpPixelCounterMax   = 0;
  int spikeDownPixelCounterMax = 0;
  
  long spikeUpPixelCounterTotal   = 0L;
  long spikeDownPixelCounterTotal = 0L;
  
  int spikeUpPixelCounterAverage   = 0;
  int spikeDownPixelCounterAverage = 0;
  
  int  spikeCount          = 0;
  int  spikeCountMax       = 0;
  long spikeCountTotal     = 0L;
  int  spikeCountAverage   = 0;
  int  spikeCountThreshold = 0;
  
  //////////////////////////////////////////////
  //then using startCol and calculated colStep implement functionality as in find_chroma_spike_cols3  
  bitS        = (char*) malloc(4096);
  *bitS       = '\0';
  char* b1    = bitS;
  char* b2    = bitS + 28;
  
  bitS2      = (char*) malloc(65536);
  *bitS2     = '\0';  
  
  //Perform many possible decodings using the range colStep-5 to colStep+5 in 0.1 or 0.2 or 0.5 increments
  
  float rowStep = (float)(anchorRow2 - anchorRow1);
  int startRow = 0;
  
  /////////////////////////////////////////////////////////////////
  long  bandWidth    = (long)((float)(4.0 / 9.0) * (float)width); //4.0 / 9.0
  float fuzzyFactor   = 5.0; //5.0
  float stepIncrement = 0.1;  //0.01
  /////////////////////////////////////////////////////////////////
  
  //FILE* ivalFile2 = fopen("/TEST/bitS.txt", "w");  
  //FILE* ivalFile3 = fopen("/TEST/colBits.txt", "w");
  
  for (startRow = anchorRow1 - fuzzyFactor; startRow < anchorRow1 + fuzzyFactor; startRow++)
  {
    long iRoundRowStep  = 0L;
    float tRowStep      = 0.0;
    
    while (tRowStep < rowStep + fuzzyFactor)
    {
      tRowStep = rowStep - fuzzyFactor + (float)(iRoundRowStep++) * stepIncrement;
      
      b1    = bitS;
      b2    = bitS + 28;
      
      col = 0;
      row = 0;
      
      int flag = 1;
      
      int i = 2; //2 colSteps after anchorBar's first col
      
      long bitCount = 0L;
      
      //while (col < width && bitCount < 56)
      while (1)
      {
	topSpikeUpPixelCounter      = 0;
	topSpikeDownPixelCounter    = 0;
	bottomSpikeUpPixelCounter   = 0;
	bottomSpikeDownPixelCounter = 0; 
	
	row = startRow + (int)round( tRowStep * (float)(i++) );
	
	//if (col > width - startCol - (int)round(tColStep) )
	if (row > height - startRow)
	{
	  //*b2 = '\0';
	  break;
	}
	
	if (flag)
	{
	  for(col = 0; col < bandWidth; col++)
	  {
	    /*
	    if (spike[row][col-3] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col-3] == 2)
	      topSpikeDownPixelCounter++;	
	    */
	    
	    if (spike[row-2][col] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row-2][col] == 2)
	      topSpikeDownPixelCounter++;
	    
	    if (spike[row-1][col] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row-1][col] == 2)
	      topSpikeDownPixelCounter++;
	    
	    if (spike[row][col] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col] == 2)
	      topSpikeDownPixelCounter++;
	    
	    if (spike[row+1][col] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row+1][col] == 2)
	      topSpikeDownPixelCounter++;
	    
	    if (spike[row+2][col] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row+2][col] == 2)
	      topSpikeDownPixelCounter++;
	    
	    /*
	    if (spike[row][col+3] == 1)
	      topSpikeUpPixelCounter++;
	    else if (spike[row][col+3] == 2)
	      topSpikeDownPixelCounter++;
	    */
	  }

	  if (topSpikeUpPixelCounter >= topSpikeDownPixelCounter) //spike up
	  {
	      *b1++ = '1'; 
	  }
	  else if (topSpikeDownPixelCounter > topSpikeUpPixelCounter)
	  {
	      *b1++ = '0';
	  }
	  
	  bitCount++;
	  
	  //fprintf(ivalFile3, "%d: col %d top bit is %c\n", bitCount, col, *(b1-1));
	  
	  for(col = width - bandWidth; col < width; col++)
	  {
	    /*
	    if (spike[row][col-3] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col-3] == 2)
	      bottomSpikeDownPixelCounter++;	
	    */
	    
	    if (spike[row-2][col] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row-2][col] == 2)
	      bottomSpikeDownPixelCounter++;
	    
	    if (spike[row-1][col] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row-1][col] == 2)
	      bottomSpikeDownPixelCounter++;
	    
	    if (spike[row][col] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col] == 2)
	      bottomSpikeDownPixelCounter++;
	    
	    if (spike[row+1][col] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row+1][col] == 2)
	      bottomSpikeDownPixelCounter++;	
	    
	    if (spike[row+2][col] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row+2][col] == 2)
	      bottomSpikeDownPixelCounter++;
	    
	    /*
	    if (spike[row][col+3] == 1)
	      bottomSpikeUpPixelCounter++;
	    else if (spike[row][col+3] == 2)
	      bottomSpikeDownPixelCounter++;
	    */
	  }
	  
	  if (bottomSpikeUpPixelCounter >= bottomSpikeDownPixelCounter) //spike up
	  {
	      *b2++ = '1';     
	  }
	  else if (bottomSpikeDownPixelCounter > bottomSpikeUpPixelCounter)
	  {
	      *b2++ = '0';
	  }
	  
	  bitCount++;
	  
	  //fprintf(ivalFile3, "%d: col %d bottom bit is %c\n", bitCount, col, *(b2-1));	
	
	  flag = 0;
	}
	else
	  flag = 1;
	
      }//while col
      
      *b2 = '\0';
      
      //if (bitCount > 55 && bitCount < 57)
      if (bitCount == 56)
      {
	strcat(bitS2, bitS);  
	strcat(bitS2, "#");
      }
      
      //fprintf(ivalFile3, "--------\n");
      //fprintf(ivalFile2, "bitS(%d) = %s\n", strlen(bitS), bitS);
      
    }//while tColStep
  
  }//for startCol
  
  //fclose(ivalFile2);
  //fclose(ivalFile3);
  
  /*
  FILE* ivalFile4 = fopen("/TEST/bitS2.txt", "w");
  fprintf(ivalFile4, "%s\n", bitS2);
  fclose(ivalFile4);
  */
  
  free(bitS);  
}

////////////////////////////////////////////////////////////////////////////////////////

void populateSpikeBlockArray(int blockWidth)
{
  long width  = my_cinfo->image_width;
  long height = my_cinfo->image_height;
  
  sbCount = 0;
  
  sb = (spikeBlock*) malloc(width * sizeof(spikeBlock));
  
  int   spikeUpCount           = 0;
  int   spikeDownCount         = 0;
  long  spikeTotalCount        = 0;
  
  long  allBlockSpikeTotal     = 0;
  int   allBlockSpikeAverage   = 0;
  int   allBlockSpikeMax       = 0;
  
  long  spikeUpTotalHeight     = 0;
  long  spikeDownTotalHeight   = 0;
  int   spikeUpAverageHeight   = 0;
  int   spikeDownAverageHeight = 0;
  
  int   spikedUpPixelCount     = 0;
  int   spikedDownPixelCount   = 0;  
  
  int col = blockWidth - 1;
  int k = 0;
  
  while (col < width)
  {
    spikeUpCount           = 0;
    spikeDownCount         = 0;
    spikeUpTotalHeight     = 0;
    spikeDownTotalHeight   = 0;
    spikedUpPixelCount     = 0;
    spikedDownPixelCount   = 0;
    spikeUpAverageHeight   = 0;
    spikeDownAverageHeight = 0;
    
    for (int i = 0; i < height; i++)
    {
      for (int j = 0; j < blockWidth; j++)
      {
	if (spike[i][col - j] == 1)
	{
	  spikeUpCount++;
	  spikeUpTotalHeight += i;
	  spikedUpPixelCount++;
	}
	else if (spike[i][col - j] == 2)
	{
	  spikeDownCount++;
	  spikeDownTotalHeight += i;
	  spikedDownPixelCount++;
	}
      }
    }
    
    spikeTotalCount = spikeUpCount + spikeDownCount;
    
    allBlockSpikeTotal += spikeTotalCount;
    
    if (allBlockSpikeMax < spikeTotalCount) 
      allBlockSpikeMax = spikeTotalCount;
    
    if (spikedUpPixelCount > 0) 
      spikeUpAverageHeight = spikeUpTotalHeight / spikedUpPixelCount;
    
    if (spikedDownPixelCount > 0)
      spikeDownAverageHeight = spikeDownTotalHeight / spikedDownPixelCount;   
     
    sb[k].col                    = col; 
    sb[k].blockWidth             = blockWidth;
    sb[k].spikeUpCount           = spikeUpCount;
    sb[k].spikeDownCount         = spikeDownCount;
    sb[k].spikeTotalCount        = spikeTotalCount;
    sb[k].spikeUpAverageHeight   = spikeUpAverageHeight;
    sb[k].spikeDownAverageHeight = spikeDownAverageHeight;    
    
    col++;
    k++;
    sbCount++;
  }
  
  allBlockSpikeAverage = allBlockSpikeTotal / sbCount;
  
  blockSpikeCountThreshold = allBlockSpikeAverage + (allBlockSpikeMax - allBlockSpikeAverage) / 3;
  
  //debug
  /*
  FILE *ivalFile1 = fopen("/TEST/blockSpikeThreshold.txt", "w");
  fprintf(ivalFile1, "allBlockSpikeAverage = %d, allBlockSpikeMax = %d, blockSpikeCountThreshold = %d\n", 
                     allBlockSpikeAverage, allBlockSpikeMax, blockSpikeCountThreshold
         );
  fclose(ivalFile1);
  
  FILE* ivalFile2 = fopen("/TEST/sb.txt", "w");
  for (int i = 0; i < sbCount; i++)
  {
    fprintf(ivalFile2, "col=%d, spikeTotalCount=%d, spikeUpCount=%d, spikeDownCount=%d, spikeUpAverageHeight=%d,\
	               spikeDownAverageHeight=%d\n",
		       sb[i].col, sb[i].spikeTotalCount, sb[i].spikeUpCount, sb[i].spikeDownCount,
		       sb[i].spikeUpAverageHeight, sb[i].spikeDownAverageHeight);
  }
  fclose(ivalFile2);  
  */
}

void populateSpikeBlockArray2(int blockWidth)
{
  long width  = my_cinfo->image_width;
  long height = my_cinfo->image_height;
  
  sbCount2 = 0;
  
  sb2 = (spikeBlock2*) malloc(height * sizeof(spikeBlock2));
  
  int   spikeUpCount           = 0;
  int   spikeDownCount         = 0;
  long  spikeTotalCount        = 0;
  
  long  allBlockSpikeTotal     = 0;
  int   allBlockSpikeAverage   = 0;
  int   allBlockSpikeMax       = 0;
  
  long  spikeUpTotalWidth      = 0;
  long  spikeDownTotalWidth    = 0;
  int   spikeUpAverageWidth    = 0;
  int   spikeDownAverageWidth  = 0;
  
  int   spikedUpPixelCount     = 0;
  int   spikedDownPixelCount   = 0;  
  
  int row = blockWidth - 1;
  int k = 0;
  
  while (row < height)
  {
    spikeUpCount           = 0;
    spikeDownCount         = 0;
    spikeUpTotalWidth      = 0;
    spikeDownTotalWidth    = 0;
    spikedUpPixelCount     = 0;
    spikedDownPixelCount   = 0;
    spikeUpAverageWidth    = 0;
    spikeDownAverageWidth  = 0;
    
    for (int i = 0; i < width; i++)
    {
      for (int j = 0; j < blockWidth; j++)
      {
	if (spike[row-j][i] == 1)
	{
	  spikeUpCount++;
	  spikeUpTotalWidth += i;
	  spikedUpPixelCount++;
	}
	else if (spike[row-j][i] == 2)
	{
	  spikeDownCount++;
	  spikeDownTotalWidth += i;
	  spikedDownPixelCount++;
	}
      }
    }
    
    spikeTotalCount = spikeUpCount + spikeDownCount;
    
    allBlockSpikeTotal += spikeTotalCount;
    
    if (allBlockSpikeMax < spikeTotalCount) 
      allBlockSpikeMax = spikeTotalCount;
    
    if (spikedUpPixelCount > 0) 
      spikeUpAverageWidth = spikeUpTotalWidth / spikedUpPixelCount;
    
    if (spikedDownPixelCount > 0)
      spikeDownAverageWidth = spikeDownTotalWidth / spikedDownPixelCount;
     
    sb2[k].row                    = row; 
    sb2[k].blockWidth             = blockWidth;
    sb2[k].spikeUpCount           = spikeUpCount;
    sb2[k].spikeDownCount         = spikeDownCount;
    sb2[k].spikeTotalCount        = spikeTotalCount;
    sb2[k].spikeUpAverageWidth    = spikeUpAverageWidth;
    sb2[k].spikeDownAverageWidth  = spikeDownAverageWidth;    
    
    row++;
    k++;
    sbCount2++;
  }
  
  allBlockSpikeAverage = allBlockSpikeTotal / sbCount2;
  
  blockSpikeCountThreshold2 = allBlockSpikeAverage + (allBlockSpikeMax - allBlockSpikeAverage) / 3;
  
  //debug
  /*
  FILE *ivalFile1 = fopen("/TEST/blockSpikeThreshold.txt", "w");
  fprintf(ivalFile1, "allBlockSpikeAverage = %d, allBlockSpikeMax = %d, blockSpikeCountThreshold = %d\n", 
                     allBlockSpikeAverage, allBlockSpikeMax, blockSpikeCountThreshold2
         );
  fclose(ivalFile1);
  
  FILE* ivalFile2 = fopen("/TEST/sb.txt", "w");
  for (int i = 0; i < sbCount2; i++)
  {
    fprintf(ivalFile2, "row=%d, spikeTotalCount=%d, spikeUpCount=%d, spikeDownCount=%d, spikeUpAverageWidth=%d,\
	               spikeDownAverageWidth=%d\n",
		       sb2[i].row, sb2[i].spikeTotalCount, sb2[i].spikeUpCount, sb2[i].spikeDownCount,
		       sb2[i].spikeUpAverageWidth, sb2[i].spikeDownAverageWidth);
  }
  fclose(ivalFile2);  
  */
}

void getAnchorSpikeBlocks()
{
  long width  = my_cinfo->image_width;
  long height = my_cinfo->image_height;
  
  int midHeight = height / 2;
  
  int heightThreshold = (int)round(1.5 * (float)height / 10.00);  
  
  asbCount = 0;
  
  asb = (spikeBlock*) malloc(sbCount * sizeof(spikeBlock));
  
  int j = 0;
  
  for (int i = 0; i < sbCount; i++)
  {
    if ( sb[i].spikeTotalCount > blockSpikeCountThreshold &&
         ( sb[i].spikeUpAverageHeight < midHeight + heightThreshold &&
           sb[i].spikeUpAverageHeight > midHeight - heightThreshold       ||
           sb[i].spikeDownAverageHeight < midHeight + heightThreshold &&  
           sb[i].spikeDownAverageHeight > midHeight - heightThreshold        ) &&
	 sb[i].spikeUpCount > (int)round(2.5 * (float)sb[i].spikeTotalCount / 10.00) &&
	 sb[i].spikeDownCount > (int)round(2.5 * (float)sb[i].spikeTotalCount / 10.00)
       )
    {
      asb[j++] = sb[i];
      asbCount++;
    }
  }
  
  //debug
  /*
  FILE* ivalFile = fopen("/TEST/asb.txt", "w");
  for (int i = 0; i < asbCount; i++)
  {
    fprintf(ivalFile, "col=%d, spikeTotalCount=%d, spikeUpCount=%d, spikeDownCount=%d, spikeUpAverageHeight=%d,\
	               spikeDownAverageHeight=%d\n",
		       asb[i].col, asb[i].spikeTotalCount, asb[i].spikeUpCount, asb[i].spikeDownCount,
		       asb[i].spikeUpAverageHeight, asb[i].spikeDownAverageHeight);
  }
  fclose(ivalFile);
  */
}

void getAnchorSpikeBlocks2()
{
  long width  = my_cinfo->image_width;
  long height = my_cinfo->image_height;
  
  int midWidth = width / 2;
  
  int widthThreshold = (int)round(1.5 * (float)width / 10.00);  
  
  asbCount2 = 0;
  
  asb2 = (spikeBlock2*) malloc(sbCount2 * sizeof(spikeBlock2));
  
  int j = 0;
  
  for (int i = 0; i < sbCount2; i++)
  {
    if ( sb2[i].spikeTotalCount > blockSpikeCountThreshold2 &&
         ( sb2[i].spikeUpAverageWidth < midWidth + widthThreshold &&
           sb2[i].spikeUpAverageWidth > midWidth - widthThreshold       ||
           sb2[i].spikeDownAverageWidth < midWidth + widthThreshold &&  
           sb2[i].spikeDownAverageWidth > midWidth - widthThreshold        ) &&
	 sb2[i].spikeUpCount > (int)round(2.5 * (float)sb2[i].spikeTotalCount / 10.00) &&
	 sb2[i].spikeDownCount > (int)round(2.5 * (float)sb2[i].spikeTotalCount / 10.00)
       )
    {
      asb2[j++] = sb2[i];
      asbCount2++;
    }
  }
  
  //debug
  /*
  FILE* ivalFile = fopen("/TEST/asb.txt", "w");
  for (int i = 0; i < asbCount2; i++)
  {
    fprintf(ivalFile, "row=%d, spikeTotalCount=%d, spikeUpCount=%d, spikeDownCount=%d, spikeUpAverageWidth=%d,\
	               spikeDownAverageWidth=%d\n",
		       asb2[i].row, asb2[i].spikeTotalCount, asb2[i].spikeUpCount, asb2[i].spikeDownCount,
		       asb2[i].spikeUpAverageWidth, asb2[i].spikeDownAverageWidth);
  }
  fclose(ivalFile);
  */
}


//Here we get only non overlapping blocks and
//the one with the max spikeTotalCount out of overlapping block sets
void getMaxCountNonOverlappingSpikeBlocks()
{
  nosb = (spikeBlock*) malloc(asbCount * sizeof(spikeBlock));
  
  spikeBlock maxBlock = asb[0];
  
  int j = 0;
  
  for (int i = 1; i < asbCount; i++)
  {
    if (asb[i].col - maxBlock.col > 5)
    {
      nosb[j++] = maxBlock;
      maxBlock = asb[i];
    }
    else if (asb[i].spikeTotalCount > maxBlock.spikeTotalCount)
    {
      maxBlock = asb[i];
    }
  }
  
  if (nosb[j-1].col != maxBlock.col)
    nosb[j++] = maxBlock;
  
  //debug
  /*  
  FILE* ivalFile = fopen("/TEST/nosb.txt", "w");
  for (int i = 0; i < j; i++)
  {
    fprintf(ivalFile, "col=%d, spikeTotalCount=%d, spikeUpCount=%d, spikeDownCount=%d, spikeUpAverageHeight=%d,\
	               spikeDownAverageHeight=%d\n",
		       nosb[i].col, nosb[i].spikeTotalCount, nosb[i].spikeUpCount, nosb[i].spikeDownCount,
		       nosb[i].spikeUpAverageHeight, nosb[i].spikeDownAverageHeight);
  }
  fclose(ivalFile);
  */
}


void getMaxCountNonOverlappingSpikeBlocks2()
{
  nosb2 = (spikeBlock2*) malloc(asbCount2 * sizeof(spikeBlock2));
  
  spikeBlock2 maxBlock = asb2[0];
  
  int j = 0;
  
  for (int i = 1; i < asbCount2; i++)
  {
    if (asb2[i].row - maxBlock.row > 5)
    {
      nosb2[j++] = maxBlock;
      maxBlock = asb2[i];
    }
    else if (asb2[i].spikeTotalCount > maxBlock.spikeTotalCount)
    {
      maxBlock = asb2[i];
    }
  }
  
  if (nosb2[j-1].row != maxBlock.row)
    nosb2[j++] = maxBlock;
  
  //debug
  /*  
  FILE* ivalFile = fopen("/TEST/nosb.txt", "w");
  for (int i = 0; i < j; i++)
  {
    fprintf(ivalFile, "row=%d, spikeTotalCount=%d, spikeUpCount=%d, spikeDownCount=%d, spikeUpAverageWidth=%d,\
	               spikeDownAverageWidth=%d\n",
		       nosb2[i].row, nosb2[i].spikeTotalCount, nosb2[i].spikeUpCount, nosb2[i].spikeDownCount,
		       nosb2[i].spikeUpAverageWidth, nosb2[i].spikeDownAverageWidth);
  }
  fclose(ivalFile);
  */
}

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

static void
debug_image_raw(const char *filename)
{
	FILE           *debug_file;
	unsigned char  *ptr;
	int             row, col;

	debug_file = fopen(filename, "wb");
	for (row = 0; row < img_data->height; ++row) {
		ptr = img_data->data[row];
		for (col = 0; col < img_data->width; ++col) {
			fputc(*ptr++, debug_file);
			fputc(*ptr++, debug_file);
			fputc(*ptr++, debug_file);
		}
	}

	fclose(debug_file);
}

int
get_xy(const char *filename, long *x, long *y)
{
	FILE           *filep;
	/* Use 65536 bytes: large enough for typical EXIF/ICC/XMP headers before SOF0 */
	#define GET_XY_BUFSZ 65536
	unsigned char   buf[GET_XY_BUFSZ];
	size_t          bytes_read;
	register int    i;
	unsigned short  block_length;

	filep = fopen(filename, "rb");
	if (filep == NULL) {
		fprintf(stderr, "Failed to open %s: %s\n",
			filename, strerror(errno));
		return -1;
	}

	bytes_read = fread(buf, 1, sizeof buf, filep);
	fclose(filep);

	/* Need at least 4 bytes for SOI + first marker */
	if (bytes_read < 4) {
		fprintf(stderr, "Error reading JFIF data from %s\n", filename);
		return -1;
	}

	i = 0;
	if ((buf[i] != 0xFF) || (buf[i + 1] != 0xD8)) {
		fprintf(stderr, "%s not a valid JPEG file\n", filename);
		return -1;
	}

	i += 4;
	if ((size_t)(i + 2) > bytes_read) return -1;
	block_length = buf[i] << 8 | buf[i + 1];

	while ((size_t)(i + 2) < bytes_read) {
		i += block_length;

		if ((size_t)(i + 9) > bytes_read) return -1;

		if (buf[i] != 0xFF) {
			return -1;
		}

		while ((size_t)(i + 2) < bytes_read && buf[i + 1] == 0xFF)
			i++;

		if ((size_t)(i + 9) > bytes_read) return -1;

		if (buf[i + 1] == 0xC0) {
			/*
			 * Found SOF0 marker: contains image height and width
			 */
			*y = buf[i + 5] << 8 | buf[i + 6];
			*x = buf[i + 7] << 8 | buf[i + 8];
			return 0;
		} else {
			i += 2;
			if ((size_t)(i + 2) > bytes_read) return -1;
			block_length = buf[i] << 8 | buf[i + 1];
		}
	}

	return -1;
}

void
free_image_data(struct image_data *imgdata)
{
	int             i;
	if (imgdata != NULL) {
		for (i = 0; i < imgdata->height; ++i)
			free(imgdata->data[i]);
		free(imgdata->data);
	}

	img_data = NULL;
}

void
steg_decode_init( char* message, 
		  short in_Steg_Decode_Specific_Diff, 
		  short in_Cb_Shift, 
		  short in_Cr_Shift,
		  short in_Steg_Decode_Bound,
		  short in_Steg_Decode_Anti_Crop_On,
		  short in_Steg_Decode_No_Crop_Hint,
		  short in_Steg_Anti_Crop_Percent,
	          short in_Steg_Decode_Ignore_Y,
                  short in_Steg_Decode_Y_Bound		  
		)
{
  watermark = message;
  Steg_Set_Decode = 1;
  
  watermark_font_size                = 10;
  Steg_Encode_Old_Overlay_On         = 0;
  Y_BUMP                             = 0;
  Y_BUMP_REACTIVE                    = 0;
  Watermark_X_Step                   = 10000;
  Watermark_Y_Step                   = 10000;	
    
  Steg_Encode_Col_Chroma_Shift_On    = 1;
  Steg_Encode_Col_Chroma_Shift       = 50;
  Steg_Encode_Col_Chroma_Shift_Cb_On = 1;
  Steg_Encode_Col_Chroma_Shift_Cr_On = 1;
  Steg_Encode_Col_Chroma_Shift_Down  = 0;
  Steg_Encode_Message                = "";
  
  Steg_Encode_Col_Step_factor        = 60;
  
  Steg_Decode_Specific_Diff          = in_Steg_Decode_Specific_Diff;
  Cb_Shift                           = in_Cb_Shift;
  Cr_Shift                           = in_Cr_Shift;
  
  Steg_Decode_Bound                  = in_Steg_Decode_Bound;
  
  //Steg_Decode_Sensitivity          = in_Steg_Decode_Sensitivity;
  Steg_Decode_Sensitivity            = 95;
  
  Steg_Decode_Anti_Crop_On           = in_Steg_Decode_Anti_Crop_On;
  
  Steg_Decode_No_Crop_Hint           = in_Steg_Decode_No_Crop_Hint;
  
  Steg_Anti_Crop_Percent             = in_Steg_Anti_Crop_Percent;

  Steg_Decode_Ignore_Y               = in_Steg_Decode_Ignore_Y;
  
  Steg_Decode_Y_Bound                = in_Steg_Decode_Y_Bound;
}



void
steg_init(char *mark,
	  short in_watermark_font_size,
	  short in_Steg_Encode_Old_Overlay_On,
	  short in_Y_BUMP,
          short in_Y_BUMP_REACTIVE,
	  short in_Watermark_X_Step,
          short in_Watermark_Y_Step,
	  short in_Steg_Encode_Anti_Crop_On,
	  short in_Steg_Anti_Crop_Percent,	  
	  short in_Steg_Encode_Col_Chroma_Shift_On,
	  short in_Steg_Encode_Col_Chroma_Shift,
	  short in_Steg_Encode_Col_Chroma_Shift_Cb_On,
	  short in_Steg_Encode_Col_Chroma_Shift_Cr_On,
	  short in_Steg_Encode_Col_Chroma_Shift_Down,
	  short in_Steg_Encode_Col_Step_factor,
	  short in_Cb_Shift,
	  short in_Cr_Shift,	  
	  char* in_Steg_Encode_Message)
{
	watermark = mark;
	
	//Fotios
	watermark_font_size                = in_watermark_font_size;
	Steg_Encode_Old_Overlay_On         = in_Steg_Encode_Old_Overlay_On;
	Y_BUMP                             = in_Y_BUMP;
	Y_BUMP_REACTIVE                    = in_Y_BUMP_REACTIVE;
	Watermark_X_Step                   = in_Watermark_X_Step;
        Watermark_Y_Step                   = in_Watermark_Y_Step;
	
	Steg_Encode_Anti_Crop_On           = in_Steg_Encode_Anti_Crop_On;
	Steg_Anti_Crop_Percent             = in_Steg_Anti_Crop_Percent;
	  
	Steg_Encode_Col_Chroma_Shift_On    = in_Steg_Encode_Col_Chroma_Shift_On;
	Steg_Encode_Col_Chroma_Shift       = in_Steg_Encode_Col_Chroma_Shift;
	Steg_Encode_Col_Chroma_Shift_Cb_On = in_Steg_Encode_Col_Chroma_Shift_Cb_On;
	Steg_Encode_Col_Chroma_Shift_Cr_On = in_Steg_Encode_Col_Chroma_Shift_Cr_On;
	Steg_Encode_Col_Chroma_Shift_Down  = in_Steg_Encode_Col_Chroma_Shift_Down;
	
	Steg_Encode_Col_Step_factor        = in_Steg_Encode_Col_Step_factor;
	Cb_Shift                           = in_Cb_Shift;
	Cr_Shift                           = in_Cr_Shift;
	
	Steg_Encode_Message                = in_Steg_Encode_Message;
	
	Steg_Set_Decode                    = 0;
}

void
steg_cleanup(void)
{
	if (img_data != NULL)
		free_image_data(img_data);
	watermark = NULL;
	Steg_Encode_Message = NULL;
}

/*
 * Error recovery stuff
 */
static void
trace_message(const char *msgtext)
{
	fprintf(stderr, msgtext,
		emethods->message_parm[0], emethods->message_parm[1],
		emethods->message_parm[2], emethods->message_parm[3],
		emethods->message_parm[4], emethods->message_parm[5],
		emethods->message_parm[6], emethods->message_parm[7]);
	fputs("\n", stderr);
}

static void
error_exit(const char *msgtext)
{
	trace_message(msgtext);
	(*emethods->free_all) ();
	longjmp(setjmp_buffer, 1);
}

/*
 * Input callbacks / functions
 */
static void
input_init(compress_info_ptr cinfo)
{
	if (img_data == NULL) return;
	
	row_idx = 0;
	cinfo->image_width                        = img_data->width;
	cinfo->image_height                       = img_data->height;
	cinfo->input_components                   = img_data->components;
	cinfo->in_color_space                     = img_data->colorspace;
	cinfo->data_precision                     = BITS_IN_JSAMPLE;
        cinfo->watermark                          = watermark;
	
	//-=Fotios=-
	cinfo->watermark_font_size                = watermark_font_size;
	cinfo->Steg_Encode_Old_Overlay_On         = Steg_Encode_Old_Overlay_On;
	cinfo->Y_BUMP                             = Y_BUMP; //percent
	cinfo->Y_BUMP_REACTIVE                    = Y_BUMP_REACTIVE;
        cinfo->Watermark_X_Step                   = Watermark_X_Step;
        cinfo->Watermark_Y_Step                   = Watermark_Y_Step;
	
	cinfo->Steg_Encode_Anti_Crop_On           = Steg_Encode_Anti_Crop_On;
	cinfo->Steg_Anti_Crop_Percent             = Steg_Anti_Crop_Percent;
	
	cinfo->Steg_Encode_Col_Chroma_Shift_On    = Steg_Encode_Col_Chroma_Shift_On;
	cinfo->Steg_Encode_Col_Chroma_Shift       = Steg_Encode_Col_Chroma_Shift;
	cinfo->Steg_Encode_Col_Chroma_Shift_Cb_On = Steg_Encode_Col_Chroma_Shift_Cb_On;
	cinfo->Steg_Encode_Col_Chroma_Shift_Cr_On = Steg_Encode_Col_Chroma_Shift_Cr_On;
	cinfo->Steg_Encode_Col_Chroma_Shift_Down  = Steg_Encode_Col_Chroma_Shift_Down;
	cinfo->Steg_Encode_Message                = Steg_Encode_Message;
	
        cinfo->Cb_Shift                           = Cb_Shift;
        cinfo->Cr_Shift                           = Cr_Shift;
	cinfo->Steg_Encode_Col_Step_factor        = Steg_Encode_Col_Step_factor;	
	
	//decode flag
	cinfo->Steg_Set_Decode                    = Steg_Set_Decode;
        cinfo->Steg_Decode_Specific_Diff          = Steg_Decode_Specific_Diff;
        cinfo->Steg_Decode_Bound                  = Steg_Decode_Bound;
	cinfo->Steg_Decode_Sensitivity            = Steg_Decode_Sensitivity;
	cinfo->Steg_Decode_Anti_Crop_On           = Steg_Decode_Anti_Crop_On;
	cinfo->Steg_Decode_No_Crop_Hint           = Steg_Decode_No_Crop_Hint;
	cinfo->Steg_Decode_Ignore_Y               = Steg_Decode_Ignore_Y;
	cinfo->Steg_Decode_Y_Bound                = Steg_Decode_Y_Bound;
}

static void
get_input_row(compress_info_ptr cinfo, JSAMPARRAY pixel_row)
{
	register JSAMPROW red, green, blue;
	register JSAMPLE *cptr, *nptr;
	register long   col = 0;

	red = pixel_row[RED];
	green = pixel_row[GREEN];
	blue = pixel_row[BLUE];

	cptr = img_data->data[row_idx];
	nptr = img_data->data[row_idx];

	while (col < img_data->width) {
		*red++ = *cptr++;
		*green++ = *cptr++;
		*blue++ = *cptr++;
		col++;
	}
	row_idx++;
}

static void
input_term(compress_info_ptr cinfo)
{
	row_idx = 0;
	cinfo->color_row_idx = 0;
	if (cinfo->overlay != NULL)
		gdImageDestroy(cinfo->overlay);
	cinfo->watermark = NULL;
	watermark = NULL;
}

/*
 * Might need to add checking here.
 */
static void
c_ui_method_selection(compress_info_ptr cinfo)
{
	jselwjfif(cinfo);
}

int
steg_compress(const char *filename, int quality)
{
	int             q = 100;
	struct Compress_info_struct cinfo;
	struct Compress_methods_struct c_methods;
	struct External_methods_struct e_methods;

	if (quality > 0)
		q = quality;

	cinfo.methods = &c_methods;
	cinfo.emethods = &e_methods;

	c_methods.c_ui_method_selection = c_ui_method_selection;
	c_methods.input_init = input_init;
	c_methods.get_input_row = get_input_row;
	c_methods.input_term = input_term;

	jselerror(&e_methods);
	jselmemmgr(&e_methods);

	emethods = &e_methods;
	e_methods.error_exit = error_exit;
	e_methods.trace_message = trace_message;
	e_methods.trace_level = 0;
	e_methods.num_warnings = 0;
	e_methods.first_warning_level = 0;
	e_methods.more_warning_level = 3;

	if (setjmp(setjmp_buffer)) {
		fclose(cinfo.input_file);
		if (img_data != NULL)
			free(img_data);
		return -1;
	}

	j_c_defaults(&cinfo, q, FALSE);

	cinfo.output_file = fopen(filename, "wb");
	if (cinfo.output_file == NULL) {
		fprintf(stderr, "Error opening file %s: %s\n",
			filename, strerror(errno));
		return -1;
	}

	jpeg_compress(&cinfo);
	fclose(cinfo.output_file);

	return 0;
}


int
steg_compress2(int quality)
{
	int             q = 100;
	struct Compress_info_struct cinfo;
	struct Compress_methods_struct c_methods;
	struct External_methods_struct e_methods;

	if (quality > 0)
		q = quality;

	cinfo.methods = &c_methods;
	cinfo.emethods = &e_methods;

	c_methods.c_ui_method_selection = c_ui_method_selection;
	c_methods.input_init = input_init;
	c_methods.get_input_row = get_input_row;
	c_methods.input_term = input_term;

	jselerror(&e_methods);
	jselmemmgr(&e_methods);

	emethods = &e_methods;
	e_methods.error_exit = error_exit;
	e_methods.trace_message = trace_message;
	e_methods.trace_level = 0;
	e_methods.num_warnings = 0;
	e_methods.first_warning_level = 0;
	e_methods.more_warning_level = 3;

	if (setjmp(setjmp_buffer)) {
		fclose(cinfo.input_file);
		if (img_data != NULL)
			free(img_data);
		return -1;
	}

	j_c_defaults(&cinfo, q, FALSE);
        
	cinfo.output_file = fopen("/tmp/steg_decodedump.jpg", "wb");
	//cinfo.output_file = stdout;
	
	if (cinfo.output_file == NULL) {
		fprintf(stderr, "Error opening file %s: %s\n",
			"/tmp/steg_decodedump.jpg", strerror(errno));
		return -1;
	}
	

	jpeg_compress(&cinfo);
	
	
	fclose(cinfo.output_file);
        
	
	return 0;
}

/*
 * Output methods / functions
 */
static void
output_init(decompress_info_ptr cinfo)
{
	int             i;

	if (img_data == NULL)
		img_data = malloc(sizeof *img_data);
	else {
		if (img_data->data != NULL)
			free_image_data(img_data);
		img_data = malloc(sizeof *img_data);
	}

	img_data->data = malloc(cinfo->image_height * sizeof *img_data->data);

	if (cinfo->out_color_space == CS_GRAYSCALE) {
		img_data->datalen = cinfo->image_height * cinfo->image_width;
		for (i = 0; i < cinfo->image_height; ++i)
			img_data->data[i] = malloc(cinfo->image_width *
						   sizeof *img_data->data);
	} else {
		img_data->datalen = cinfo->image_height *
		    cinfo->image_width * 3;
		for (i = 0; i < cinfo->image_height; ++i)
			img_data->data[i] = malloc(cinfo->image_width * 3 *
						   sizeof *img_data->data);
	}

	img_data->height = cinfo->image_height;
	img_data->width = cinfo->image_width;
	img_data->precision = cinfo->data_precision;
	img_data->colorspace = cinfo->out_color_space;
	img_data->components = cinfo->num_components;
}

/*
 * Called if cinfo->quantize_colors is set.
 */
static void
put_color_map(decompress_info_ptr cinfo, int num_colors, JSAMPARRAY colormap)
{
	/*
	 * Currently unsupported
	 */
}

/*
 * Simply writes the raw data to our 2d data buffer
 */
static void
put_pixel_rows(decompress_info_ptr cinfo, int num_rows, JSAMPIMAGE pixel_data)
{
	register JSAMPROW red, green, blue;
	register int    row;
	register long   col = 0;
	register unsigned char *cptr;

	for (row = 0; row < num_rows; ++row) {
		red = pixel_data[RED][row];
		green = pixel_data[GREEN][row];
		blue = pixel_data[BLUE][row];

		cptr = img_data->data[row_idx + row];
		while (col < cinfo->image_width) {
			*cptr++ = GETJSAMPLE(*red++);
			*cptr++ = GETJSAMPLE(*green++);
			*cptr++ = GETJSAMPLE(*blue++);
			col++;
		}
		col = 0;
	}

	row_idx += num_rows;
}

static void
output_term(decompress_info_ptr cinfo)
{
	row_idx = 0;
}

/*
 * Might need to add checking in here eventually.
 */
static void
d_ui_method_selection(decompress_info_ptr cinfo)
{
	cinfo->methods->output_init = output_init;
	cinfo->methods->put_color_map = put_color_map;
	cinfo->methods->put_pixel_rows = put_pixel_rows;
	cinfo->methods->output_term = output_term;
}

struct image_data *
steg_decompress(const char *filename)
{
	struct Decompress_info_struct cinfo;
	struct Decompress_methods_struct dc_methods;
	struct External_methods_struct e_methods;

	cinfo.input_file = fopen(filename, "rb");
	if (cinfo.input_file == NULL) {
		fprintf(stderr, "Couldn't open %s: %s\n",
			filename, strerror(errno));
		return NULL;
	}

	cinfo.output_file = NULL;
	cinfo.methods = &dc_methods;
	cinfo.emethods = &e_methods;

	emethods = &e_methods;
	e_methods.error_exit = error_exit;
	e_methods.trace_message = trace_message;
	e_methods.trace_level = 0;
	e_methods.num_warnings = 0;
	e_methods.first_warning_level = 0;
	e_methods.more_warning_level = 3;

	if (setjmp(setjmp_buffer)) {
		fclose(cinfo.input_file);
		if (img_data != NULL)
			free(img_data);
		return NULL;
	}

	jselmemmgr(&e_methods);

	dc_methods.d_ui_method_selection = d_ui_method_selection;
	j_d_defaults(&cinfo, TRUE);

	jselrjfif(&cinfo);

	jpeg_decompress(&cinfo);

	fclose(cinfo.input_file);

	return img_data;
}
