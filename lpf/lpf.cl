__kernel void
lpf(__global unsigned char* in, __global unsigned char* out, unsigned int w)
{
	int i;	
	const int j = get_global_id(0);
	__private unsigned char row[WIDTH];

	/* Prefetch the row */
	for (i=0; i<w; i++)
		row[i] = in[j*w+i];

	/* First px of the row */
	out[j*w] = row[0];

	/* LPF - calculate the rest pxs */
	for (i=1; i<w; i++)
		/* I_new(i,j) = [I(i,j) + I(i-1, j)]/2 */
		out[j*w + i] = (row[i] + row[i-1])/2;
	
}
