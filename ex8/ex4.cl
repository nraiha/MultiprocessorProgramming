__kernel void
moving_avg(__global unsigned char* in, __global unsigned char* out,
			unsigned int w, unsigned int h)
{
	int id = get_global_id(0);
	int i, j, temp;

	if (id%w < 2 || id%w >=w-2 || id < 2*w || id > w*h-2*w) {
		out[id] = in[id];
	} else {
		temp = 0;
		for (i=-2; i<=2; i++) {
			for (j=-2; j<=2; j++) {
				   temp += in[i*w + id+j];
			}
		}
		out[id] = temp * 0.04;
	}
}

