#include <stdio.h>
#include <math.h>
#include "lodepng.h"


#if defined __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#define PROGRAM "ex4.cl"
#define FUNC "moving_avg"

#define INPUT "image.png"
#define OUTPUT "output.png"

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



unsigned char* read_image(unsigned* width, unsigned* height)
{
	unsigned error;
	unsigned char* image=0;

	error = lodepng_decode_file(&image, width, height, INPUT, LCT_GREY, 8);
	if (error) {
		printf("Error %u: %s\n", error, lodepng_error_text(error));
		return NULL;
	}
	return image;
}

void write_image(unsigned char* image, unsigned width, unsigned height)
{
	unsigned error;
	error = lodepng_encode_file(OUTPUT, image, width, height, LCT_GREY, 8);
	if (error)
		printf("Error %u: %s\n", error, lodepng_error_text(error));
}


int main(void)
{
	/* For LodePNG */
	unsigned char* 	 image = 0;
	unsigned char*   image_out = 0;
	unsigned 	 width;
	unsigned 	 height;
	size_t 	 	 buff_size;

	/* For openCL */
	cl_device_id 	 device;
	cl_context	 context;
	size_t		 local_item_size;
	size_t		 global_item_size;

	cl_program	 program;
	cl_kernel	 kernel;

	cl_command_queue queue;

	cl_mem		 buff_in;
	cl_mem		 buff_out;

	cl_int 		 err;


	/* Read image as grayscale and get the size of the image buffer */
	image = read_image(&width, &height);
	if (image == NULL) {
		printf("Could not read image!\n");
		exit(1);
	}
	buff_size = width * height * 4 * sizeof(unsigned char);


	/* openCL: create Device, context and work items */
	device = create_device();
	context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
	if (err < 0) error(err, "clCreateContext");
	local_item_size = 64;
	global_item_size =
		ceil(buff_size/(float)local_item_size)*local_item_size;


	/* openCL: create kernel and program */
	program = build_program(context, device, PROGRAM);
	kernel = clCreateKernel(program, FUNC, &err);
	if (err < 0) error(err, "clCreateKernel");

	/* openCL: create command queue */
	queue = clCreateCommandQueue(context, device, 0, &err);
	if (err < 0) error(err, "clCreateCommandQueue");


	/* openCL: create buffers */
	buff_in = clCreateBuffer(context, CL_MEM_READ_ONLY |
			CL_MEM_COPY_HOST_PTR, buff_size, image, &err);
	if (err < 0) error(err, "clCreateBuffer (buff_in)");
	buff_out = clCreateBuffer(context, CL_MEM_WRITE_ONLY, buff_size, NULL,
			&err);
	if (err < 0) error(err, "clCreateBuffer (buff_out)");


	/* openCL - kernel arguments */
	err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buff_in);
	if (err < 0) error(err, "clSetKernelArg 0");
	err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &buff_out);
	if (err < 0) error(err, "clSetKernelArg 1");
	err = clSetKernelArg(kernel, 2, sizeof(unsigned), (void*)&width);
	if (err < 0) error(err, "clSetKernelArg 2");
	err = clSetKernelArg(kernel, 3, sizeof(unsigned), (void*)&height);
	if (err < 0) error(err, "clSetKernelArg 3");


	/* openCL - execute the kernel */
	err = clEnqueueNDRangeKernel(queue, kernel, CL_TRUE, NULL,
			&global_item_size, &local_item_size, 0, NULL, NULL);
	if (err < 0) error(err, "clEnqueueNDRangeKernel");


	/* openCL - copy image back from the device */
	image_out = (unsigned char*) malloc(buff_size);
	err = clEnqueueReadBuffer(queue, buff_out, CL_TRUE, 0, buff_size,
			image_out, 0, NULL, NULL);
	if (err < 0) error(err, "clEnqueueNDRangeKernel");


	/* write image with lodepng */
	write_image(image_out, width, height);


	clReleaseKernel(kernel);
	clReleaseProgram(program);
	clReleaseMemObject(buff_in);
	clReleaseMemObject(buff_out);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);

	free(image);
	free(image_out);

	return 0;
}



