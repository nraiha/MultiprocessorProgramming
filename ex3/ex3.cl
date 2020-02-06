__kernel void mat_add(__global float* m1, 
		       __global float* m2,
		       __global float* res)
{
	int i = get_global_id(0);
	res[i] = m1[i] + m2[i];

#if 0
	int i = 10*get_global_id(0);
	res[i+0] = m1[i+0] + m2[i+0];
	res[i+1] = m1[i+1] + m2[i+1];
	res[i+2] = m1[i+2] + m2[i+2];
	res[i+3] = m1[i+3] + m2[i+3];
	res[i+4] = m1[i+4] + m2[i+4];
	res[i+5] = m1[i+5] + m2[i+5];
	res[i+6] = m1[i+6] + m2[i+6];
	res[i+7] = m1[i+7] + m2[i+7];
	res[i+8] = m1[i+8] + m2[i+8];
	res[i+9] = m1[i+9] + m2[i+9];
#endif
}

