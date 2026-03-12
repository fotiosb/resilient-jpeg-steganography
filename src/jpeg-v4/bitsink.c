/*
 * Input should have the following format:
 * 
 * +-----+----------- -----+-------------------------------- | A | B B B . . . 
 * B | C C C C C C C C C C . . . +-----+-----------
 * -----+--------------------------------
 * 
 * "A" is five bits.  It expresses the length (in bits) of field B. Order is
 * most-significant-bit first.
 * 
 * "B" is some number of bits from zero to thirty-one.  It expresses the
 * length (in bytes) of the injection file.  Order is again
 * most-significant-bit first.  The range of values for "B" is 0 to
 * 2147483648.
 * 
 * "C" is the bits in the injection file.  No ordering is implicit on the bit
 * stream.
 * 
 * ===============
 * 
 */

#ifdef STEG_SUPPORTED

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define BOTTOM 0x1
#define BITMAX CHAR_BIT - 1
#define BUFSIZE 1024
#define AWIDTH 5
#define BMAXWIDTH 31

static unsigned char buf[BUFSIZE];
static unsigned char *buf_ptr = buf;
static unsigned short bufindex = 0;
static unsigned short use_buffer = 0;
static signed short bitindex = BITMAX;

static FILE    *outfile = 0;
static unsigned long file_size = 0;
static unsigned long output_size = 0;
static unsigned long buf_size = 0;
static unsigned long fileindex = 0;
static unsigned short rep_width = 0;
static unsigned short use_exject = 0;
static unsigned short curr_special = 0;

static int
empty(void)
{
        int             written = fwrite(buf, sizeof(char), bufindex, outfile);
        if (written != bufindex) {
                bufindex = 0;
                bitindex = BITMAX;
                fclose(outfile);
                outfile = 0;
                return -1;
        }
        bufindex = 0;
        bitindex = BITMAX;
        return 0;
}

FILE           *
bitsavefile(FILE * out)
{
        if (bufindex)
                empty();
        use_exject = 1;
        if (outfile) {
                fclose(out);
                return outfile;
        }
        outfile = out;
        bufindex = 0;
        bitindex = BITMAX;
        return outfile;
}

void
bitsavebuffer(void *buffer, size_t bufsz)
{
        use_exject = 1;
        use_buffer = 1;
        buf_size = bufsz;
        buf_ptr = buffer;
        bufindex = 0;
        bitindex = BITMAX;
}

int
bitsetbit(unsigned char c)
{
        int             ret_val;

        if (use_buffer == 0) {
                if (outfile == 0)
                        return -1;
                if (curr_special < AWIDTH)
                        rep_width |= ((c & BOTTOM) << (AWIDTH - ++curr_special));
                else if (curr_special < AWIDTH + rep_width)
                        file_size |=
                            ((c & BOTTOM) << (AWIDTH + rep_width - ++curr_special));
                else {
                        buf_ptr[bufindex] &= ~(BOTTOM << bitindex);
                        buf_ptr[bufindex] |= ((c & BOTTOM) << bitindex);
                        if (--bitindex == -1) {
                                bitindex = BITMAX;
                                ++bufindex;
                                ++fileindex;
                        }
                        if (fileindex == file_size) {
                                if (bitindex != BITMAX)
                                        return -2;
                                ret_val = empty();
                                fclose(outfile);
                                outfile = 0;
                                return ret_val;
                        }
                        if (bufindex == BUFSIZE)
                                return empty();
                }
                return 0;
        }
        else {
                if (curr_special < AWIDTH)
                        rep_width |= ((c & BOTTOM) << (AWIDTH - ++curr_special));
                else if (curr_special < AWIDTH + rep_width) {
                        output_size |=
                            ((c & BOTTOM) << 
                             (AWIDTH + rep_width - ++curr_special));
                        if (output_size > buf_size)
                                output_size = buf_size;
                }
                else {
                        buf_ptr[bufindex] &= ~(BOTTOM << bitindex);
                        buf_ptr[bufindex] |= ((c & BOTTOM) << bitindex);
                        if (--bitindex == -1) {
                                bitindex = BITMAX;
                                ++bufindex;
                        }
                        if (bufindex > output_size) {
                                use_exject = 0;
                                if (bitindex != BITMAX)
                                        return -2;
                                return 0;
                        }
                }
                return 0;
        }
}

void
exject(short outval)
{
        if ((outval & BOTTOM) != outval)
                if (use_exject)
                        bitsetbit(outval & BOTTOM);
}

#endif				/* STEG_SUPPORTED */
