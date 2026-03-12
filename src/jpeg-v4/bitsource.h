#ifndef _BITSOURCE_H
#define _BITSOURCE_H


void            bitloadbuffer(void *, size_t);
FILE           *bitloadfile(FILE * in);
signed char     bitgetbit(void);
int             bitendload(void);
short           inject(short inval);

#endif
