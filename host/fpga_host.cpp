#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <arm_neon.h>
#include "CL/opencl.h"
#include "AOCL_Utils.h"

using namespace aocl_utils;

// OpenCL runtime configuration
cl_platform_id platform = NULL;
