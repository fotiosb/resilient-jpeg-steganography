#ifndef _INTERFACE_H
#define _INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

extern char* Steg_Message;
//extern int** spike;

int         steg_easy_apply2( char*, 
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
			      char*,
			      char*, 
			      char* );

char*       steg_easy_decode2( char*,
			       short,
			       short,
			       short,
			       short,
			       short,
			       short,
			       short,
			       short,
			       short,
			       char*,
			       char* );

#ifdef __cplusplus
}
#endif

#endif
