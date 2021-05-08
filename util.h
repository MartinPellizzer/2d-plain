#ifndef _util_h
#define _util_h

#include <stdio.h>

typedef struct vec2i_t
{
	uint32_t x;
	uint32_t y;
} vec2i_t;

typedef struct vec2f_t
{
	float x;
	float y;
} vec2f_t;

char* _itoa(int32_t val)
{
	static char buff[8];
	sprintf(buff, "%d", val);
	return buff;
}

int32_t _ftoi(float num)
{
	return (int32_t)floorf(num);
}

// TODO(martin): this function is for debug purposes only!!
char* concat(char *src_1, char *src_2)
{
	static char dst[20];

	uint32_t i_dst = 0;
	uint32_t i_src = 0;
	while(src_1[i_src] != '\0')
	{
		dst[i_dst] = src_1[i_src];
		i_dst++;
		i_src++;
	}
	i_src = 0;
	while(src_2[i_src] != '\0')
	{
		dst[i_dst] = src_2[i_src];
		i_dst++;
		i_src++;
	}
	dst[i_dst] = '\0';
	
	return dst;
}

float lerp(float a, float b, float t)
{
	return a + t * (b - a);
}


#endif
