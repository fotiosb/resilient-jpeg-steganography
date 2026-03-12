// steg.i

%module steganography2
%{
#include "interface.h"
%}

int     steg_easy_apply2(char  *message,
		         short watermark_font_size,
                         short Steg_Encode_Old_Overlay_On,
			 short Y_BUMP,
			 short Y_BUMP_REACTIVE,
	                 short Watermark_X_Step,
                         short Watermark_Y_Step,
			 short Steg_Encode_Anti_Crop_On,
			 short Steg_Anti_Crop_Percent,
			 short Steg_Encode_Col_Chroma_Shift_On,
			 short Steg_Encode_Col_Chroma_Shift,
			 short Steg_Encode_Col_Chroma_Shift_Cb_On,
			 short Steg_Encode_Col_Chroma_Shift_Cr_On,
			 short Steg_Encode_Col_Chroma_Shift_Down,
	                 short Steg_Encode_Col_Step_factor,
	                 short Cb_Shift,
	                 short Cr_Shift,	
                         char  *Steg_Encode_Message,
			 char  *infile,
			 char  *outfile);

%newobject steg_easy_decode2;
char*   steg_easy_decode2(char* message,
                          short Steg_Decode_Specific_Diff, 
		          short Cb_Shift, 
		          short Cr_Shift,
			  short Steg_Decode_Bound,
                          short Steg_Decode_Anti_Crop_On,
                          short Steg_Decode_No_Crop_Hint,
			  short Steg_Anti_Crop_Percent,
			  short Steg_Decode_Ignore_Y,
                          short Steg_Decode_Y_Bound,
                          char* infile, 
                          char* outfile);
