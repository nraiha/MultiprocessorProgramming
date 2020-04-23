#include <stdio.h>
#include <math.h>
#include "lodepng.h"
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif


#define PROGRAM "lpf.cl"
#define FUNC "lpf"


void error(cl_int err, char* func_name);
cl_device_id create_device(void);
cl_program build_program(cl_context ctx, cl_device_id dev, const char* name, 
	char* args);
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

cl_program build_program(cl_context ctx, cl_device_id dev, const char* name,
	char* args)
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

	err = clBuildProgram(program, 0, NULL, args, NULL, NULL);
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
	const char* in = "input.png";
	const char* out = "output.png";

	unsigned char* image=0;
	unsigned char* res=0;
	unsigned int w;
	unsigned int h;

	cl_device_id device;
	cl_context context;
	cl_program program;
	cl_kernel kernel;
	cl_command_queue queue;

	cl_event event;
	cl_ulong start, end;
	double total;

	cl_mem buff_in;
	cl_mem buff_out;
	size_t buff_size;
	size_t global_item_size;

	cl_int err;
	char args[64];


	image = read_image(&w, &h, in);
	buff_size = w * h * 4 * sizeof(unsigned char);
	res = calloc(buff_size, sizeof(unsigned char));	

	device = create_device();
	context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
	if (err < 0) error(err, "clCreateContext");

	/* https://software.intel.com/en-us/forums/opencl/topic/520001 */
	sprintf(args, "-DWIDTH=%d", w);
	program = build_program(context, device, PROGRAM, args);
	kernel = clCreateKernel(program, FUNC, &err);
	if (err < 0) error(err, "clCreateKernel");

	queue = clCreateCommandQueue(context, device, 
		CL_QUEUE_PROFILING_ENABLE, &err);
	if (err < 0) error(err, "clCreateCommandQueue");

	buff_in = clCreateBuffer(context, CL_MEM_READ_ONLY | 
			CL_MEM_COPY_HOST_PTR, buff_size, image, &err);
	if (err < 0) error(err, "clCreateBuffer - in");
	buff_out = clCreateBuffer(context, CL_MEM_WRITE_ONLY, buff_size, NULL, 
			&err);
	if (err < 0) error(err, "clCreateBuffer - out");

	global_item_size = (size_t)h;
	err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&buff_in);
	if (err < 0) error(err, "clSetKernelArg");
	err = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&buff_out);
	if (err < 0) error(err, "clSetKernelArg");
	err = clSetKernelArg(kernel, 2, sizeof(unsigned int), (void *)&w);
	if (err < 0) error(err, "clSetKernelArg");
	

	err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_item_size,
		NULL, 0, NULL, &event);
	if (err < 0) error(err, "clEnqueueNDRangeKernel");

	err = clEnqueueReadBuffer(queue, buff_out, CL_TRUE, 0, buff_size, 
		res, 0, NULL, NULL);
	if (err < 0) error(err, "clEnqueueReadBuffer");


	err = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, 
		sizeof(start), &start, NULL);
	err = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, 
		sizeof(end), &end, NULL);
	total = (end-start)/1000000.0;
	printf("LPF kernel execution time: %0.3f ms\n", total);

	lodepng_encode_file(out, res, w, h, LCT_GREY, 8);
	

	clReleaseKernel(kernel);
	clReleaseProgram(program);
	clReleaseMemObject(buff_in);
	clReleaseMemObject(buff_out);
	clReleaseCommandQueue(queue);
	clReleaseContext(context);

	free(image);


}


