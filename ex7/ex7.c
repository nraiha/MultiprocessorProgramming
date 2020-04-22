#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "lodepng.h"

#define MAX_DISP 64
#define MIN_DISP 0
#define WIN_W 18
#define WIN_H 14
#define WIN_PIXELS WIN_W*WIN_H
#define THRESHOLD 12

/* Prototypes */
void calc_zncc(unsigned char* il, unsigned char* ir, unsigned int w,
		unsigned int h, int disp_min, int disp_max,
		unsigned char* disp_map);

void cross_checking(unsigned char* left, unsigned char* right,
		unsigned int size, unsigned char* out);

void occlusion_filling(unsigned char* res, unsigned int size);

void normalize(unsigned char* res, unsigned int size);




void calc_zncc(unsigned char* il, unsigned char* ir, unsigned int w,
		unsigned int h, int disp_min, int disp_max,
		unsigned char* disp_map)
{
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

#pragma omp parallel for
	for (int i=0; i<h; i++) {
	for (int j=0; j<w; j++) {
		cur_max = -1;
		disp_best = disp_max;

	for (int d=disp_min; d<=disp_max; d++) {
		/*
		 * Calculate the mean
		 */
		sum_left = 0;
		sum_right = 0;
		for (int win_x=-WIN_H/2; win_x<WIN_H/2; win_x++) {
		for (int win_y=-WIN_W/2; win_y<WIN_W/2; win_y++) {
			/* Border checking */
                         if (i+win_x < 0     || i+win_x >= h ||
			     j+win_y < 0     || j+win_y >= w ||
			     j+win_y-d < 0   || j+win_y-d >= w)
				 continue;

			idx_l = w * (i+win_x) +j+win_y;
			idx_r = w *(i+win_x)+j+win_y-d;

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

		for (int win_x=-WIN_H/2; win_x<WIN_H/2; win_x++) {
		for (int win_y=-WIN_W/2; win_y<WIN_W/2; win_y++) {
			/* Border checking */
                        if (i+win_x < 0     || i+win_x >= h ||
                            j+win_y < 0     || j+win_y >= w ||
                            j+win_y-d < 0   || j+win_y-d >= w)
                            continue;
			idx_l = w * (i+win_x) +j+win_y;
			idx_r = w *(i+win_x)+j+win_y-d;

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
	}

}


void cross_checking(unsigned char* left, unsigned char* right,
		unsigned int size, unsigned char* out)
{
	int i;
	for (i=0; i<size; i++) {
		if (abs(left[i] - right[i]) > THRESHOLD)
			out[i] = 0;
		else
			out[i] = left[i];
	}
}

void occlusion_filling(unsigned char* res, unsigned int size)
{
	int i;
	int nn_color = 0; // nearest neighbour color
	for (i=0; i<size; i++) {
		if (res[i] == 0)
			res[i] = nn_color;
		else
			nn_color = res[i];
	}
}

void normalize(unsigned char* res, unsigned int size)
{
	int i;
	unsigned char max = 0;
	unsigned char min = 255;
	for (i=0; i<size; i++) {
		if (res[i] > max)
			max = res[i];
		if (res[i] < min)
			min = res[i];
	}

	for (i=0; i<size; i++) {
		if(max-min == 0) {
			res[i] = 0;
			continue;
		}
		res[i] = (unsigned char) (255*(res[i]-min)/(max-min));
	}
}


int main(void)
{
	const char* inL = "imageL.png";
	const char* inR = "imageR.png";
	const char* out = "output.png";

	unsigned err;
	unsigned char* imageL=0;
	unsigned char* imageR=0;

	unsigned int w, h;
	unsigned int temp, size;
	unsigned char* disp_l2r;
	unsigned char* disp_r2l;
	unsigned char* res;

	/* Load images */
	err = lodepng_decode_file(&imageL, &w, &h, inL, LCT_GREY, 8);
	if (err)
		return 1;
	temp = w*h;

	err = lodepng_decode_file(&imageR, &w, &h, inR, LCT_GREY, 8);
	if (err)
		return 1;
	size = w*h;

	if (size != temp) {
		printf("Image size should be the same!\n");
		return 2;
	}

	disp_l2r = calloc(size, sizeof(unsigned char));
	disp_r2l = calloc(size, sizeof(unsigned char));

	/* Calculate ZNCC */
	printf("Calculating L2R\n");
	calc_zncc(imageL, imageR, w, h, MIN_DISP, MAX_DISP, disp_l2r);
	printf("Calculating R2L\n");
	calc_zncc(imageR, imageL, w, h, -MAX_DISP, MIN_DISP, disp_r2l);
	res = calloc(size, sizeof(unsigned char));

	printf("Post-processing...\n");

	/* Cross checking */
	cross_checking(disp_l2r, disp_r2l, size, res);
	/* Occlusion filling */
	occlusion_filling(res, size);
	/* Normalize */
	normalize(res, size);

	printf("Done, creating the output image\n");
	lodepng_encode_file(out, res, w, h, LCT_GREY, 8);

	free(imageL);
	free(imageR);
	free(disp_l2r);
	free(disp_r2l);
	free(res);
	return 0;

}
