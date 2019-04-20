#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctime>
#include <string>
#include <sys/time.h>
#include "CL/opencl.h"
#include "AOCL_Utils.h"
#include "fpga_host.h"

using namespace aocl_utils;

// OpenCL runtime configuration
cl_platform_id platform = NULL;
unsigned num_devices = 0;
scoped_array<cl_device_id> device;
cl_context context = NULL;
scoped_array<cl_command_queue> queue;
cl_program program = NULL;
scoped_array<cl_kernel> kernel; 
scoped_array<cl_mem> input_fares_buf;
scoped_array<cl_mem> output_sum_buf;
unsigned N;

float fpga_avg(int num_of_elements, float *data)
{
	cl_int status;
	float *output;
	float result;
	int num_groups;
	struct timeval kernel_start_time, kernel_end_time;
	N = num_of_elements;
	int i = 0;
	
	init_opencl();
	
	scoped_array<cl_event> kernel_event(num_devices);
	scoped_array<cl_event> finish_event(num_devices);
	
	// Figure out dimensions of problem space
	const size_t global_work_size = 32768;
	const size_t local_work_size = 256;
	num_groups = global_work_size / local_work_size;
	
	output = (float *)malloc(num_groups * sizeof(float));
	
	// Allocate device memory for input buffer
	input_fares_buf[i] = clCreateBuffer(context, CL_MEM_READ_ONLY,
		N * sizeof(float), NULL, &status);
	checkError(status, "Failed to create buffer for input");
	
	// Allocate device memory for output buffer
	output_sum_buf[i] = clCreateBuffer(context, CL_MEM_WRITE_ONLY,
		num_groups * sizeof(float), NULL, &status);
	checkError(status, "Failed to create output bufferf");
	
	// Transfer input to the device
	cl_event write_event[1];
	status = clEnqueueWriteBuffer(queue[i], input_fares_buf[i], CL_FALSE,
		0, N * sizeof(float), data, 0, NULL, &write_event[0]);
	checkError(status, "Failed to transfer input array values");
	
	// Set kernel arguments
	unsigned argi = 0;
	
	status = clSetKernelArg(kernel[i], argi++, sizeof(cl_mem), &input_fares_buf[i]);
	checkError(status, "Failed to set argument %d", argi-1);
	
	status = clSetKernelArg(kernel[i], argi++, sizeof(cl_mem), &output_sum_buf[i]);
	checkError(status, "Failed to set argument %d", argi-1); 

	status = clSetKernelArg(kernel[i], argi++, local_work_size * sizeof(float), NULL);
	checkError(status, "Failed to set argument %d", argi-1);
	
	// Launch the kernel
	status = clEnqueueNDRangeKernel(queue[i], kernel[i], 1, NULL, &global_work_size,
		&local_work_size, 1, write_event, &kernel_event[i]);
	checkError(status, "Failed to launch kernel");
	
	// Read the result into the output array
	status = clEnqueueReadBuffer(queue[i], output_sum_buf[i], CL_FALSE,
			0, num_groups * sizeof(float), output, 1, &kernel_event[i], &finish_event[i]);
			
	// Get the average of the values in the output array
	result = get_average(num_groups, output);
			
	// Release events
	clReleaseEvent(write_event[i]);
	clReleaseEvent(kernel_event[i]);
	clReleaseEvent(finish_event[i]);
	
	cleanup();
	free(output);
	
	return result;
}

float get_average(int num_elements, float *arr)
{
	float sum = 0;
	float avg;
	
	for (int i = 0; i < num_elements; ++i) {
		sum += arr[i];
	}
	
	avg = sum / num_elements;
	return avg;
}

// Initialize the OpenCL objects
bool init_opencl()
{
	cl_int status;
	
	printf("Initializing OpenCL\n");
	
	if (!setCwdToExeDir()) {
		return false;
	}
	
	// Get the OpenCL platform
	platform = findPlatform("Altera");
	if (platform == NULL) {
		printf("ERROR: Unable to find Altera OpenCL platform.\n");
		return false;
	}
	
	// Query the available OpenCL device
	device.reset(getDevices(platform, CL_DEVICE_TYPE_ALL, &num_devices));
	printf("Platform: %s\n", getPlatformName(platform).c_str());
	printf("Using %d device(s)\n", num_devices);
	for(unsigned i = 0; i < num_devices; ++i) {
		printf("  %s\n", getDeviceName(device[i]).c_str());
	}
	
	// Create the context
	context = clCreateContext(NULL, num_devices, device, NULL, NULL, &status);
	checkError(status, "Failed to create context");
	
	// Create the program for the device
	std::string binary_file = getBoardBinaryFile("fpgasum", device[0]);
	printf("Using AOCX: %s\n", binary_file.c_str());
	program = createProgramFromBinary(context, binary_file.c_str(), device, num_devices);
	
	// Build the program that was just created
	status = clBuildProgram(program, 0, NULL, "", NULL, NULL);
	checkError(status, "Failed to build program");
	
	// Create device objects
	queue.reset(num_devices);
	kernel.reset(num_devices);
	input_fares_buf.reset(num_devices);
	
	// I already know there is only one device, so no need to loop through a list of devices
	int i = 0;
	
	// Command queue
	queue[i] = clCreateCommandQueue(context, device[i], CL_QUEUE_PROFILING_ENABLE, &status);
	checkError(status, "Failed to create command queue");
	
	// Kernel
	const char *kernel_name = "fpgasum";
	kernel[i] = clCreateKernel(program, kernel_name, &status);
	checkError(status, "Failed to create kernel");
	
	return true;	
}

void cleanup()
{
	int i = 0;
	
	if(kernel && kernel[i]) {
      clReleaseKernel(kernel[i]);
    }
    if(queue && queue[i]) {
      clReleaseCommandQueue(queue[i]);
    }
    if(input_fares_buf && input_fares_buf[i]) {
      clReleaseMemObject(output_sum_buf[i]);
    }
    if(output_sum_buf && output_sum_buf[i]) {
      clReleaseMemObject(input_fares_buf[i]);
    }
	
	if(program) {
		clReleaseProgram(program);
	}
	if(context) {
		clReleaseContext(context);
	}
}
