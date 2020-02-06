#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#ifdef MAC
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#define PROGRAM_FILE "ex3.cl"
#define KERNEL_FUNC "mat_add"
#define OUTPUT "output_cl.txt"

#define SIZE 100

void error(cl_int err, char* func_name)
{
	printf("Error %d in %s\n", err, func_name);
	exit(1);
}

void populate(float m1[][SIZE], float m2[][SIZE])
{
	cl_int i, j;
	for (i=0; i<SIZE; i++) {
		for (j=0; j<SIZE; j++) {
			m1[i][j] = 1.0f;
			m2[i][j] = 2.0f;
		}
	}	
}

cl_device_id create_device(void)
{
	cl_platform_id 	plat;
	cl_device_id 	dev;
	cl_int		err,
			is_cpu=0;
	size_t		size;
	
	err = clGetPlatformIDs(1, &plat, NULL);
	if (err < 0) error(err, "clGetPlatformIDs");

	err = clGetDeviceIDs(plat, CL_DEVICE_TYPE_GPU, 1, &dev, NULL);
	if (err == CL_DEVICE_NOT_FOUND) {
		err = clGetDeviceIDs(plat, CL_DEVICE_TYPE_CPU, 1, &dev, NULL);
		is_cpu=1;
	}
	if (err < 0) error(err, "clGetDeviceIDs");

	err = clGetDeviceInfo(dev, CL_DEVICE_EXTENSIONS, 0, NULL, &size);
	if (err < 0) error(err, "clGetDeviceInfo");

	char name[size];
	err = clGetDeviceInfo(dev, CL_DEVICE_NAME, size, name, NULL);
	if (err < 0) error(err, "clGetDeviceInfo");
	printf("Device type: ");
	printf(is_cpu > 0 ? "CPU" : "GPU");
	printf("\nName: %s\n", name);

	return dev;
}

cl_program create_program(cl_context ctx)
{
	cl_program	program;
	FILE 		*hProgram;
	char 		*program_buff;
	size_t 		program_size;
	cl_int		err;

	/* Read program file and place content into buffer */
	hProgram = fopen(PROGRAM_FILE, "r");
	if (hProgram == NULL) {
		perror("File nout found");
		exit(1);
	}

	fseek(hProgram, 0, SEEK_END);
	program_size = ftell(hProgram);
	rewind(hProgram);
	program_buff = (char *)malloc(program_size +1);
	program_buff[program_size] = '\0';
	fread(program_buff, sizeof(char), program_size, hProgram);
	fclose(hProgram);

	/* Create program from a file */
	program = clCreateProgramWithSource(ctx, 1, 
		(const char**)&program_buff, &program_size, &err);
	if (err < 0) error(err, "clCreateProgramWithSource");
	free(program_buff);

	return program;
}

cl_program build_program(cl_program program, cl_context ctx, cl_device_id dev)
{
	char		*program_log;
	size_t		log_size;
	cl_int		err;

	/* Build program */
	err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
	if (err < 0) {
		clGetProgramBuildInfo(program, dev, CL_PROGRAM_BUILD_LOG,
			0, NULL, &log_size);
		program_log = (char*) malloc(log_size + 1);
		program_log[log_size] = '\0';
		clGetProgramBuildInfo(program, dev, CL_PROGRAM_BUILD_LOG,
			log_size + 1, program_log, NULL);
		printf("%s\n", program_log);
		free(program_log);
		exit(1);
	}
	return program;
}

void print_results(float res[][SIZE])
{
	FILE 	*out;
	cl_int 	i, j;
	out = fopen(OUTPUT, "w");
	if (out == NULL) {
		printf("Creating output file failed\n");
		exit(1);
	}

	for (i=0; i<SIZE; i++)
		for (j=0; j<SIZE; j++)
			fprintf(out, "%f\n", res[i][j]);
	fclose(out);
}

int main(void)
{
	/* Host devices, program and kernel structures */
	cl_device_id 		device;
	cl_context 		context;
	cl_command_queue	queue;
	cl_program 		program;
	cl_kernel		kernel;

	/* Data and buffers */
	float			m1[SIZE][SIZE],
				m2[SIZE][SIZE],
				res[SIZE][SIZE];
	cl_mem			m1_buff,
				m2_buff,
				res_buff;
	/* Misc */
	cl_int			err;
	clock_t 		s, e, t;	
	size_t			max_dim;
	size_t 			local_item_size,
				global_item_size;
	
	local_item_size = 4;
	global_item_size = 10000;




	/* Populate matrices */
	populate(m1, m2);

	/* Create device and context */
	device = create_device();
	context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
	if (err < 0) error(err, "clCreateContext");

	/* Get max work item dimension and sizes */	
	err = clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,
		sizeof(cl_uint), &max_dim, NULL);
	if (err < 0) error(err, "clGetDeviceInfo - Max work item dimension");
	
	/* Create and build the program */
	program = create_program(context);
	program = build_program(program, context, device);

	/* Create buffers */
	m1_buff = clCreateBuffer(context, CL_MEM_READ_ONLY |
		CL_MEM_COPY_HOST_PTR, sizeof(m1), m1, &err);
	if (err < 0) error(err, "clCreateBuffer_m1");
	m2_buff = clCreateBuffer(context, CL_MEM_READ_ONLY |
		CL_MEM_COPY_HOST_PTR, sizeof(m2), m2, &err);
	if (err < 0) error(err, "clCreateBuffer_m2");
	res_buff = clCreateBuffer(context, CL_MEM_WRITE_ONLY,
		sizeof(res), NULL, &err);
	if (err < 0) error(err, "clCreateBuffer_res");

	/* Create command queue */
	queue = clCreateCommandQueue(context, device, 0, &err);
	if (err < 0) error(err, "clCreateCommandQueue");



	err = clEnqueueWriteBuffer(queue, m1_buff, CL_TRUE, 0, sizeof(m1),
		 m1, 0, NULL, NULL);
	err |= clEnqueueWriteBuffer(queue, m2_buff, CL_TRUE, 0, sizeof(m2),
		 m2, 0, NULL, NULL);
	if (err < 0) error(err, "EnqueueWriteBuffer");

	
	/* Create Kernel */
	kernel = clCreateKernel(program, KERNEL_FUNC, &err);
	if (err < 0) error(err, "clCreateKernel");

	/* Create kernel arguments */
	err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &m1_buff);
	err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &m2_buff);
	err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &res_buff);
	if (err < 0) error(err, "clSetKernelArg");

	s = clock();	
	/* Enqueue the command queue to device */
	err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_item_size, 
		&local_item_size, 0, NULL, NULL);
	e = clock();
	if (err < 0) error(err, "clEnqueueNDRangeKernel");
	
	t = (e-s);
	printf("clock ticks: %ld\n", t);

	/* Read output buffer */
	err = clEnqueueReadBuffer(queue, res_buff, CL_TRUE, 0, sizeof(res),
		res, 0, NULL, NULL);
	if (err < 0) error(err, "clEnqueueReadBuffer");

	/* Print result to file. Easy to diff with plain c result. */
	print_results(res);

	/* Deallocate resources */
	clReleaseMemObject(m1_buff);
	clReleaseMemObject(m2_buff);
	clReleaseMemObject(res_buff);
	clReleaseKernel(kernel);
	clReleaseCommandQueue(queue);
	clReleaseProgram(program);
	clReleaseContext(context);

	printf("\n");
	return 0;
}

