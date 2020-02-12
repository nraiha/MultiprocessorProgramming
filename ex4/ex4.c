#include <stdio.h>
#include <png.h>


//#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define _CRT_SECURE_NO_WARNINGS

#if defined __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#define PROGRAM_FILE "ex4.cl"
#define KERNEL_FUNC1 "grayscale"
#define KERNEL_FUNC2 "moving_avg"

#define IMAGE "image.png"
#define OUTPUT "output_image.png"

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
	FILE *p_handle;
	char *p_buffer, *p_log;
	size_t p_size, log_size;
	cl_int err;
	
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



void read_image(const char* filename, png_bytep* input, png_bytep* output,
		size_t* width, size_t* height)
{
	int i;
	FILE *png_input;
	png_input = fopen(filename, "rb");
	if (png_input == NULL) {
		perror("Error reading image file");
		exit(1);
	}

	/* Read image data */
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
			NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	png_init_io(png_ptr, png_input);
	png_read_info(png_ptr, info_ptr);
	*width = png_get_image_width(png_ptr, info_ptr);
	*height = png_get_image_height(png_ptr, info_ptr);

	/* Alloc memory for input and output. Initialize data */
	*input = malloc(*height * png_get_rowbytes(png_ptr, info_ptr));
	*output = malloc(*height * png_get_rowbytes(png_ptr, info_ptr));
	for (i=0; i<*height; i++)
		png_read_row(png_ptr, *input + i *
			png_get_rowbytes(png_ptr, info_ptr), NULL);

	png_read_end(png_ptr, info_ptr);
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
	fclose(png_input);
}

void write_image_data(const char* filename, png_bytep data, size_t w, size_t h)
{
	int i;
	FILE *png_output;
	png_output = fopen(filename, "wb");
	if (png_output == NULL) {
		perror("Create output file failed");
		exit(1);
	}

	/* Write image data */
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
			NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	png_init_io(png_ptr, png_output);
	png_set_IHDR(png_ptr, info_ptr, w, h, 16, PNG_COLOR_TYPE_GRAY, 
			PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, 
			PNG_FILTER_TYPE_BASE);
	png_write_info(png_ptr, info_ptr);
	for (i=0; i<h; i++) 
		png_write_row(png_ptr, data+i * 
				png_get_rowbytes(png_ptr, info_ptr));
	
	png_write_end(png_ptr, NULL);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(png_output);
}


int main(void)
{
	/* Host/device */
	cl_device_id 	 dev = NULL;
	cl_context  	 ctx = NULL;
	cl_command_queue queue = NULL;
	cl_program  	 prog = NULL;
	cl_kernel	 g_kernel = NULL/*;
			 mavg_kernel = NULL*/;
	cl_int 		 err;
	size_t 		 global_size[2];

	/* Image data */
	png_bytep 	 input_pixels, 
			 output_pixels;
	cl_image_format  png_format;
	cl_mem		 input_img,
			 output_img;
	size_t 		 width,
			 height;
	size_t		 origin[3],
			 region[3];

	read_image(IMAGE, &input_pixels, &output_pixels, &width, &height);

	/* Device and context */
	dev = create_device();
	ctx = clCreateContext(NULL, 1, &dev, NULL, NULL, &err);
	if (err < 0) error(err, "clCreateContext");

	/* Kernel and program */
	prog = build_program(ctx, dev, PROGRAM_FILE);
	g_kernel = clCreateKernel(prog, KERNEL_FUNC1, &err);
	if (err < 0) error(err, "clCreateKernel");

	/* input image object */
	png_format.image_channel_order = CL_LUMINANCE;
	png_format.image_channel_data_type = CL_UNORM_INT16;
	input_img = clCreateImage2D(ctx, CL_MEM_READ_ONLY | 
			CL_MEM_COPY_HOST_PTR, &png_format, width, height,
			0, (void*)input_pixels, &err);
	if (err < 0) error(err, "clCreateImage2D - input");
	
	output_img = clCreateImage2D(ctx, CL_MEM_WRITE_ONLY, &png_format, 
			width, height, 0, NULL, &err);
	if (err < 0) error(err, "clCreateImage2D - output");

	/* Buffer */
	
	/* Kernel args */
	err = clSetKernelArg(g_kernel, 0, sizeof(cl_mem), &input_img);
	err |= clSetKernelArg(g_kernel, 1, sizeof(cl_mem), &input_img);
	if (err < 0) error(err, "clSetKernelArg");

	/* Command queue */
	queue = clCreateCommandQueue(ctx, dev, 0, &err);
	if (err < 0) error(err, "clCreateCommandQueue");

	/* Enqueue kernel */
	global_size[0] = width;
	global_size[1] = height;
	err = clEnqueueNDRangeKernel(queue, g_kernel, 2, NULL, global_size, 
		NULL, 0, NULL, NULL);
	if (err < 0) error(err, "clEnqueueNDRangeKernel");

	origin[0] = 0;
	origin[1] = 0;
	origin[2] = 0;
	region[0] = width;
	region[1] = height;
	region[2] = 1;
	err = clEnqueueReadImage(queue, output_img, CL_TRUE, origin, region,
		0, 0, (void*)output_pixels, 0, NULL, NULL);
	if (err < 0) error(err, "clEnqueueReadImage");

	/* Create output image */
	write_image_data(OUTPUT, output_pixels, width, height);

	/* Dealloc */
	free(input_pixels);
	free(output_pixels);
	clReleaseMemObject(input_img);
	clReleaseMemObject(output_img);
	clReleaseKernel(g_kernel);
	clReleaseCommandQueue(queue);
	clReleaseProgram(prog);
	clReleaseContext(ctx);
	return 0;
}



