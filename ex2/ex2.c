#include <stdio.h>
#include <stdlib.h>
#include "lodepng.h"

int main(void)
{
	const char*   in = "image.png";
	const char*   out = "output_image.png";

	unsigned      error, width, height, i, gray;
	unsigned char *image = 0;

	size_t        buff_size;

	/* Load and decode the image */
	error = lodepng_decode32_file(&image, &width, &height, in);
	if (error)
		printf("Error %u: %s\n", error, lodepng_error_text(error));

	/* 
	 * Calculate the total buffer size and image resolution 
	 * One pixel consists of 4 bytes.
	 * 	Red 	Green	Blue	Alpha
	 * 	i	i+1	i+2	i+3
	 */
	buff_size = width * height * 4; 
	
	/* 
	 *	    Image processing 
	 *
	 * Formula for luminance of a color by CIE:
	 * 	L = 0.2126xR + 0.7152xG + 0.0722xB
	 */

	for (i=0; i<buff_size; i+=4) {
		gray = 0x2126*image[i] + 0.7152*image[i+1] + 0.0722*image[i+2];
		if (gray < 128)
			gray = 0;
		image[i] = gray;
		image[i+1] = gray;
		image[i+2] = gray;
	}
	
	/* Write the image as output_image.png */
	error = lodepng_encode32_file(out, image, width, height);	
	if (error)	
		printf("error %u: %s\n", error, lodepng_error_text(error));

	free(image);
	return 0;
}
