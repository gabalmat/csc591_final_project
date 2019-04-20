
__kernel void fpga_add(__global float16 *restrict input,
					   __global float *restrict output,
					   __local float *local_buf)
{ 
	
	float group_sum;
	float16 vector1, vector2, vector_sum;
	unsigned global_id, local_id;
	
	global_id = get_global_id(0) * 2;
	
	vector1 = input[global_id];
	vector2 = input[global_id + 1];
	
	vector_sum = vector1 + vector2;
	
	float8 v_8 = vector_sum.lo + vector_sum.hi;
	float4 v_4 = v_8.lo + v_8.hi;
	float2 v_2 = v_4.lo + v_4.hi;
	
	local_id = get_local_id(0);
	local_buf[local_id] = v_2.x + v_2.y;
	
	barrier(CLK_LOCAL_MEM_FENCE);
	
	if (local_id == 0) {
		
		output[get_group_id(0)] = dot(local_buf[0], 1.0f);
		
	}
	 
}
