#include "lodepng.h"
#include <OpenCL/cl.h>
#include <stdio.h>

#define PROGRAM "ex8.cl"
#define F_ZNCC "calc_zncc"

#define WIN_W 18
#define WIN_H 14
#define WIN_SIZE WIN_W*WIN_H
#define THRESHOLD 12




void error(cl_int err, char* func_name);
cl_device_id create_device(void);
cl_program build_program(cl_context ctx, cl_device_id dev, const char* name);
unsigned char* read_image(unsigned* width, unsigned* height, const char* name);


void error(cl_int err, char* func_name)
{
	printf("Error %d in %s\n", err, func_name);
	exit(1);
}

cl_device_id create_device(void)
{
	cl_platform_id plat;
	cl_device_id dev;
	cl_int err;

	err = clGetPlatformIDs(1, &plat, NULL);
	if (err < 0) error(err, "clGetPlatformIDs");

	err = clGetDeviceIDs(plat, CL_DEVICE_TYPE_GPU, 1, &dev, NULL);
	if (err == CL_DEVICE_NOT_FOUND)
		err = clGetDeviceIDs(plat, CL_DEVICE_TYPE_CPU, 1, &dev, NULL);
	if (err < 0) error(err, "clGetDeviceIDs");
	return dev;
}

cl_program build_program(cl_context ctx, cl_device_id dev, const char* name)
{
	cl_program program;
	FILE 	   *p_handle;
	char 	   *p_buffer;
	char 	   *p_log;
	size_t 	   p_size;
	size_t 	   log_size;
	cl_int 	   err;

	p_handle = fopen(name, "r");
	if (p_handle == NULL) {
		perror("Couldn't find the file");
		exit(1);
	}
	fseek(p_handle, 0, SEEK_END);
	p_size = ftell(p_handle);
	rewind(p_handle);
	p_buffer = (char*)malloc(p_size+1);
	p_buffer[p_size] = '\0';
	fread(p_buffer, sizeof(char), p_size, p_handle);
	fclose(p_handle);

	program = clCreateProgramWithSource(ctx, 1, (const char**)&p_buffer,
			&p_size, &err);
	if (err < 0) error(err, "clCreateProgramWithSource");
	free(p_buffer);

	err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
	if (err < 0) {
		clGetProgramBuildInfo(program, dev, CL_PROGRAM_BUILD_LOG, 0,
				NULL, &log_size);
		p_log = (char *) malloc(log_size+1);
		p_log[log_size] = '\0';
		clGetProgramBuildInfo(program, dev, CL_PROGRAM_BUILD_LOG,
			log_size+1, p_log, NULL);
		printf("%s\n", p_log);
		free(p_log);
		exit(1);
	}

	return program;
}


unsigned char* read_image(unsigned* width, unsigned* height, const char* name)
{
	unsigned error;
	unsigned char* image=0;

	error = lodepng_decode_file(&image, width, height, name, LCT_GREY, 8);
	if (error) {
		printf("Error %u: %s\n", error, lodepng_error_text(error));
		return NULL;
	}
	return image;
}


int main(void)
{
	const char* inL = "imageL.png";
	const char* inR = "imageR.png";
	//const char* out = "output.png";

	unsigned char* imageL=0;
	unsigned char* imageR=0;

	unsigned int w, h;
	unsigned int temp, size;
	unsigned char* d_l2r;
	unsigned char* d_r2l;
	unsigned char* res=0;

	int max_disp;
	int min_disp;

	/* For OpenCL */
	cl_device_id device;
	cl_context context;
	size_t global_item_size[2];
	size_t buff_size;

	cl_program program;
	cl_kernel zncc_kernel;

	cl_command_queue queue;

	cl_mem buff_left;
	cl_mem buff_right;
	cl_mem buff_out_l2r;
	cl_mem buff_out_r2l;

	cl_event event1;
	cl_event event2;
	cl_ulong start;
	cl_ulong end;
	double total;
	cl_int err;


	/*****************************
	 *
	 *	READ IMAGES
	 *
	 *****************************/
	imageL = read_image(&w, &h, inL);
	temp = w*h;
	imageR = read_image(&w, &h, inR);
	size = w*h;
	if(imageL == NULL || imageR == NULL) {
		printf("Image is NULL!\n");
		return 1;
	}
	if (temp != size) {
		printf("Image dimensions should be the same!\n");
		return 2;
	}


	/*****************************
	 *
	 *	CALLOCS
	 *
	 *****************************/
	d_l2r = calloc(size, sizeof(unsigned char));
	d_r2l = calloc(size, sizeof(unsigned char));
	res = calloc(size, sizeof(unsigned char));


	/* w x h x 4 pixels (RGBA) x sizeof */
	buff_size = size * 4 *sizeof(unsigned char);
	global_item_size[0] = h;
	global_item_size[1] = w;
	max_disp = 64;
	min_disp = 0;


	/*****************************
	 *
	 *  	DEVICE & CONTEXT
	 *
	 *****************************/
	device = create_device();
	context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
	if (err < 0) error(err, "clCreateContext");


	/*****************************
	 *
	 *	PROGRAM & KERNEL
	 *
	 *****************************/
	program = build_program(context, device, PROGRAM);
	zncc_kernel = clCreateKernel(program, F_ZNCC, &err);
	if (err < 0) error(err, "clCreateKernel");


	/*****************************
	 *
	 *	QUEUE
	 *
	 *****************************/
	queue = clCreateCommandQueue(context, device, 0, &err);
	if (err < 0) error(err, "clCreateCommandQueue");


	/*****************************
	 *
	 *	BUFFERS
	 *
	 *****************************/
	/* IN */
	buff_left = clCreateBuffer(context, CL_MEM_READ_ONLY |
			CL_MEM_COPY_HOST_PTR, buff_size, imageL, &err);
	if (err < 0) error(err, "clCreateBuffer (buff_left)");

	buff_right = clCreateBuffer(context, CL_MEM_READ_ONLY |
			CL_MEM_COPY_HOST_PTR, buff_size, imageR, &err);
	if (err < 0) error(err, "clCreateBuffer (buff_right)");


	/* OUT */
	buff_out_l2r = clCreateBuffer(context, CL_MEM_WRITE_ONLY, buff_size,
			NULL, &err);
	if (err < 0) error(err, "ClCreateBuffer (buff_out_l2r");

	buff_out_r2l = clCreateBuffer(context, CL_MEM_WRITE_ONLY, buff_size,
			NULL, &err);
	if (err < 0) error(err, "ClCreateBuffer (buff_out_r2l");
#if 0

	/*****************************
	 *
	 *	KERNEL ARGS - L2R
	 *
	 *****************************/
	err = clSetKernelArg(zncc_kernel, 0, sizeof(cl_mem),
			(void*)&buff_left);
	err |= clSetKernelArg(zncc_kernel, 1, sizeof(cl_mem),
			(void*)&buff_right);
	err |= clSetKernelArg(zncc_kernel, 2, sizeof(unsigned int), (void*)&w);
	err |= clSetKernelArg(zncc_kernel, 3, sizeof(unsigned int), (void*)&h);
	err |= clSetKernelArg(zncc_kernel, 4, sizeof(int), (void*)&min_disp);
	err |= clSetKernelArg(zncc_kernel, 5, sizeof(int), (void*)&max_disp);
	err |= clSetKernelArg(zncc_kernel, 6, sizeof(cl_mem),
			(void*)&buff_out_l2r);
	if (err < 0) error(err, "clSetKernelArg L2R");


	/*****************************
	 *
	 *	ENQUEUE KERNEL L2R
	 *
	 *****************************/
	err = clEnqueueNDRangeKernel(queue, zncc_kernel, 2, NULL,
			global_item_size, NULL, 0, NULL, &event1);
	if (err < 0 ) error(err, "clEnqueueNDRangeKernel - zncc");
	clFinish(queue);


	/*****************************
	 *
	 *	BUFFER BACK TO HOST
	 *
	 *****************************/
	err = clEnqueueReadBuffer(queue, buff_out_l2r, CL_TRUE, 0, size, d_l2r,
			0, NULL, NULL);
	if (err < 0) error(err, "clEnqueueReadBuffer");

#endif
	/*****************************
	 *
	 *	KERNEL ARGS - R2L
	 *
	 *****************************/
	max_disp = -max_disp;

	err = clSetKernelArg(zncc_kernel, 0, sizeof(cl_mem),
			(void*)&buff_right);
	err |= clSetKernelArg(zncc_kernel, 1, sizeof(cl_mem),
			(void*)&buff_left);
	err |= clSetKernelArg(zncc_kernel, 2, sizeof(unsigned int), (void*)&w);
	err |= clSetKernelArg(zncc_kernel, 3, sizeof(unsigned int), (void*)&h);
	err |= clSetKernelArg(zncc_kernel, 4, sizeof(int), (void*)&max_disp);
	err |= clSetKernelArg(zncc_kernel, 5, sizeof(int), (void*)&min_disp);
	err |= clSetKernelArg(zncc_kernel, 6, sizeof(cl_mem),
			(void*)&buff_out_r2l);
	if (err < 0) error(err, "clSetKernelArg R2L");


	/*****************************
	 *
	 *	ENQUEUE KERNEL R2L
	 *
	 *****************************/
	err = clEnqueueNDRangeKernel(queue, zncc_kernel, 2, NULL,
			global_item_size, NULL, 0, NULL, &event2);
	if (err < 0 ) error(err, "clEnqueueNDRangeKernel - zncc");
	clFinish(queue);

	/*****************************
	 *
	 *	BUFFER BACK TO HOST
	 *
	 *****************************/
	err = clEnqueueReadBuffer(queue, buff_out_r2l, CL_TRUE, 0, size, d_r2l,
			0, NULL, NULL);
	if (err < 0) error(err, "clEnqueueReadBuffer");


	/*****************************
	 *
	 *	POST PROCESS
	 *
	 *****************************/


	/*****************************
	 *
	 *	PROFILING
	 *
	 *****************************/
#if 0
	err = clGetEventProfilingInfo(event1, CL_PROFILING_COMMAND_START,
			sizeof(start), &start, NULL);
	err = clGetEventProfilingInfo(event1, CL_PROFILING_COMMAND_END,
			sizeof(end), &end, NULL);
	total = end - start;

#endif
	err = clGetEventProfilingInfo(event2, CL_PROFILING_COMMAND_START,
			sizeof(start), &start, NULL);
	err = clGetEventProfilingInfo(event2, CL_PROFILING_COMMAND_END,
			sizeof(end), &end, NULL);
	total = end - start;


	printf("ZNCC kernel execution time: %0.3f ms\n",
			total / 1000000.0);



	/*****************************
	 *
	 *	FREE
	 *
	 *****************************/
	clReleaseKernel(zncc_kernel);
	clReleaseProgram(program);
	clReleaseMemObject(buff_left);
	clReleaseMemObject(buff_right);
	clReleaseMemObject(buff_out_l2r);
	clReleaseMemObject(buff_out_r2l);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);

	free(imageL);
	free(imageR);
	free(d_l2r);
	free(d_r2l);

	return 0;
}


