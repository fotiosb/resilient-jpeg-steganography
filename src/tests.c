#include <stdio.h>
#include <stdlib.h>

#include "jsteg.h"
#include "jpeg.h"
#include "interface.h"

int
main(int argc, char **argv)
{
	char            test_str[] = "Watermark Test";
	char            input_buf[13];
	struct image_data *image;
	char           *decoded;
	int             i;

	if (argc < 4) {
		fprintf(stderr, "Insufficient arguments given\n");
		exit(EXIT_FAILURE);
	}
#if 0
	steg_init(argv[1]);
	image = steg_decompress(argv[2]);
	fprintf(stderr, "Max data: %ld\n", jsteg_max_size(image));
	jsteg_compress(test_str, argv[3], sizeof test_str,
		       jsteg_max_size(image));

	jsteg_decompress((void *) &decoded, argv[3]);
	printf("%s\n", decoded);
	steg_cleanup();
	free(decoded);
#endif

        /*
         * Test the easy interface
         */
        steg_easy_apply2(test_str, 10, 0, 4, 1, 40, 40, 1, 15, 1, 1, 1, 1, 0, 120, 2, 2, "test", argv[2], argv[3]);

	return 0;
}
