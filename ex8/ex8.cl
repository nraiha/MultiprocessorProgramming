#define WIN_W 18
#define WIN_H 14
#define WIN_PIXELS WIN_W*WIN_H

__kernel void
calc_zncc(__global unsigned char* il, __global unsigned char* ir,
		   unsigned int w, unsigned int h, int disp_min, int disp_max,
		   __global unsigned char* disp_map)
{
	const int i = get_global_id(0);
	const int j = get_global_id(1);

	float cur_max;
	float sum_left;
	float sum_right;
	int idx_l;
	int idx_r;
	float nominator=0;
	float denominator1=0;
	float denominator2=0;
	float center_left;
	float center_right;
	float zncc;
	int disp_best=0;


	cur_max = -1;
	disp_best = disp_max;

	for (int d=disp_min; d<=disp_max; d++) {
		/*
		 * Calculate the mean
		 */
		sum_left = 0;
		sum_right = 0;
		for (int win_y=-WIN_H/2; win_y<WIN_H/2; win_y++) {
			for (int win_x=-WIN_W/2; win_x<WIN_W/2; win_x++) {
				/* Border checking */
				 if (i+win_y < 0     || i+win_y >= h ||
				     j+win_x < 0     || j+win_x >= w ||
				     j+win_x-d < 0   || j+win_x-d >= w)
					 continue;

				idx_l = w * (i+win_y) +j+win_x;
				idx_r = w *(i+win_y)+j+win_x-d;

				sum_left += il[idx_l];
				sum_right += ir[idx_r];
			}
		}
		sum_left /= WIN_PIXELS;
		sum_right /= WIN_PIXELS;

		/*
		 * Calcucate ZNCC
		 */
		nominator = 0;
		denominator1 = 0;
		denominator2 = 0;

		for (int win_y=-WIN_H/2; win_y<WIN_H/2; win_y++) {
			for (int win_x=-WIN_W/2; win_x<WIN_W/2; win_x++) {
				/* Border checking */
				if (i+win_y < 0     || i+win_y >= h ||
				    j+win_x < 0     || j+win_x >= w ||
				    j+win_x-d < 0   || j+win_x-d >= w)
                            		continue;
				idx_l = w * (i+win_y) +j+win_x;
				idx_r = w *(i+win_y)+j+win_x-d;

				center_left = il[idx_l] - sum_left;
				center_right = ir[idx_r] - sum_right;

				nominator += center_left * center_right;
				denominator1 += pow(center_left, 2);
				denominator2 += pow(center_right, 2);
			}
		}
		zncc = nominator / (sqrt(denominator1*denominator2));

		if (zncc > cur_max) {
			cur_max = zncc;
			disp_best = d;
		}
	}
	disp_map[i*w+j] = (unsigned char) abs(disp_best);
}


