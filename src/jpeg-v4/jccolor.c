/*
 * jccolor.c
 *
 * Copyright (C) 1991, 1992, Thomas G. Lane.
 * Copyright (C) 2011, 2012, Fotios Basagiannis.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains input colorspace conversion routines.
 * These routines are invoked via the methods get_sample_rows
 * and colorin_init/term.
 */

#define MALLOC_FAR_OVERHEAD  (SIZEOF(void FAR *))	/* for jget_large() */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "jinclude.h"
#include <stdlib.h>
#include <limits.h>

#include <math.h>

#include <gd.h>

#define TTF_LOCATION FONTDIR "VeraMono.ttf"

static JSAMPARRAY pixel_row;	/* Workspace for a pixel row in input format */
static JSAMPARRAY last_pixel_row;
static JSAMPROW last_pixel_row0;
static JSAMPROW last_pixel_row1;
static JSAMPROW last_pixel_row2;

//-=Fotios=-

// bit-string stuff /////////////////////////////
static char* bitS = NULL;
static char* bitSptr = NULL;
static char* bitSptr1 = NULL;
static char* bitSptr2 = NULL;

static char* leftBitString  = NULL;
static char* rightBitString = NULL;

static char* leftBitString_ptr  = NULL;
static char* rightBitString_ptr = NULL;
/////////////////////////////////////////////////

// bit-arrays ///////////////////////////////////
static int leftBitArray[20][20];
static int rightBitArray[20][20];
/////////////////////////////////////////////////

static int globRow = 0;
static int curRowStep = 1;
static int stepRow = 0;

static int Steg_Encode_Col_Bit_Value = 2; //should not become a web option
static int Steg_Encode_Row_Bit_Value1 = 2; //should not become a web option
static int Steg_Encode_Row_Bit_Value2 = 2; //should not become a web option

static int Steg_Anti_Crop_Anchor_Flag = 0;
static int Steg_Anti_Crop_Anchor_Flag2 = 0;
static int rowEncodeEnabled = 0;

static FILE* ivalFile = NULL;
static FILE* anticropFile = NULL;
static FILE* testFile = NULL;

//////////////
//extern char* Steg_Message;
//extern int *spike[];
//extern int** spike;
//compress_info_ptr my_cinfo = NULL;
//static int rowAnchorSpike[20][20];
//////////////

//short spike[2048][2048];

static float Steg_Encode_Col_Step = 0.0;
static float Steg_Encode_Row_Step = 0.0;

static float Steg_Anti_Crop_Frame_Width = 0.0;

static int   Steg_Anti_Crop_First_Col   = 0;
static int   Steg_Anti_Crop_First_Row   = 0;

//should only become a web option if
//it is possible to detect chroma shifts 
//without using the original JPEG as a guide
static short Steg_Encode_Col_Step_factor = 0;
static short Steg_Encode_Row_Step_factor = 0;

//static short Steg_Encode_Row_Step_factor = 62; //divide image's height into 7 x 4 rowBars or 4 7-bit strips
				               //(plus anchor and delimiter bars)

/**************** RGB -> YCbCr conversion: most common case **************/

/*
 * YCbCr is defined per CCIR 601-1, except that Cb and Cr are
 * normalized to the range 0..MAXJSAMPLE rather than -0.5 .. 0.5.
 * The conversion equations to be implemented are therefore
 *      Y  =  0.29900 * R + 0.58700 * G + 0.11400 * B
 *      Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B  + MAXJSAMPLE/2
 *      Cr =  0.50000 * R - 0.41869 * G - 0.08131 * B  + MAXJSAMPLE/2
 * (These numbers are derived from TIFF 6.0 section 21, dated 3-June-92.)
 *
 * To avoid floating-point arithmetic, we represent the fractional constants
 * as integers scaled up by 2^16 (about 4 digits precision); we have to divide
 * the products by 2^16, with appropriate rounding, to get the correct answer.
 *
 * For even more speed, we avoid doing any multiplications in the inner loop
 * by precalculating the constants times R,G,B for all possible values.
 * For 8-bit JSAMPLEs this is very reasonable (only 256 entries per table);
 * for 12-bit samples it is still acceptable.  It's not very reasonable for
 * 16-bit samples, but if you want lossless storage you shouldn't be changing
 * colorspace anyway.
 * The MAXJSAMPLE/2 offsets and the rounding fudge-factor of 0.5 are included
 * in the tables to save adding them separately in the inner loop.
 */

#ifdef SIXTEEN_BIT_SAMPLES
#define SCALEBITS	14	/* avoid overflow */
#else
#define SCALEBITS	16	/* speedier right-shift on some machines */
#endif
#define ONE_HALF	((INT32) 1 << (SCALEBITS-1))
#define FIX(x)		((INT32) ((x) * (1L<<SCALEBITS) + 0.5))

/*
 * We allocate one big table and divide it up into eight parts, instead of
 * doing eight alloc_small requests.  This lets us use a single table base
 * address, which can be held in a register in the inner loops on many
 * machines (more than can hold all eight addresses, anyway). 
 */

static INT32   *rgb_ycc_tab;	/* => table for RGB to YCbCr conversion */
#define R_Y_OFF		0	/* offset to R => Y section */
#define G_Y_OFF		(1*(MAXJSAMPLE+1))	/* offset to G => Y section */
#define B_Y_OFF		(2*(MAXJSAMPLE+1))	/* etc. */
#define R_CB_OFF	(3*(MAXJSAMPLE+1))
#define G_CB_OFF	(4*(MAXJSAMPLE+1))
#define B_CB_OFF	(5*(MAXJSAMPLE+1))
#define R_CR_OFF	B_CB_OFF	/* B=>Cb, R=>Cr are the same */
#define G_CR_OFF	(6*(MAXJSAMPLE+1))
#define B_CR_OFF	(7*(MAXJSAMPLE+1))
#define TABLE_SIZE	(8*(MAXJSAMPLE+1))

#ifdef STEG_SUPPORTED
METHODDEF void
generate_text_overlay(compress_info_ptr cinfo)
{
        gdImagePtr im;
        int black, white;
        long i, j;
        long max_x, max_y, x, y;
        int brect[8];
        char *err = NULL;
#ifdef DEBUG
        FILE *test;
#endif

        im = gdImageCreate(cinfo->image_width, cinfo->image_height);
        black = gdImageColorResolve(im, 0, 0, 0);
        white = gdImageColorResolve(im, 255, 255, 255);

        err = gdImageStringFT(NULL, &brect[0], 0, TTF_LOCATION, cinfo->watermark_font_size, 0.0, 0, 0,
                              cinfo->watermark);
        if (err != NULL) {
                fprintf(stderr, "%s\n", err);
                gdImageDestroy(im);
                return;
        }

        /*
         * See how many marks the image can hold.  Could fit more
         * than this, but there should be some reasonable spacing
         * defaults.
         */
        x = brect[2] - brect[6] + strlen(cinfo->watermark);
        y = brect[3] - brect[7] + strlen(cinfo->watermark);
        //max_x = cinfo->image_width / (x * 2);
	max_x = cinfo->image_width / x;
        //max_y = cinfo->image_height / (y * 2);
	max_y = cinfo->image_width / y;
	
	//-=Fotios=-
	short Watermark_X_Step = cinfo->Watermark_X_Step;
        short Watermark_Y_Step = cinfo->Watermark_Y_Step;
	
	//string_to_bitstring(cinfo->watermark);

        //for (i = x; i < max_x, i < cinfo->image_width; i += x * 2) {
                //for (j = y; j < max_y, j < cinfo->image_height; j += y * 2) {
        for (i = Watermark_X_Step; i < max_x, i < cinfo->image_width; i += x + Watermark_X_Step) {
                for (j = Watermark_Y_Step; j < max_y, j < cinfo->image_height; j += y + Watermark_Y_Step) {
		        
                        err = gdImageStringFT(im, &brect[0], -white, 
                                              TTF_LOCATION, cinfo->watermark_font_size, 0.0, i, j, 
                                              cinfo->watermark);
                        
			/*
                        err = gdImageStringFT(im, &brect[0], -white, 
                                              TTF_LOCATION, cinfo->watermark_font_size, 0.0, i, j, 
                                              bitS);
                        */
			
                        if (err != NULL) {
                                fprintf(stderr, "%s\n", err);
                                gdImageDestroy(im);
                                return;
                        }
                }
        }
        
        //free(bitS);

#ifdef DEBUG
        test = fopen("overlay.jpg", "wb");
        gdImageJpeg(im, test, -1);
#endif

        cinfo->overlay = im;
}
#endif

/*
 * Initialize for colorspace conversion.
 */

METHODDEF void
rgb_ycc_init(compress_info_ptr cinfo)
{
	INT32           i;

	/*
	 * Allocate a workspace for the result of get_input_row. 
	 */
	pixel_row = (*cinfo->emethods->alloc_small_sarray)
	    (cinfo->image_width, (long) cinfo->input_components);
	/*    
	last_pixel_row = (*cinfo->emethods->alloc_small_sarray)
	    (cinfo->image_width, (long) cinfo->input_components);	
	    */
	    
#ifdef STEG_SUPPORTED
        generate_text_overlay(cinfo);
        cinfo->color_row_idx = 0;
#endif

	/*
	 * Allocate and fill in the conversion tables. 
	 */
	rgb_ycc_tab = (INT32 *) (*cinfo->emethods->alloc_small)
	    (TABLE_SIZE * SIZEOF(INT32));

	for (i = 0; i <= MAXJSAMPLE; i++) {
		rgb_ycc_tab[i + R_Y_OFF] = FIX(0.29900) * i;
		rgb_ycc_tab[i + G_Y_OFF] = FIX(0.58700) * i;
		rgb_ycc_tab[i + B_Y_OFF] = FIX(0.11400) * i + ONE_HALF;
		rgb_ycc_tab[i + R_CB_OFF] = (-FIX(0.16874)) * i;
		rgb_ycc_tab[i + G_CB_OFF] = (-FIX(0.33126)) * i;
		rgb_ycc_tab[i + B_CB_OFF] =
		    FIX(0.50000) * i + ONE_HALF * (MAXJSAMPLE + 1);
		/*
		 * B=>Cb and R=>Cr tables are the same rgb_ycc_tab[i+R_CR_OFF] 
		 * = FIX(0.50000) * i + ONE_HALF*(MAXJSAMPLE+1); 
		 */
		rgb_ycc_tab[i + G_CR_OFF] = (-FIX(0.41869)) * i;
		rgb_ycc_tab[i + B_CR_OFF] = (-FIX(0.08131)) * i;
	}
}


//5-bit chars - ASCII 97 and up 
void string_to_bitstring(char* s)
{
  int len = strlen(s);
  int n = 0;
  
  bitS = (char*) malloc(1024); //mem alloc on the global var will be cleared by the calling function
  char* b = bitS;
  //*b++ = '2'; //we start with a non-shift bar
  
  for(int i=0; i < len; i++)
  {
    n = (int)s[i] - 96;
    int f = 16;
    int bit = 0;
    for (int j=0; j < 5; j++)
    {
      bit = n / f;
      if (bit) 
	*b++ = '1';
      else
	*b++ = '0';
      //*b++ = '2';
      n = n % f;
      f /= 2;
    }
  }
  
  *b = '\0';
}


//7-bit ASCII chars
void string_to_bitstring2(char* s)
{
  int len = strlen(s);
  int n = 0;
  
  bitS = (char*) malloc(1024); //mem alloc on the global var will be cleared by the calling function
  char* b = bitS;
  
  for(int i = 0; i < len; i++)
  {
    n = (int)s[i];
    int f = 64;
    int bit = 0;
    for (int j=0; j < 7; j++)
    {
      bit = n / f;
      
      if (bit) 
	*b++ = '1';
      else
	*b++ = '0';

      n = n % f;
      f /= 2;
    }
  }
  
  *b = '\0';
}


void reverseString(char* s)
{
  int len = strlen(s);  
  char temp = '\0';
  
  for (int i = 0; i < len/2 ; i++)
  {
    temp       = s[len-i-1];
    s[len-i-1] = s[i];
    s[i]       = temp;
  }
}


void string_to_bitArrays(char* s)
{
  string_to_bitstring2(s);
  
  leftBitString  = (char*) malloc(1024);
  rightBitString = (char*) malloc(1024);
  
  leftBitString_ptr = leftBitString;
  rightBitString_ptr = rightBitString;
  
  strcpy(leftBitString, bitS);
  strcpy(rightBitString, bitS);
  
  free(bitS);
  bitS = NULL;
  
  if (ivalFile)
  {
    fprintf(ivalFile, "leftBitString =  %s\n", leftBitString);
    fprintf(ivalFile, "rightBitString = %s\n", rightBitString);
  }
  
  //reverseString(leftBitString);
  
  //Now fill up the bit arrays
  for (int i = 0; i < 14; i++)
  {
    if (!(*leftBitString_ptr)) leftBitString_ptr = leftBitString;
    if (!(*rightBitString_ptr)) rightBitString_ptr = rightBitString;
    
    for (int j = 0; j < 7; j++)
    {
      leftBitArray[j][i]   = (int)(*leftBitString_ptr) - 48;
      //if (ivalFile) fprintf(ivalFile, "%c - ", *leftBitString_ptr);
      leftBitArray[j+7][i] = (int)(*leftBitString_ptr++) - 48;
    }
    
    for(int j = 0; j < 7; j++)
    { 
      rightBitArray[j][i]   = (int)(*rightBitString_ptr) - 48;
      //if (ivalFile) fprintf(ivalFile, "%c - ", *rightBitString_ptr);
      rightBitArray[j+7][i] = (int)(*rightBitString_ptr++) - 48;
    }
  }
  
  //if (ivalFile) fprintf(ivalFile, "--------------------------------------\n");
  
  //free left and right bit-strings here
  free(leftBitString);
  free(rightBitString);
  
  leftBitString_ptr  = leftBitString  = NULL;
  rightBitString_ptr = rightBitString = NULL;
}


void string_to_reverse_bitstrings(char* s)
{
  int len = 0;
  int n = 0;
  
  //mem alloc on the global vars will be cleared by the calling function
  leftBitString  = (char*) malloc(1024);
  rightBitString = (char*) malloc(1024);
  
  char* lbs = leftBitString;
  char* rbs = rightBitString;
  
  char* leftString  = (char*) malloc(1024);
  char* rightString = (char*) malloc(1024);
  
  char* ls = leftString;
  char* rs = rightString;
  
  //first put odd and even indexed encode
  //string chars in separate strings
  len = strlen(s);
  for (int i = 0; i < len; i++)
  {
    if (i % 2)
      *ls++ = *s++;
    else
      *rs++ = *s++;
  }
  
  *ls = '\0';
  *rs = '\0';
  
  ls = leftString;
  rs = rightString;
  
  //now encode chars of left string to corresponding bit-string
  len = strlen(leftString);
  for (int i = 0; i < len; i++)
  {
    n = (int)leftString[i] - 96;
    int f = 16;
    int bit = 0;
    for (int j=0; j < 5; j++)
    {
      bit = n / f;
      
      if (bit) 
	*lbs++ = '1';
      else
	*lbs++ = '0';
      
      n = n % f;
      f /= 2;
    }
  }
  *lbs = '\0';
  
  //then encode chars of right string to corresponding bit-string
  len = strlen(rightString);
  for (int i = 0; i < len; i++)
  {
    n = (int)rightString[i] - 96;
    int f = 16;
    int bit = 0;
    for (int j=0; j < 5; j++)
    {
      bit = n / f;
      
      if (bit) 
	*rbs++ = '1';
      else
	*rbs++ = '0';
      
      n = n % f;
      f /= 2;
    }
  }
  *rbs = '\0';
  
  //now reverse both bit-strings
  reverseString(leftBitString); //only left string needs to be reversed
  //reverseString(rightBitString);
  
  //free temp strings
  free(leftString);
  free(rightString);
}


/*
 * Fetch some rows of pixels from get_input_row and convert to the
 * JPEG colorspace.
 */
METHODDEF void
get_rgb_ycc_rows(compress_info_ptr cinfo,
		 int rows_to_read,
		 JSAMPIMAGE image_data)
{
#ifdef SIXTEEN_BIT_SAMPLES
	register UINT16 r, g, b;
#else
	register int    r, g, b;
#endif
	register int    Y1, Cb1, Cr1, Y2, Cb2, Cr2;
        register int    color;
	register INT32 *ctab = rgb_ycc_tab;
	register JSAMPROW inptr0, inptr1, inptr2;
	register JSAMPROW last_inptr0, last_inptr1, last_inptr2;	
	register JSAMPROW outptr0, outptr1, outptr2; 
	//register JSAMPROW last_outptr0, last_outptr1, last_outptr2;
	register JSAMPLE last_outptr0, last_outptr1, last_outptr2;
	//register JSAMPROW Y, Cb, Cr;
	register long   col;
	long            width = cinfo->image_width;
	long            height = cinfo->image_height;
	int             row;
	
        //FOTIOS
	//FILE *ivalFile = fopen("/TEST/ival.txt", "a");
	//FILE *ivalFile = fopen("/TEST/running.txt", "a");
	//fprintf(ivalFile, "running...\n");
	//fclose(ivalFile);
	
	//Option variables         
	short Steg_Encode_Anti_Crop_On   = cinfo->Steg_Encode_Anti_Crop_On;
	short Steg_Anti_Crop_Percent     = cinfo->Steg_Anti_Crop_Percent;	
        
	// -=Fotios=-
	if (globRow == 0)
	{
	  my_cinfo->image_width = cinfo->image_width;
	  my_cinfo->image_height = cinfo->image_height;
	  //my_cinfo_struct = *cinfo;
	  /*
	  FILE* ivalFile11 = fopen("/mnt/www/fotios/domino.bz/steg/steg/jccolorc_dimensions.txt", "w");
	  fprintf(ivalFile11, "width=%ld, height=%ld\n", my_cinfo->image_width, my_cinfo->image_height);
	  fclose(ivalFile11);
	  */
	  if (width >= height)
	  {
	    if (Steg_Encode_Anti_Crop_On)
	    {
	      Steg_Anti_Crop_Frame_Width  = (float)( (float)width * (float)Steg_Anti_Crop_Percent / 100.00 );
	      Steg_Anti_Crop_First_Col    = (int)round(Steg_Anti_Crop_Frame_Width);
	      Steg_Encode_Col_Step_factor = 58;
	      Steg_Encode_Col_Step        = ((float)width - 2.0 * Steg_Anti_Crop_Frame_Width) / (float)Steg_Encode_Col_Step_factor;
	    }
	    else
	    {
	      Steg_Encode_Col_Step_factor = 56;
	      Steg_Encode_Col_Step = (float)width / (float)Steg_Encode_Col_Step_factor;
	    }
	  }
	  else
	  {
	    if (Steg_Encode_Anti_Crop_On)
	    {
	      Steg_Anti_Crop_Frame_Width  = (float)( (float)height * (float)Steg_Anti_Crop_Percent / 100.00 );
	      Steg_Anti_Crop_First_Row    = (int)round(Steg_Anti_Crop_Frame_Width);
	      Steg_Encode_Row_Step_factor = 58;
	      Steg_Encode_Row_Step        = ((float)height - 2.0 * Steg_Anti_Crop_Frame_Width) / (float)Steg_Encode_Row_Step_factor;
	    }
	    else
	    {
	      Steg_Encode_Row_Step_factor = 56;
	      Steg_Encode_Row_Step = (float)height / (float)Steg_Encode_Row_Step_factor;
	    }	    
	    
	  }
	}//if globRow == 0
	
		                                            
        //short Steg_Encode_Col_Step_factor      = cinfo->Steg_Encode_Col_Step_factor;
	
	//should not become web option
	//Steg_Encode_Row_Step                     = (float)height / (float)Steg_Encode_Row_Step_factor;
	
	////////////////////////////////////////////////////////
	/*
	int colAnchor1_step                      = (int) round(Steg_Encode_Col_Step_factor / 2) - 1;
	int colAnchor2_step                      = (int) round(Steg_Encode_Col_Step_factor / 2) + 1;
	
	int colAnchor1_col                       = (int) round(colAnchor1_step * Steg_Encode_Col_Step);
	int colAnchor2_col                       = (int) round(colAnchor2_step * Steg_Encode_Col_Step);
	*/
	///////////////////////////////////////////////////////
	/*
	int rowAnchor1_step                      = (int) round(Steg_Encode_Row_Step_factor / 2) - 1;
	int rowAnchor2_step                      = (int) round(Steg_Encode_Row_Step_factor / 2) + 1;
	
	int rowAnchor1_row                       = (int) round(rowAnchor1_step * Steg_Encode_Row_Step);
	int rowAnchor2_row                       = (int) round(rowAnchor2_step * Steg_Encode_Row_Step);
	*/
	
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	//The following vars beginning with "Steg_Encode_Col" do not have anything in particular to do
	//with Cols (they are used in both Col-wise and Row-wise encoding/decoding). The "Col" part of
	//their name was kept sp that the name is not changed throughout the application.
	//It is recommended that this changes in a future version of this module and a separate
	//set of vars is used for row-wise encoding/decoding while these are used in col-wise
	//encoding/decoding only.
	short Steg_Encode_Col_Chroma_Shift_On    = cinfo->Steg_Encode_Col_Chroma_Shift_On;
	short Steg_Encode_Col_Chroma_Shift       = cinfo->Steg_Encode_Col_Chroma_Shift;
	short Steg_Encode_Col_Chroma_Shift_Cb_On = cinfo->Steg_Encode_Col_Chroma_Shift_Cb_On;
	short Steg_Encode_Col_Chroma_Shift_Cr_On = cinfo->Steg_Encode_Col_Chroma_Shift_Cr_On; 
	short Steg_Encode_Col_Chroma_Shift_Down  = cinfo->Steg_Encode_Col_Chroma_Shift_Down;
	
	short Steg_Encode_Old_Overlay_On         = cinfo->Steg_Encode_Old_Overlay_On;
	short Y_BUMP                             = cinfo->Y_BUMP; //percent
	short Y_BUMP_REACTIVE                    = cinfo->Y_BUMP_REACTIVE;
	//float y_bump = Y_BUMP/100;
	
	short Steg_Decode_Specific_Diff          = cinfo->Steg_Decode_Specific_Diff;
	short Cb_Shift                           = cinfo->Cb_Shift;
	short Cr_Shift                           = cinfo->Cr_Shift;
	
	short Steg_Decode_Bound                  = cinfo->Steg_Decode_Bound;
	
	//fprintf(ivalFile, "image width = %d --- \n", width);
        //fprintf(ivalFile, "image bar step = %d --- \n", Steg_Encode_Col_Step);
	//fprintf(ivalFile, "----------------------------------\n");
	
	if ( !(cinfo->Steg_Set_Decode) )
	{
	  
	  if (globRow == 0)
	  {
	    string_to_bitstring2(cinfo->Steg_Encode_Message);
	  }
	  
	  //fprintf(ivalFile, "bitS is:  %s\n", bitS);
	  //fclose(ivalFile);

	  //ROW PROCESSING
	  for (row = 0; row < rows_to_read; row++, globRow++) 
	  {
	    
		  /*
		  * Read one row from the source file 
		  */
		  (*cinfo->methods->get_input_row) (cinfo, pixel_row);
		  cinfo->color_row_idx++;
		  
		  /*
		  * Convert colorspace 
		  */
		  inptr0 = pixel_row[0];
		  inptr1 = pixel_row[1];
		  inptr2 = pixel_row[2];
		  
		  //2D ptr assignments to proper positions in the 3D image matrix
		  outptr0 = image_data[0][row];
		  outptr1 = image_data[1][row];
		  outptr2 = image_data[2][row];
		  
		  int curColStep = 1;
		  //int curRowStep = 1;
		  
		  //COLUMN PROCESSING
		  for (col = 0; col < width; col++) {
			  r = GETJSAMPLE(inptr0[col]);
			  g = GETJSAMPLE(inptr1[col]);
			  b = GETJSAMPLE(inptr2[col]);
			  
			  //FOTIOS
			  //fprintf(ivalFile, "r = %d - g = %d - b = %d --- ", r, g, b);
			  //fprintf(ivalFile, "col = %d --- ", col);

  #ifdef STEG_SUPPORTED
			  /*
			  * If the inputs are 0..MAXJSAMPLE, the outputs of
			  * these equations must be too; we do not need an
			  * explicit range-limiting operation. Hence the value
			  * being shifted is never negative, and we don't need
			  * the general RIGHT_SHIFT macro. 
			  */

			  /*
			  * Y 
			  */
			  outptr0[col] = (JSAMPLE)
			      ((ctab[r + R_Y_OFF] + ctab[g + G_Y_OFF] +
				ctab[b + B_Y_OFF])
			      >> SCALEBITS);
			  /*
			  * Cb 
			  */
			  outptr1[col] = (JSAMPLE)
			      ((ctab[r + R_CB_OFF] + ctab[g + G_CB_OFF] +
				ctab[b + B_CB_OFF])
			      >> SCALEBITS);
			  /*
			  * Cr 
			  */
			  outptr2[col] = (JSAMPLE)
			      ((ctab[r + R_CR_OFF] + ctab[g + G_CR_OFF] +
				ctab[b + B_CR_OFF])
			      >> SCALEBITS);
			    
			  
			  /*
			  * Should be replaced with a lookup table in the
			  * final phase!
			  */
			  // -=Fotios=-
			  // Here the Y element of each the text overlay's pixels is 
			  // slightly modified to become visible.
			  // Modifications are done relative to the current Y value of
			  // the pixel being processed. 
			  color = gdImageGetPixel(cinfo->overlay, col,
						  cinfo->color_row_idx);
			  
			  if (!Steg_Encode_Old_Overlay_On)
			  {			
			    if ( (cinfo->overlay->red[color] != 0)   &&
				(cinfo->overlay->green[color] != 0) &&
				(cinfo->overlay->blue[color] != 0)      ) 
			    {
			      if (outptr0[col] + outptr0[col] * Y_BUMP/100 < 255)
			      { 
				if (outptr0[col] < 63)
				  outptr0[col] += (outptr0[col] * Y_BUMP/100);
				else if (outptr0[col] >= 63 && outptr0[col] < 127) 
				  outptr0[col] += (outptr0[col] * Y_BUMP/100/2);
				else if (outptr0[col] >= 127 && outptr0[col] < 189)
				  outptr0[col] += (outptr0[col] * Y_BUMP/100/3);
				else 
				  outptr0[col] += (outptr0[col] * Y_BUMP/100/3);
			      }
			      else if ( Y_BUMP_REACTIVE && 
					(outptr0[col] - outptr0[col] * Y_BUMP/100 > 0) )
			      {
				if (outptr0[col] < 63)
				  outptr0[col] -= (outptr0[col] * Y_BUMP/100);
				else if (outptr0[col] >= 63 && outptr0[col] < 127) 
				  outptr0[col] -= (outptr0[col] * Y_BUMP/100/2);
				else if (outptr0[col] >= 127 && outptr0[col] < 189)
				  outptr0[col] -= (outptr0[col] * Y_BUMP/100/3);
				else 
				  outptr0[col] -= (outptr0[col] * Y_BUMP/100/3);
			      }
			    }
			  }
			  else
			  {
			    if ((cinfo->overlay->red[color] != 0) &&
				(cinfo->overlay->green[color] != 0) &&
				(cinfo->overlay->blue[color] != 0)) {
				    if (outptr0[col] < 63) {
					    outptr0[col] -= (outptr0[col] * .04);
				    } else if ((outptr0[col] > 63) &&
					      (outptr0[col] < 127)) {
					    outptr0[col] -= (outptr0[col] * .02);
				    } else if ((outptr0[col] > 127) &&
					      (outptr0[col] < 189)) {
					    outptr0[col] -= (outptr0[col] * .02);
				    } else {
					    outptr0[col] -= (outptr0[col] * .02);
				    }
			    }
			  }
			
			  
			  // -=Fotios=-
			  // ** Steg_Encode_Row_Step has been deprecated **
			  // Here we bar-encode the steganographic information into the image.
			  // The info is encoded as binary (up or down) shifts in one or both the 
			  // chrominance channels (Cb and Cr) for the given column (actually a set 
			  // of Steg_Encode_Col_Step columns).
			  // 26 letters in the English alphabet - therefore, we need a 5-bit char space (32 max distinct chars).
			  // The max bits and therefore letters we can encode into an image depends on its 
			  // pixel width and the Steg_Encode_Row_Step value, e.g. 30 shifts = 30 bits = 30/5 = 6 chars
			  // 'a' has an index of 1 and z has an index of 26. '~' has an index of 30. 
			  // The bigger the Steg_Encode_Row_Step is, the more resistant to image change and attacks the 
			  // steganographic encoding is. However, the bigger Steg_Encode_Row_Step is the less the
			  // information that can be encoded into the image.
			  // Chroma shift should preferably be done after watermark overlay is done.
			  
			  if (Steg_Encode_Col_Chroma_Shift_On && width >= height)
			  {
			    int bitSlen = strlen(bitS);
			    int topLen  = 28; //4chars x 7bits
			    int stepCol = 0;
			    
			    if (Steg_Encode_Anti_Crop_On)
			    { 
			      stepCol = (int)round((float)Steg_Anti_Crop_Frame_Width + 
			                Steg_Encode_Col_Step * (float)(curColStep + 2)); //next step col
			      
			      //if (globRow == 0 && col == 0) anticropFile = fopen("/TEST/anticrop.txt", "w");
			      //if (!globRow && !col) testFile = fopen("/TEST/test.txt", "w");
			      
			      if (col == 0 || col == width - Steg_Anti_Crop_First_Col)
			      {
				Steg_Encode_Col_Bit_Value = 2;
			      }
			      
			      if ( col == Steg_Anti_Crop_First_Col + (int)round(Steg_Encode_Col_Step) )
			      {
				bitSptr1 = bitS;
			      
				if (bitSlen > topLen)
				  bitSptr2 = bitS + topLen;
				else
				  bitSptr2 = bitS;
				
				Steg_Encode_Col_Bit_Value = 2;
			      }			      
			      
			      //encode two col anchors at step distance
			      if ( col >= Steg_Anti_Crop_First_Col + (int)round(Steg_Encode_Col_Step)     &&
				   col <  Steg_Anti_Crop_First_Col + 2 * (int)round(Steg_Encode_Col_Step)    
				 )
			      {				
				
				if ( col == Steg_Anti_Crop_First_Col + (int)round(Steg_Encode_Col_Step) && 
				     !(globRow % 50) 
				   )
				{
				  //fprintf(testFile, "Flag switching at globRow: %d\n", globRow);
				  Steg_Anti_Crop_Anchor_Flag = !Steg_Anti_Crop_Anchor_Flag;
				}
				
			        if (!Steg_Anti_Crop_Anchor_Flag)
				{
				  outptr1[col] += Cb_Shift + 1;
				  outptr2[col] += Cr_Shift + 1;
				}
				else
				{
				  outptr1[col] -= Cb_Shift + 1;
				  outptr2[col] -= Cr_Shift + 1;				  
				}
			      }
			      else if ( col >= Steg_Anti_Crop_First_Col + 2 * (int)round(Steg_Encode_Col_Step) &&
				        col < width - Steg_Anti_Crop_First_Col &&
				        !(col % stepCol) 
				      )
			      {
				if (curColStep % 2)
				{
				  if (globRow < cinfo->image_height / 2)
				  {
				    //fprintf(testFile, "bitS     = %s\nbitSptr1 = %s\n----\n", bitS, bitSptr1);
				    
				    if (*bitSptr1 == '\0' || bitSptr1 - bitS >= topLen) 
				      bitSptr1 = bitS;
				  
				    if(*bitSptr1 == '0')
				      Steg_Encode_Col_Bit_Value = 0;
				    else if (*bitSptr1 == '1')
				      Steg_Encode_Col_Bit_Value = 1;
				    
				    bitSptr1++;
				  }
				  else
				  {
				    if (*bitSptr2 == '\0')
				    {
				      if (bitSlen > topLen) 
					bitSptr2 = bitS + topLen;
				      else
					bitSptr2 = bitS;
				    }
				  
				    if(*bitSptr2 == '0') 
				      Steg_Encode_Col_Bit_Value = 0;
				    else if (*bitSptr2 == '1')
				      Steg_Encode_Col_Bit_Value = 1;
				    
				    bitSptr2++;			
				  }
				}
				else
				{
				  Steg_Encode_Col_Bit_Value = 2;
				}				  
				  
				curColStep++;
			      }		      
			      
			      //if (anticropFile && globRow == height - 1 && col == width - 1) fclose(anticropFile);
			      //if (testFile && globRow == height - 1 && col == width - 1) fclose(testFile);
			      
			    }
			    else //no anti-crop
			    {	
			      
			      stepCol = (int)round((float)Steg_Encode_Col_Step * (float)curColStep); //next step col
			      //int stepRow = (int)round(Steg_Encode_Row_Step * curRowStep); //next step row			    
			      
			      if (col == 0)
			      {
				bitSptr1 = bitS;
				
				if (bitSlen > topLen)
				  bitSptr2 = bitS + topLen;
				else
				  bitSptr2 = bitS;			      
				
				Steg_Encode_Col_Bit_Value = 2;
			      }
			      //else if (!(col % Steg_Encode_Col_Step))
			      else if ( !(col % stepCol) )
			      {
				if (curColStep % 2)
				{
				  if (globRow < cinfo->image_height / 2)
				  {
				    if (*bitSptr1 == '\0' || bitSptr1 - bitS >= topLen) 
				      bitSptr1 = bitS;
				  
				    if(*bitSptr1 == '0')
				      Steg_Encode_Col_Bit_Value = 0;
				    else if (*bitSptr1 == '1')
				      Steg_Encode_Col_Bit_Value = 1;
				    
				    bitSptr1++;
				  }
				  else
				  {
				    if (*bitSptr2 == '\0')
				    {
				      if (bitSlen > topLen) 
					bitSptr2 = bitS + topLen;
				      else
					bitSptr2 = bitS;
				    }
				  
				    if(*bitSptr2 == '0') 
				      Steg_Encode_Col_Bit_Value = 0;
				    else if (*bitSptr2 == '1')
				      Steg_Encode_Col_Bit_Value = 1;
				    
				    bitSptr2++;			
				  }
				}
				else
				{
				  Steg_Encode_Col_Bit_Value = 2;
				}
				
				curColStep++;
			      }			    
			      
			    }
			    
			    ////////////////////////////////
			    
			    //Here we do the actual chroma shifting of the pixel being processed
			    if (Steg_Encode_Col_Bit_Value == 0)
			    {
			      //if(outptr1[col] - Cb_Shift >= 0 && outptr2[col] - Cr_Shift >= 0)
			      //{
				outptr1[col] -= Cb_Shift;
				outptr2[col] -= Cr_Shift;
			      //}
			    }
			    else if (Steg_Encode_Col_Bit_Value == 1)
			    {
			      //if(outptr1[col] + Cb_Shift <= 255 && outptr2[col] + Cr_Shift <= 255)
			      //{			      
				outptr1[col] += Cb_Shift;
				outptr2[col] += Cr_Shift;
			      //}
			    }
			    else if (Steg_Encode_Col_Bit_Value == 3) //anchor bar
			    {
			      //if(outptr1[col] + Cb_Shift <= 255 && outptr2[col] - Cr_Shift >= 0)
			      //{			      
				outptr1[col] += Cb_Shift;
				//outptr1[col] -= Cb_Shift;
				
				outptr2[col] -= Cr_Shift;
				//outptr2[col] += Cr_Shift;
			      //}
			    }			    

			  }
			  else if (Steg_Encode_Col_Chroma_Shift_On)
			  {
			    int bitSlen = strlen(bitS);
			    int topLen  = 28; //4chars x 7bits
			    //int stepRow = 0;
			    
			    if (Steg_Encode_Anti_Crop_On)
			    { 
			      stepRow = (int)round((float)Steg_Anti_Crop_Frame_Width + 
			                Steg_Encode_Row_Step * (float)(curRowStep + 2)); //next step col
			      
			      //if (globRow == 0 && col == 0) anticropFile = fopen("/TEST/anticrop.txt", "w");
			      //if (!globRow && !col) testFile = fopen("/TEST/test.txt", "w");
			      
			      //if (globRow == stepRow)
				//fprintf(testFile, "globRow is %d, col is %d, stepRow is %d\n", globRow, col, stepRow);
			      
			      if (globRow == 0 || globRow == height - Steg_Anti_Crop_First_Row)
			      {
				Steg_Encode_Row_Bit_Value1 = 2;
				Steg_Encode_Row_Bit_Value2 = 2;
			      }
			      
			      //if ( globRow == Steg_Anti_Crop_First_Row + (int)round(Steg_Encode_Row_Step) )
			      if (globRow == 0)
			      {
				bitSptr1 = bitS;
			      
				if (bitSlen > topLen)
				  bitSptr2 = bitS + topLen;
				else
				  bitSptr2 = bitS;
				
				//Steg_Encode_Row_Bit_Value = 2;
			      }
			      
			      if (col == 0)
			      {
				Steg_Anti_Crop_Anchor_Flag2 = 0;
			      }
			      
			      //encode two col anchors at step distance
			      if ( globRow >= Steg_Anti_Crop_First_Row + (int)round(Steg_Encode_Row_Step)     &&
				   globRow <  Steg_Anti_Crop_First_Row + 2 * (int)round(Steg_Encode_Row_Step)
				 )
			      {		
				
				
				//if ( globRow == Steg_Anti_Crop_First_Row + (int)round(Steg_Encode_Row_Step) && !(col % 50) )
				if ( !(col % 50) )				  
				{
				  Steg_Anti_Crop_Anchor_Flag2 = !Steg_Anti_Crop_Anchor_Flag2;
				  //fprintf(testFile, "Flag switching at globRow: %d, col: %d and new flag value is: %d\n", 
					            //globRow, col, Steg_Anti_Crop_Anchor_Flag2);				  
				}
				
			        if (!Steg_Anti_Crop_Anchor_Flag2)
				{
			          //fprintf(testFile, "Setting pixel values for negative flag\n");		  
				  outptr1[col] += Cb_Shift + 1;
				  outptr2[col] += Cr_Shift + 1;
				}
				else
				{
				  //fprintf(testFile, "Setting pixel values for positive flag\n");
				  outptr1[col] -= Cb_Shift + 1;
				  outptr2[col] -= Cr_Shift + 1;
				}
							    
				
				Steg_Encode_Row_Bit_Value1 = 2;
				Steg_Encode_Row_Bit_Value2 = 2;
			      }
			  
			      else if ( globRow >= Steg_Anti_Crop_First_Row + 2 * (int)round(Steg_Encode_Row_Step) &&
				        globRow < height - Steg_Anti_Crop_First_Row && 
				        !(globRow % stepRow) 
				      )
			      {
				//fprintf(testFile, "inside for globRow: %d, col: %d, stepRow: %d\n", globRow, col, stepRow);

				if (curRowStep % 2)
				{
				  
				    //fprintf(testFile, "setting for globRow: %d, col: %d, stepRow: %d\n", globRow, col, stepRow);
				    
				    
				    if (*bitSptr1 == '\0' || bitSptr1 - bitS >= topLen) 
				      bitSptr1 = bitS;
				  
				    if(*bitSptr1 == '0')
				      Steg_Encode_Row_Bit_Value1 = 0;
				    else if (*bitSptr1 == '1')
				      Steg_Encode_Row_Bit_Value1 = 1;
				    
				    bitSptr1++;	
				    
				    //Steg_Encode_Row_Bit_Value1 = 0;

				    
				    if (*bitSptr2 == '\0')
				    {
				      if (bitSlen > topLen) 
					bitSptr2 = bitS + topLen;
				      else
					bitSptr2 = bitS;
				    }
				  
				    if(*bitSptr2 == '0') 
				      Steg_Encode_Row_Bit_Value2 = 0;
				    else if (*bitSptr2 == '1')
				      Steg_Encode_Row_Bit_Value2 = 1;
				    
				    bitSptr2++;	
				    
				    //Steg_Encode_Row_Bit_Value2 = 1;
				  }
				  else
				  {
				    Steg_Encode_Row_Bit_Value1 = 2;
				    Steg_Encode_Row_Bit_Value2 = 2;				    
				  }
				  
				  //if (col == width - 1) 
				  curRowStep++;				  				  
				}
				else
				{
				  //Steg_Encode_Row_Bit_Value1 = 2;
				  //Steg_Encode_Row_Bit_Value2 = 2;
				}	
				  
			      }		      
			      else //no anti-crop
			      {	
				stepRow = (int)round((float)Steg_Encode_Row_Step * (float)curRowStep); //next step col		    
				
				if (globRow == 0)
				{
				  bitSptr1 = bitS;
				  
				  if (bitSlen > topLen)
				    bitSptr2 = bitS + topLen;
				  else
				    bitSptr2 = bitS;			      
				  
				  Steg_Encode_Row_Bit_Value1 = 2;
				  Steg_Encode_Row_Bit_Value2 = 2;				  
				}
				else if ( !(globRow % stepRow) )
				{
				  if (curRowStep % 2)
				  {
				    if (*bitSptr1 == '\0' || bitSptr1 - bitS >= topLen) 
				      bitSptr1 = bitS;
				  
				    if(*bitSptr1 == '0')
				      Steg_Encode_Row_Bit_Value1 = 0;
				    else if (*bitSptr1 == '1')
				      Steg_Encode_Row_Bit_Value1 = 1;
				    
				    bitSptr1++;
				    
				    

				    if (*bitSptr2 == '\0')
				    {
				      if (bitSlen > topLen) 
					bitSptr2 = bitS + topLen;
				      else
					bitSptr2 = bitS;
				    }
				  
				    if(*bitSptr2 == '0') 
				      Steg_Encode_Row_Bit_Value2 = 0;
				    else if (*bitSptr2 == '1')
				      Steg_Encode_Row_Bit_Value2 = 1;
				    
				    bitSptr2++;			
				  }
				  else
				  {
				    Steg_Encode_Row_Bit_Value1 = 2;
				    Steg_Encode_Row_Bit_Value2 = 2;				    
				  }
				  
				  curRowStep++;
				}			    
				
			      }
			      
			      //if (anticropFile && globRow == height - 1 && col == width - 1) fclose(anticropFile);
			      //if (testFile && globRow == height - 1 && col == width - 1) fclose(testFile);			    
			    
			      ////////////////////////////////
			      
			      //Here we do the actual chroma shifting of the pixel being processed
			      if (Steg_Encode_Row_Bit_Value1 == 0 && col < width / 2)
			      {
				  outptr1[col] -= Cb_Shift;
				  outptr2[col] -= Cr_Shift;
			      }
			      
			      if (Steg_Encode_Row_Bit_Value1 == 1 && col < width / 2)
			      {			      
				  outptr1[col] += Cb_Shift;
				  outptr2[col] += Cr_Shift;
			      }
			      
			      if (Steg_Encode_Row_Bit_Value2 == 0 && col >= width / 2)
			      {
				  outptr1[col] -= Cb_Shift;
				  outptr2[col] -= Cr_Shift;
			      }
			      
			      if (Steg_Encode_Row_Bit_Value2 == 1 && col >= width / 2)
			      {			      
				outptr1[col] += Cb_Shift;
				outptr2[col] += Cr_Shift;
			      }
			    

			  }			  
			  
			  //FOTIOS
			  //fprintf(ivalFile, "Y = %d - Cb = %d - Cr = %d\n", outptr0[col], outptr1[col], outptr0[col]);
			  //fprintf(ivalFile, "Cb_Shift = %d - Cr_Shift = %d\n", Cb_Shift, Cr_Shift);
  #else
 
			  /*
			  * Y 
			  */
			  
			  outptr0[col] = (JSAMPLE)
			      ((ctab[r + R_Y_OFF] + ctab[g + G_Y_OFF] +
				ctab[b + B_Y_OFF])
			      >> SCALEBITS);
			  
			  /*
			  * Cb 
			  */
			  
			  outptr1[col] = (JSAMPLE)
			      ((ctab[r + R_CB_OFF] + ctab[g + G_CB_OFF] +
				ctab[b + B_CB_OFF])
			      >> SCALEBITS);
			  
			  /*
			  * Cr 
			  */
			  
			  outptr2[col] = (JSAMPLE)
			      ((ctab[r + R_CR_OFF] + ctab[g + G_CR_OFF] +
				ctab[b + B_CR_OFF])
			      >> SCALEBITS);
			  
  #endif
		  } //col for
	  } //row for
	  
       //fclose(ivalFile);
       
       //if (bitS) free(bitS);
       //bitS = NULL;
       //bitSptr = NULL;
       
       //free(leftBitString);
       //free(rightBitString);
       
       //leftBitString_ptr  = NULL;
       //rightBitString_ptr = NULL;
     }
     else //perform steganographic image decoding ////////////////////////////////////////////////////////////////////////////
     {
	//allocate chroma spike array only once
	//static short spike[2048][2048];
	
	int* spikePtr = NULL;
	
	if (width >= height)
	{
	  
	  //ROW PROCESSING
	  for (row = 0; row < rows_to_read; row++, globRow++) 
	  {
		  /*
		  * Read one row from the source file 
		  */
		  (*cinfo->methods->get_input_row) (cinfo, pixel_row);
		  cinfo->color_row_idx++;
		  
		  /*
		  * Convert colorspace 
		  */
		  inptr0 = pixel_row[0];
		  inptr1 = pixel_row[1];
		  inptr2 = pixel_row[2];
		  
		  //2D ptr assignments to proper positions in the 3D image matrix
		  outptr0 = image_data[0][row];
		  outptr1 = image_data[1][row];
		  outptr2 = image_data[2][row];
		  
		  //COLUMN PROCESSING
		  for (col = 0; col < width; col++) 
		  {
			  r = GETJSAMPLE(inptr0[col]);
			  g = GETJSAMPLE(inptr1[col]);
			  b = GETJSAMPLE(inptr2[col]);
			  
			  //FOTIOS
			  //fprintf(ivalFile, "r = %d - g = %d - b = %d --- ", r, g, b);
			  //fprintf(ivalFile, "Cb_Shift = %d - Cr_Shift = %d --- ", Cb_Shift, Cr_Shift);

  #ifdef STEG_SUPPORTED
			  /*
			  * If the inputs are 0..MAXJSAMPLE, the outputs of
			  * these equations must be too; we do not need an
			  * explicit range-limiting operation. Hence the value
			  * being shifted is never negative, and we don't need
			  * the general RIGHT_SHIFT macro. 
			  */

			  /*
			  * Y 
			  */
			  outptr0[col] = (JSAMPLE)
			      ((ctab[r + R_Y_OFF] + ctab[g + G_Y_OFF] +
				ctab[b + B_Y_OFF])
			      >> SCALEBITS);
			  /*
			  * Cb 
			  */
			  outptr1[col] = (JSAMPLE)
			      ((ctab[r + R_CB_OFF] + ctab[g + G_CB_OFF] +
				ctab[b + B_CB_OFF])
			      >> SCALEBITS);
			  /*
			  * Cr 
			  */
			  outptr2[col] = (JSAMPLE)
			      ((ctab[r + R_CR_OFF] + ctab[g + G_CR_OFF] +
				ctab[b + B_CR_OFF])
			      >> SCALEBITS);
			     
			      
                          
			  //Decoder 10: detects pixels with same Y and both Cb and Cr upshifted or downshifted
			  //            and also supports custom and independent Cb and Cr shift values
			  //            as well as bound based checking.
			  //            Additionally, it detects Cb-upshifted & Cr-downshifted anchor pixels.
			    if (!cinfo->Steg_Decode_Ignore_Y)
			    {
			      if (col > 0)
			      {
				if (!Steg_Decode_Specific_Diff)
				{
				  if      ( abs(outptr0[col] - outptr0[col-1]) <= cinfo->Steg_Decode_Y_Bound &&
					    outptr1[col] >  outptr1[col-1] &&
					    outptr2[col] >  outptr2[col-1]    
					  )
				  {
				    spike[globRow][col] = 1; //it's a chroma spike up
				  }
				  else if ( abs(outptr0[col] - outptr0[col-1]) <= cinfo->Steg_Decode_Y_Bound &&
					    outptr1[col] <  outptr1[col-1] &&
					    outptr2[col] <  outptr2[col-1]    
					  )
				  {
				    spike[globRow][col] = 2; //it's a chroma spike down
				  }
				  else if ( abs(outptr0[col] - outptr0[col-1]) <= cinfo->Steg_Decode_Y_Bound &&
					    outptr1[col] >  outptr1[col-1] &&
					    outptr2[col] <  outptr2[col-1]    
					  )	
				  {
				    spike[globRow][col] = 3; //it's an anchor1 spike
				  }
				  else if ( abs(outptr0[col] - outptr0[col-1]) <= cinfo->Steg_Decode_Y_Bound &&
					    outptr1[col] <  outptr1[col-1] &&
					    outptr2[col] >  outptr2[col-1]    
					  )
				  {
				    spike[globRow][col] = 4; //it's an anchor2 spike
				  }			      
				  else
				  {
				    spike[globRow][col] = 0; //no chroma spike
				  }
				}
				else
				{
				  if      ( abs(outptr0[col] - outptr0[col-1]) <= cinfo->Steg_Decode_Y_Bound &&
					    outptr1[col] >  outptr1[col-1]                                   &&
					    outptr1[col] <  outptr1[col-1] + Cb_Shift + Steg_Decode_Bound    &&
					    outptr2[col] >  outptr2[col-1]                                   &&
					    outptr2[col] <  outptr2[col-1] + Cr_Shift + Steg_Decode_Bound    
					  )
				  {
				    spike[globRow][col] = 1; //it's a chroma spike up
				  }
				  else if ( abs(outptr0[col] - outptr0[col-1]) <= cinfo->Steg_Decode_Y_Bound &&
					    outptr1[col] <  outptr1[col-1]                                   &&
					    outptr1[col] >  outptr1[col-1] - Cb_Shift - Steg_Decode_Bound    &&
					    outptr2[col] <  outptr2[col-1]                                   &&
					    outptr2[col] >  outptr2[col-1] - Cr_Shift - Steg_Decode_Bound    
					  )
				  {
				    spike[globRow][col] = 2; //it's a chroma spike down
				  }
				  else if ( abs(outptr0[col] - outptr0[col-1]) <= cinfo->Steg_Decode_Y_Bound &&
					    outptr1[col] >  outptr1[col-1]                                   &&
					    outptr1[col] <  outptr1[col-1] + Cb_Shift + Steg_Decode_Bound    &&
					    outptr2[col] <  outptr2[col-1]                                   &&
					    outptr2[col] >  outptr2[col-1] - Cr_Shift - Steg_Decode_Bound    
					  )	
				  {
				    spike[globRow][col] = 3; //it's an anchor spike 1
				  }
				  else if ( abs(outptr0[col] - outptr0[col-1]) <= cinfo->Steg_Decode_Y_Bound &&
					    outptr1[col] <  outptr1[col-1]                                   &&
					    outptr1[col] >  outptr1[col-1] - Cb_Shift - Steg_Decode_Bound    &&
					    outptr2[col] >  outptr2[col-1]                                   &&
					    outptr2[col] <  outptr2[col-1] + Cr_Shift + Steg_Decode_Bound    
					  )	
				  {
				    spike[globRow][col] = 4; //it's an anchor spike 2
				  }			      
				  else
				  {
				    spike[globRow][col] = 0; //no chroma spike
				  }
				  
				}
			      }
			    }
			    else //Y is ignored
			    {
			      if (col > 0)
			      {
				if (!Steg_Decode_Specific_Diff)
				{
				  if      ( outptr1[col] >  outptr1[col-1] &&
					    outptr2[col] >  outptr2[col-1]    
					  )
				  {
				    spike[globRow][col] = 1; //it's a chroma spike up
				  }
				  else if ( outptr1[col] <  outptr1[col-1] &&
					    outptr2[col] <  outptr2[col-1]    
					  )
				  {
				    spike[globRow][col] = 2; //it's a chroma spike down
				  }
				  else if ( outptr1[col] >  outptr1[col-1] &&
					    outptr2[col] <  outptr2[col-1]    
					  )	
				  {
				    spike[globRow][col] = 3; //it's an anchor1 spike
				  }
				  else if ( outptr1[col] <  outptr1[col-1] &&
					    outptr2[col] >  outptr2[col-1]    
					  )
				  {
				    spike[globRow][col] = 4; //it's an anchor2 spike
				  }			      
				  else
				  {
				    spike[globRow][col] = 0; //no chroma spike
				  }
				}
				else
				{
				  if      ( outptr1[col] >  outptr1[col-1]                                &&
					    outptr1[col] <  outptr1[col-1] + Cb_Shift + Steg_Decode_Bound &&
					    outptr2[col] >  outptr2[col-1]                                &&
					    outptr2[col] <  outptr2[col-1] + Cr_Shift + Steg_Decode_Bound    
					  )
				  {
				    spike[globRow][col] = 1; //it's a chroma spike up
				  }
				  else if ( outptr1[col] <  outptr1[col-1]                                &&
					    outptr1[col] >  outptr1[col-1] - Cb_Shift - Steg_Decode_Bound &&
					    outptr2[col] <  outptr2[col-1]                                &&
					    outptr2[col] >  outptr2[col-1] - Cr_Shift - Steg_Decode_Bound    
					  )
				  {
				    spike[globRow][col] = 2; //it's a chroma spike down
				  }
				  else if ( outptr1[col] >  outptr1[col-1]                                &&
					    outptr1[col] <  outptr1[col-1] + Cb_Shift + Steg_Decode_Bound &&
					    outptr2[col] <  outptr2[col-1]                                &&
					    outptr2[col] >  outptr2[col-1] - Cr_Shift - Steg_Decode_Bound    
					  )	
				  {
				    spike[globRow][col] = 3; //it's an anchor spike 1
				  }
				  else if ( outptr1[col] <  outptr1[col-1]                                &&
					    outptr1[col] >  outptr1[col-1] - Cb_Shift - Steg_Decode_Bound &&
					    outptr2[col] >  outptr2[col-1]                                &&
					    outptr2[col] <  outptr2[col-1] + Cr_Shift + Steg_Decode_Bound    
					  )	
				  {
				    spike[globRow][col] = 4; //it's an anchor spike 2
				  }			      
				  else
				  {
				    spike[globRow][col] = 0; //no chroma spike
				  }				
				}
			      }			    
			    }
			    
			    
			    //Now mark the chroma spikes detected in the current pixel row
			    if (col == width - 1)
			    {
			      
			      int i = 1;
			      int j = 0;
			      if (!cinfo->Steg_Decode_Anti_Crop_On)
			      {
				while (j < width) //add magenta guide lines
				{				
				  j = (int)round((Steg_Encode_Col_Step * (float)i++));
				  
				  outptr0[j] = 255;
				  outptr1[j] = 255;
				  outptr2[j] = 255;				      
				}
			      }
			      
			      for (int i = 0; i < width; i++)
			      {
				if (spike[globRow][i] == 1) //make upshifted pixels white
				{
				  outptr0[i] = 255;
				  outptr1[i] = 128;
				  outptr2[i] = 128;  
				}
				else if (spike[globRow][i] == 2) //make downshifted pixels black
				{
				  outptr0[i] = 0;
				  outptr1[i] = 128;
				  outptr2[i] = 128; 				  
				}
				else if (spike[globRow][i] == 3) //make anchor 1 spike pixels magenta
				{
				  outptr0[i] = 255;
				  outptr1[i] = 255;
				  outptr2[i] = 255;				  
				}
				else if (spike[globRow][i] == 4) //make anchor 2 spike pixels magenta
				{
				  outptr0[i] = 0;
				  outptr1[i] = 0;
				  outptr2[i] = 0;				  
				}
				
				//leave unshifted pixels unchanged
			      }
			    }

  #else

  #endif
			      
		  } //columns for
		  
		  //if (globRow == 200) break;
		  
	  } //rows for
	}
	else //width < height
	{
	  
	  //ROW PROCESSING
	  for (row = 0; row < rows_to_read; row++, globRow++) 
	  {
		  //if (globRow == 0) last_pixel_row = (*cinfo->emethods->alloc_small_sarray)
							//(cinfo->image_width, (long) cinfo->input_components);	 
							
		  if (globRow == 0)
		  {
		    last_pixel_row0 = (JSAMPROW) malloc (width);
		    last_pixel_row1 = (JSAMPROW) malloc (width);
		    last_pixel_row2 = (JSAMPROW) malloc (width);
		  }

		  //(*cinfo->methods->get_input_row) (cinfo, last_pixel_row);		  
		  (*cinfo->methods->get_input_row) (cinfo, pixel_row);
		  //cinfo->color_row_idx += 2;
		  cinfo->color_row_idx++;

		  inptr0 = pixel_row[0];
		  inptr1 = pixel_row[1];
		  inptr2 = pixel_row[2];
		  
		  //2D ptr assignments to proper positions in the 3D image matrix
		  outptr0 = image_data[0][row];
		  outptr1 = image_data[1][row];
		  outptr2 = image_data[2][row];
		  
		  if (globRow > 0)
		  {

		    last_inptr0 = last_pixel_row0;
		    last_inptr1 = last_pixel_row1;
		    last_inptr2 = last_pixel_row2; 
		    
		    //2D ptr assignments to proper positions in the 3D image matrix
		    /*
		    last_outptr0 = image_data[0][row-1];
		    last_outptr1 = image_data[1][row-1];
		    last_outptr2 = image_data[2][row-1];		
		    */
		  } 
		  
		  //COLUMN PROCESSING
		  for (col = 0; col < width; col++) 
		  {
			  r = GETJSAMPLE(inptr0[col]);
			  g = GETJSAMPLE(inptr1[col]);
			  b = GETJSAMPLE(inptr2[col]);
			  
			  //FOTIOS
			  //fprintf(ivalFile, "r = %d - g = %d - b = %d --- ", r, g, b);
			  //fprintf(ivalFile, "Cb_Shift = %d - Cr_Shift = %d --- ", Cb_Shift, Cr_Shift);

  #ifdef STEG_SUPPORTED

			  outptr0[col] = (JSAMPLE)
			      ((ctab[r + R_Y_OFF] + ctab[g + G_Y_OFF] +
				ctab[b + B_Y_OFF])
			      >> SCALEBITS);

			  outptr1[col] = (JSAMPLE)
			      ((ctab[r + R_CB_OFF] + ctab[g + G_CB_OFF] +
				ctab[b + B_CB_OFF])
			      >> SCALEBITS);

			  outptr2[col] = (JSAMPLE)
			      ((ctab[r + R_CR_OFF] + ctab[g + G_CR_OFF] +
				ctab[b + B_CR_OFF])
			      >> SCALEBITS);
			      
			  //////////////////////////////////////////////////////
			  if (globRow > 0)
			  {
			    r = GETJSAMPLE(last_inptr0[col]);
			    g = GETJSAMPLE(last_inptr1[col]);
			    b = GETJSAMPLE(last_inptr2[col]);
			    
			    //FOTIOS
			    //fprintf(ivalFile, "r = %d - g = %d - b = %d --- ", r, g, b);
			    //fprintf(ivalFile, "Cb_Shift = %d - Cr_Shift = %d --- ", Cb_Shift, Cr_Shift);
			    
			    /*
			    last_outptr0[col] = (JSAMPLE)
				((ctab[r + R_Y_OFF] + ctab[g + G_Y_OFF] +
				  ctab[b + B_Y_OFF])
				>> SCALEBITS);

			    last_outptr1[col] = (JSAMPLE)
				((ctab[r + R_CB_OFF] + ctab[g + G_CB_OFF] +
				  ctab[b + B_CB_OFF])
				>> SCALEBITS);

			    last_outptr2[col] = (JSAMPLE)
				((ctab[r + R_CR_OFF] + ctab[g + G_CR_OFF] +
				  ctab[b + B_CR_OFF])
				>> SCALEBITS);	
			    */
			  
			    last_outptr0 = (JSAMPLE)
				((ctab[r + R_Y_OFF] + ctab[g + G_Y_OFF] +
				  ctab[b + B_Y_OFF])
				>> SCALEBITS);

			    last_outptr1 = (JSAMPLE)
				((ctab[r + R_CB_OFF] + ctab[g + G_CB_OFF] +
				  ctab[b + B_CB_OFF])
				>> SCALEBITS);

			    last_outptr2 = (JSAMPLE)
				((ctab[r + R_CR_OFF] + ctab[g + G_CR_OFF] +
				  ctab[b + B_CR_OFF])
				>> SCALEBITS);				    
			  } 	
		  
			  ////////////////////////////////////////////////////////////////////////////////
			  
			  if (!cinfo->Steg_Decode_Ignore_Y)
			  {
			    if (globRow > 0)
			    {
			      if (!Steg_Decode_Specific_Diff)
			      {
				if      ( abs(outptr0[col] - last_outptr0) <= cinfo->Steg_Decode_Y_Bound &&
					  outptr1[col] >  last_outptr1 &&
					  outptr2[col] >  last_outptr2    
					)
				{
				  spike[globRow][col] = 1; //it's a chroma spike up
				}
				else if ( abs(outptr0[col] - last_outptr0) <= cinfo->Steg_Decode_Y_Bound &&
					  outptr1[col] <  last_outptr1 &&
					  outptr2[col] <  last_outptr2    
					)
				{
				  spike[globRow][col] = 2; //it's a chroma spike down
				}
				else if ( abs(outptr0[col] - last_outptr0) <= cinfo->Steg_Decode_Y_Bound &&
					  outptr1[col] >  last_outptr1 &&
					  outptr2[col] <  last_outptr2    
					)	
				{
				  spike[globRow][col] = 3; //it's an anchor1 spike
				}
				else if ( abs(outptr0[col] - last_outptr0) <= cinfo->Steg_Decode_Y_Bound &&
					  outptr1[col] <  last_outptr1 &&
					  outptr2[col] >  last_outptr2    
					)
				{
				  spike[globRow][col] = 4; //it's an anchor2 spike
				}			      
				else
				{
				  spike[globRow][col] = 0; //no chroma spike
				}
			      }
			      else
			      {
				if      ( abs(outptr0[col] - last_outptr0) <= cinfo->Steg_Decode_Y_Bound &&
					  outptr1[col] >  last_outptr1                                   &&
					  outptr1[col] <  last_outptr1 + Cb_Shift + Steg_Decode_Bound    &&
					  outptr2[col] >  last_outptr2                                   &&
					  outptr2[col] <  last_outptr2 + Cr_Shift + Steg_Decode_Bound    
					)
				{
				  spike[globRow][col] = 1; //it's a chroma spike up
				}
				else if ( abs(outptr0[col] - last_outptr0) <= cinfo->Steg_Decode_Y_Bound &&
					  outptr1[col] <  last_outptr1                                   &&
					  outptr1[col] >  last_outptr1 - Cb_Shift - Steg_Decode_Bound    &&
					  outptr2[col] <  last_outptr2                                   &&
					  outptr2[col] >  last_outptr2 - Cr_Shift - Steg_Decode_Bound    
					)
				{
				  spike[globRow][col] = 2; //it's a chroma spike down
				}
				else if ( abs(outptr0[col] - last_outptr0) <= cinfo->Steg_Decode_Y_Bound &&
					  outptr1[col] >  last_outptr1                                   &&
					  outptr1[col] <  last_outptr1 + Cb_Shift + Steg_Decode_Bound    &&
					  outptr2[col] <  last_outptr2                                   &&
					  outptr2[col] >  last_outptr2 - Cr_Shift - Steg_Decode_Bound    
					)	
				{
				  spike[globRow][col] = 3; //it's an anchor spike 1
				}
				else if ( abs(outptr0[col] - last_outptr0) <= cinfo->Steg_Decode_Y_Bound &&
					  outptr1[col] <  last_outptr1                                   &&
					  outptr1[col] >  last_outptr1 - Cb_Shift - Steg_Decode_Bound    &&
					  outptr2[col] >  last_outptr2                                   &&
					  outptr2[col] <  last_outptr2 + Cr_Shift + Steg_Decode_Bound    
					)	
				{
				  spike[globRow][col] = 4; //it's an anchor spike 2
				}			      
				else
				{
				  spike[globRow][col] = 0; //no chroma spike
				}
				
			      }
			    }
			  }
			  else //Y is ignored
			  {
			    if (globRow > 0)
			    {
			      if (!Steg_Decode_Specific_Diff)
			      {
				if      ( outptr1[col] >  last_outptr1 &&
					  outptr2[col] >  last_outptr2    
					)
				{
				  spike[globRow][col] = 1; //it's a chroma spike up
				}
				else if ( outptr1[col] <  last_outptr1 &&
					  outptr2[col] <  last_outptr2    
					)
				{
				  spike[globRow][col] = 2; //it's a chroma spike down
				}
				else if ( outptr1[col] >  last_outptr1 &&
					  outptr2[col] <  last_outptr2    
					)	
				{
				  spike[globRow][col] = 3; //it's an anchor1 spike
				}
				else if ( outptr1[col] <  last_outptr1 &&
					  outptr2[col] >  last_outptr2    
					)
				{
				  spike[globRow][col] = 4; //it's an anchor2 spike
				}			      
				else
				{
				  spike[globRow][col] = 0; //no chroma spike
				}
			      }
			      else
			      {
				if      ( outptr1[col] >  last_outptr1                                &&
					  outptr1[col] <  last_outptr1 + Cb_Shift + Steg_Decode_Bound &&
					  outptr2[col] >  last_outptr2                                &&
					  outptr2[col] <  last_outptr2 + Cr_Shift + Steg_Decode_Bound    
					)
				{
				  spike[globRow][col] = 1; //it's a chroma spike up
				}
				else if ( outptr1[col] <  last_outptr1                                &&
					  outptr1[col] >  last_outptr1 - Cb_Shift - Steg_Decode_Bound &&
					  outptr2[col] <  last_outptr2                                &&
					  outptr2[col] >  last_outptr2 - Cr_Shift - Steg_Decode_Bound    
					)
				{
				  spike[globRow][col] = 2; //it's a chroma spike down
				}
				else if ( outptr1[col] >  last_outptr1                                &&
					  outptr1[col] <  last_outptr1 + Cb_Shift + Steg_Decode_Bound &&
					  outptr2[col] <  last_outptr2                                &&
					  outptr2[col] >  last_outptr2 - Cr_Shift - Steg_Decode_Bound    
					)	
				{
				  spike[globRow][col] = 3; //it's an anchor spike 1
				}
				else if ( outptr1[col] <  last_outptr1                                &&
					  outptr1[col] >  last_outptr1 - Cb_Shift - Steg_Decode_Bound &&
					  outptr2[col] >  last_outptr2                                &&
					  outptr2[col] <  last_outptr2 + Cr_Shift + Steg_Decode_Bound    
					)	
				{
				  spike[globRow][col] = 4; //it's an anchor spike 2
				}			      
				else
				{
				  spike[globRow][col] = 0; //no chroma spike
				}				
			      }
			    }			    
			  }
			  
			  
			  //Now highlight the current row if it is a step row (write guide line)
			  if (col == width - 1)
			  {
			    int j = 0;
			    int i = 1;
			    int crStep = 0;
			    if (!cinfo->Steg_Decode_Anti_Crop_On)
			    {
			      while (j < height) //add magenta guide lines
			      {	
				j = (int)round((Steg_Encode_Row_Step * (float)i++));
				
				if (j == globRow)
				{
				  for (int i = 0; i < width; i++)
				  {
				    outptr0[i] = 255;
				    outptr1[i] = 255;
				    outptr2[i] = 255;
				  }
				}
			      }
			    }
			    
			    for (int i = 0; i < width; i++)
			    {
			      if (spike[globRow][i] == 1) //make upshifted pixels white
			      {
				outptr0[i] = 255;
				outptr1[i] = 128;
				outptr2[i] = 128;  
			      }
			      else if (spike[globRow][i] == 2) //make downshifted pixels black
			      {
				outptr0[i] = 0;
				outptr1[i] = 128;
				outptr2[i] = 128; 				  
			      }
			      else if (spike[globRow][i] == 3) //make anchor 1 spike pixels magenta
			      {
				outptr0[i] = 255;
				outptr1[i] = 255;
				outptr2[i] = 255;				  
			      }
			      else if (spike[globRow][i] == 4) //make anchor 2 spike pixels magenta
			      {
				outptr0[i] = 0;
				outptr1[i] = 0;
				outptr2[i] = 0;				  
			      }
			      
			      //leave unshifted pixels unchanged
			    }
			  }	
	                  
  #else

  #endif
 
		  }//col for
		  
		  //(*cinfo->methods->get_input_row) (cinfo, last_pixel_row);
		  //(*cinfo->methods->get_input_row) (cinfo, pixel_row);
		  //cinfo->color_row_idx++;
		  //if (globRow == height - 1) free(last_pixel_row);
		  
		  //copy pixel_row to last_pixel_row here
		  //long total_bytes_sarray = width * SIZEOF(JSAMPLE);// + MALLOC_FAR_OVERHEAD;
		  memcpy(last_pixel_row0, pixel_row[0], width * SIZEOF(JSAMPLE));
		  memcpy(last_pixel_row1, pixel_row[1], width * SIZEOF(JSAMPLE));
		  memcpy(last_pixel_row2, pixel_row[2], width * SIZEOF(JSAMPLE));
		  
		  if (globRow == height - 1)
		  {
		    free(last_pixel_row0);
		    free(last_pixel_row1);
		    free(last_pixel_row2);		    
		  }

	    }//row for
       
      }//width < height
         
  
  //#ifdef STEG_SUPPORTED
	  
  //#endif	  
	  
     }//else, steganographic decoding section end
     
     
     if (globRow == cinfo->image_height)
     {
	globRow = 0;
	curRowStep = 1;
	
        Steg_Encode_Col_Bit_Value = 2;
        rowEncodeEnabled = 0;
	
        //Steg_Encode_Col_Step = 0.0;
        //Steg_Encode_Row_Step = 0.0;

	if (ivalFile) fclose(ivalFile);
        ivalFile = NULL;	
	
	//FILE* ivalFile = fopen("/TEST/bits.txt", "w");
	//fprintf(ivalFile, "%s", bitS);
	//fclose(ivalFile);
	
        if (bitS)
	{
	  free(bitS);
	  bitS = NULL;
	  bitSptr = NULL;
	  bitSptr1 = NULL;
	  bitSptr2 = NULL;
	}
     }
     
     //fclose(ivalFile);
}



/**************** Cases other than RGB -> YCbCr **************/


/*
 * Fetch some rows of pixels from get_input_row and convert to the
 * JPEG colorspace.
 * This version handles RGB->grayscale conversion, which is the same
 * as the RGB->Y portion of RGB->YCbCr.
 * We assume rgb_ycc_init has been called (we only use the Y tables).
 */

METHODDEF void
get_rgb_gray_rows(compress_info_ptr cinfo,
		  int rows_to_read, JSAMPIMAGE image_data)
{
#ifdef SIXTEEN_BIT_SAMPLES
	register UINT16 r, g, b;
#else
	register int    r, g, b;
#endif
	register INT32 *ctab = rgb_ycc_tab;
	register JSAMPROW inptr0, inptr1, inptr2;
	register JSAMPROW outptr;
	register long   col;
	long            width = cinfo->image_width;
	int             row;

	for (row = 0; row < rows_to_read; row++) {
		/*
		 * Read one row from the source file 
		 */
		(*cinfo->methods->get_input_row) (cinfo, pixel_row);
		/*
		 * Convert colorspace 
		 */
		inptr0 = pixel_row[0];
		inptr1 = pixel_row[1];
		inptr2 = pixel_row[2];
		outptr = image_data[0][row];
		for (col = 0; col < width; col++) {
			r = GETJSAMPLE(inptr0[col]);
			g = GETJSAMPLE(inptr1[col]);
			b = GETJSAMPLE(inptr2[col]);
			/*
			 * If the inputs are 0..MAXJSAMPLE, the outputs of
			 * these equations must be too; we do not need an
			 * explicit range-limiting operation. Hence the value
			 * being shifted is never negative, and we don't need
			 * the general RIGHT_SHIFT macro. 
			 */
			/*
			 * Y 
			 */
			outptr[col] = (JSAMPLE)
			    ((ctab[r + R_Y_OFF] + ctab[g + G_Y_OFF] +
			      ctab[b + B_Y_OFF])
			     >> SCALEBITS);
		}
	}
}


/*
 * Initialize for colorspace conversion.
 */

METHODDEF void
colorin_init(compress_info_ptr cinfo)
{
	/*
	 * Allocate a workspace for the result of get_input_row. 
	 */
	pixel_row = (*cinfo->emethods->alloc_small_sarray)
	    (cinfo->image_width, (long) cinfo->input_components);
}


/*
 * Fetch some rows of pixels from get_input_row and convert to the
 * JPEG colorspace.
 * This version handles grayscale output with no conversion.
 * The source can be either plain grayscale or YCbCr (since Y == gray).
 */

METHODDEF void
get_grayscale_rows(compress_info_ptr cinfo,
		   int rows_to_read, JSAMPIMAGE image_data)
{
	int             row;

	for (row = 0; row < rows_to_read; row++) {
		/*
		 * Read one row from the source file 
		 */
		(*cinfo->methods->get_input_row) (cinfo, pixel_row);
		/*
		 * Convert colorspace (gamma mapping needed here) 
		 */
		jcopy_sample_rows(pixel_row, 0, image_data[0], row,
				  1, cinfo->image_width);
	}
}


/*
 * Fetch some rows of pixels from get_input_row and convert to the
 * JPEG colorspace.
 * This version handles multi-component colorspaces without conversion.
 */

METHODDEF void
get_noconvert_rows(compress_info_ptr cinfo,
		   int rows_to_read, JSAMPIMAGE image_data)
{
	int             row, ci;

	for (row = 0; row < rows_to_read; row++) {
		/*
		 * Read one row from the source file 
		 */
		(*cinfo->methods->get_input_row) (cinfo, pixel_row);
		/*
		 * Convert colorspace (gamma mapping needed here) 
		 */
		for (ci = 0; ci < cinfo->input_components; ci++) {
			jcopy_sample_rows(pixel_row, ci, image_data[ci], row,
					  1, cinfo->image_width);
		}
	}
}


/*
 * Finish up at the end of the file.
 */

METHODDEF void
colorin_term(compress_info_ptr cinfo)
{
	/*
	 * no work (we let free_all release the workspace) 
	 */
}


/*
 * The method selection routine for input colorspace conversion.
 */

GLOBAL void
jselccolor(compress_info_ptr cinfo)
{
	/*
	 * Make sure input_components agrees with in_color_space 
	 */
	switch (cinfo->in_color_space) {
	case CS_GRAYSCALE:
		if (cinfo->input_components != 1)
			ERREXIT(cinfo->emethods, "Bogus input colorspace");
		break;

	case CS_RGB:
	case CS_YCbCr:
	case CS_YIQ:
		if (cinfo->input_components != 3)
			ERREXIT(cinfo->emethods, "Bogus input colorspace");
		break;

	case CS_CMYK:
		if (cinfo->input_components != 4)
			ERREXIT(cinfo->emethods, "Bogus input colorspace");
		break;

	default:
		ERREXIT(cinfo->emethods, "Unsupported input colorspace");
		break;
	}

	/*
	 * Standard init/term methods (may override below) 
	 */
	cinfo->methods->colorin_init = colorin_init;
	cinfo->methods->colorin_term = colorin_term;

	/*
	 * Check num_components, set conversion method based on requested
	 * space 
	 */
	switch (cinfo->jpeg_color_space) {
	case CS_GRAYSCALE:
		if (cinfo->num_components != 1)
			ERREXIT(cinfo->emethods, "Bogus JPEG colorspace");
		if (cinfo->in_color_space == CS_GRAYSCALE)
			cinfo->methods->get_sample_rows = get_grayscale_rows;
		else if (cinfo->in_color_space == CS_RGB) {
			cinfo->methods->colorin_init = rgb_ycc_init;
			cinfo->methods->get_sample_rows = get_rgb_gray_rows;
		} else if (cinfo->in_color_space == CS_YCbCr)
			cinfo->methods->get_sample_rows = get_grayscale_rows;
		else
			ERREXIT(cinfo->emethods,
				"Unsupported color conversion request");
		break;

	case CS_YCbCr:
		if (cinfo->num_components != 3)
			ERREXIT(cinfo->emethods, "Bogus JPEG colorspace");
		if (cinfo->in_color_space == CS_RGB) {
			cinfo->methods->colorin_init = rgb_ycc_init;
			cinfo->methods->get_sample_rows = get_rgb_ycc_rows;
		} else if (cinfo->in_color_space == CS_YCbCr)
			cinfo->methods->get_sample_rows = get_noconvert_rows;
		else
			ERREXIT(cinfo->emethods,
				"Unsupported color conversion request");
		break;

	case CS_CMYK:
		if (cinfo->num_components != 4)
			ERREXIT(cinfo->emethods, "Bogus JPEG colorspace");
		if (cinfo->in_color_space == CS_CMYK)
			cinfo->methods->get_sample_rows = get_noconvert_rows;
		else
			ERREXIT(cinfo->emethods,
				"Unsupported color conversion request");
		break;

	default:
		ERREXIT(cinfo->emethods, "Unsupported JPEG colorspace");
		break;
	}
}
