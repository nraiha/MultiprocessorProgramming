__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
				CLK_ADDRESS_CLAMP_TO_EDGE |
				CLK_FILTER_LINEAR;

__kernel void
grayscale(__read_only image2d_t input, __write_only image2d_t output)
{
	int2 coord = (int2)(get_global_id(0), get_global_id(1));
	int2 size = get_image_dim(input);

	if (all(coord < size)) {
		uint4 pixel = read_imageui(input, sampler, coord);
		float4 color = convert_float4(pixel) / 255;
		/* Formula by CIE */
		color.xyz = 0.2126*color.x + 0.7162*color.y + 0.0722*color.z;
		pixel = convert_uint4_rte(color * 255);
		write_imageui(output, coord, pixel);
	}
}

#if 0
__kernel void
moving_average()
{
	int 2 coord = (int2)(get_global_id(0), get_global_id(2));
	int2 size = get_image_dim(input);
}
#endif
