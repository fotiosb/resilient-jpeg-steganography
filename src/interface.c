
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interface.h"
#include "jsteg.h"
#include "jpeg.h"

//char* Steg_Message;

int** spike = NULL;
//int spike[2048][2048];

int** rowAnchorSpike = NULL;
//int rowAnchorSpike[2048][2048];


int
steg_easy_apply2(char  *message,
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
		 char* Steg_Encode_Message,
		 char* infile,
		 char* outfile)
{
        struct image_data *image;
        long            max;

        steg_init(message, 
		  watermark_font_size,
		  Steg_Encode_Old_Overlay_On,
		  Y_BUMP,
		  Y_BUMP_REACTIVE,
		  Watermark_X_Step,
                  Watermark_Y_Step,
		  Steg_Encode_Anti_Crop_On,
		  Steg_Anti_Crop_Percent,		  
		  Steg_Encode_Col_Chroma_Shift_On,
		  Steg_Encode_Col_Chroma_Shift,
		  Steg_Encode_Col_Chroma_Shift_Cb_On,
		  Steg_Encode_Col_Chroma_Shift_Cr_On,
		  Steg_Encode_Col_Chroma_Shift_Down,
	          Steg_Encode_Col_Step_factor,
	          Cb_Shift,
	          Cr_Shift,
		  Steg_Encode_Message);
	
        if ((image = steg_decompress(infile)) == NULL)
                return -1;

        max = jsteg_max_size(image);	
	
        jsteg_compress(message, outfile, strlen(message) + 1, max);
	
        steg_cleanup();

        return 0;
}

char* steg_easy_decode2 ( char* message,
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
			  char* outfile )
{
        struct image_data *image;
        long            max;	
	
	steg_decode_init( message, 
			  Steg_Decode_Specific_Diff, 
			  Cb_Shift, 
			  Cr_Shift, 
			  Steg_Decode_Bound, 
			  Steg_Decode_Anti_Crop_On,
			  Steg_Decode_No_Crop_Hint,
			  Steg_Anti_Crop_Percent,
			  Steg_Decode_Ignore_Y,
                          Steg_Decode_Y_Bound		  
			);
	
        if ((image = steg_decompress(infile)) == NULL)
                return -1;

	//FILE *ivalFile = fopen("/TEST/ival.txt", "w");
	
	//contiguous block dyn mem allocation
	int* temp1 = NULL;
	int* temp2 = NULL;
	if (!spike)
	{	
          spike = (int**) malloc(2048 * sizeof(int*));
	  rowAnchorSpike = (int**) malloc(2048 * sizeof(int*));
	  
	  temp1  = (int*)  malloc(4194304 * sizeof(int));
	  temp2  = (int*)  malloc(4194304 * sizeof(int));
	  
	  for (int i = 0; i < 2048; i++) 
	  {
	    spike[i] = temp1 + (i * 2048);
	    rowAnchorSpike[i] = temp2 + (i * 2048);
	    //int** spikePtr = spike + i;
	    //*spikePtr = temp + (i * 2048);
	  }
	}
	
	//fprintf(ivalFile, "spike = %d, temp1 = %d, rowAnchorSpike = %d, temp2 = %d\n", spike, temp1, rowAnchorSpike, temp2);
	//fclose(ivalFile);	
	
        max = jsteg_max_size(image);	
	
        jsteg_compress(message, outfile , strlen(message) + 1, max);	
	
	//return "fine";
	
	//Here we do the statistical processing of the spike array in order to
	//decode the steganographic message and place it in the extern var Steg_Message
	//short pixelPercent = 5;
	
	if (Steg_Decode_Anti_Crop_On && !Steg_Decode_No_Crop_Hint)
	{
	  //find_chroma_spike_cols4();
	  
	  //getColAnchorStep();
	  //stepThroughCols();
	  
	  getAnchorStep();
	  stepThrough();
	  decodeStegMessage4();
	  processMessageStrings();
	}
	else if (Steg_Decode_Anti_Crop_On && Steg_Decode_No_Crop_Hint)
	{
	  //find_chroma_spike_cols3b(58, Steg_Anti_Crop_Percent);
	  find_chroma_spikes(58, Steg_Anti_Crop_Percent);
	  decodeStegMessage3();
	}
	else
	{
	  //find_chroma_spike_cols3(56);
	  find_chroma_spikes2();
	  decodeStegMessage3();
	}
	

        /* Free dynamically allocated memory.
         * temp1/temp2 hold the actual pixel data blocks; spike/rowAnchorSpike
         * are merely arrays of pointers into those blocks.
         * Free the data blocks first, then the pointer arrays. */
	if (temp1) { free(temp1); temp1 = NULL; }
	if (temp2) { free(temp2); temp2 = NULL; }
	if (spike) { free(spike); spike = NULL; }
	if (rowAnchorSpike) { free(rowAnchorSpike); rowAnchorSpike = NULL; }
	
        //fprintf(ivalFile, "spike = %d, temp1 = %d, rowAnchorSpike = %d, temp2 = %d\n", spike, temp1, rowAnchorSpike, temp2);
	//fclose(ivalFile);
	
        steg_cleanup();
	
	return Steg_Message;
	//return "fine";
}
