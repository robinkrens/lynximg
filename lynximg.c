/**
 * File              : lynximg.c
 * Author            : Robin Krens <robin@robinkrens.nl>
 * Date              : 15.07.2020
 * Last Modified Date: 19.07.2020
 * Last Modified By  : Robin Krens <robin@robinkrens.nl>
 *
 * lynximg is a simple tool for Atari Lynx to convert
 * bitmap files to a format that is understood by the
 * Atari Lynx.
 *
 * Currently supported:
 * 	- Uncompressed 24-bit .bmp files
 * 	- Compressed 24-bit .bmp files
 * 
 * You can use GIMP to export .bmp as 24 (r8/b8/g8) bits .bmp
 * files. 
 *
 * Output is 4 bits per pixels (define as in Sprite Control
 * Block as BPP4) 
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <SDL2/SDL_image.h>

#define MAX_COLORS 256
#define VERBOSE 0

typedef struct packed_data {
	int repeatcount;
	uint8_t color; /* Palette color 0-15 */
	struct packed_data * next;
} packed_data_t;

packed_data_t * scan_line(uint8_t * data, int data_len) 
{
	struct packed_data * top;
	struct packed_data * current = malloc(sizeof(packed_data_t));

	top = current;

	current->repeatcount = 1;
	current->color = *data;
	current->next = NULL;

	data++;

	for (int i = 0; i < data_len-1; i++) {
		
		if(*data == current->color) {
			current->repeatcount++;
		}
		else {
			current->next = malloc(sizeof(packed_data_t));
			current = current->next;
			current->repeatcount = 1;
			current->color = *data;
			current->next = NULL;
		}

		data++;
	}
	
	return top;

}


int print_image_stats(SDL_Surface * img)
{

	SDL_PixelFormat * fmt;
	SDL_Color * color;

	fmt = img->format;

	fprintf(stdout,"WIDTH: %d, HEIGHT: %d, BPP: %d\n", img->w, img->h, fmt->BitsPerPixel);

	return 0;
}

int check_unique(int color, int * palette, int current_count) 
{

	for (int i = 0; i < current_count; i++) {
		if (palette[i] == color)
			return 0;
	}
	return 1;
}

int get_paletteindex(int color, int * palette, int current_count) 
{
	for (int idx = 0; idx < current_count; idx++) {
		if (palette[idx] == color)
			return idx;
	}
	fprintf(stderr, "can't find color");
	return -1;
}

uint8_t * set_bits(uint8_t * byteoffset, int * currentbit,  uint8_t mask) 
{

	if (*currentbit > 4) {
		int remainder = *currentbit - 4;
		*byteoffset |= mask >> remainder;
		byteoffset++;
		*byteoffset = mask << (8 - remainder);
		*currentbit -= 4;
	}
	else {
		*byteoffset |= mask << (4 - *currentbit);
		*currentbit += 4;
		if (*currentbit > 7) {
			*currentbit = 0;
			byteoffset++;
		}

	}

	return byteoffset;

}

uint8_t * set_literal(uint8_t * byteoffset, int * currentbit, uint8_t mask, int cnt) 
{

//	printf("color: %d\n", mask);
	byteoffset = set_bits(byteoffset, currentbit, cnt);
	byteoffset = set_bits(byteoffset, currentbit, mask);

	//while (cnt > 0) {
	//	byteoffset = set_bits(byteoffset, currentbit, mask);
	//	cnt--;
	//}

	return byteoffset;
}

int pack_line(packed_data_t * data)
{
	//int offset = 0;
	int bitindex = 0;
	uint8_t tmpbuf[256] = {};
	bzero(tmpbuf, 0);
	uint8_t * p = tmpbuf;
	uint8_t * top = tmpbuf;

	packed_data_t * datatop, * dataprev;
	datatop = data;
	//printf("line\n");

	/* first entry is reserved for offset */
	*p++ = 0xFF;

	while(data) {

		//printf("Repeat Count: %d, Color: %d\n", data->repeatcount, data->color);
		int count = data->repeatcount;

		while(count > 16) {
			bitindex++;
			if (bitindex > 7) {
				bitindex = 0;
				p++;
			}
			p = set_bits(p, &bitindex, 0x0F);
			p = set_bits(p, &bitindex, data->color);

			count -= 16;
		}
		
		if (count == 1) {
			*p |= 0x1 << (7 - bitindex);
			bitindex++;
			if (bitindex > 7) {
				bitindex = 0;
				p++;
			}
	
			p = set_literal(p, &bitindex, data->color, count - 1);
		
		}
		else {
			bitindex++;
			if (bitindex > 7) {
				bitindex = 0;
				p++;
			}
			
			p = set_bits(p, &bitindex, count-1);
			p = set_bits(p, &bitindex, data->color);
		}

		data = data->next;

	}

	//int offset = 0;
	//while(*top++ != 0x0) {
	//	offset++;
	//}

	int size =  p - tmpbuf + 1;

	top = tmpbuf;
	top[0] = size;

	/* Hardware bug correction */
	if (top[size-1] & 0x1) {
		printf("correcting hardware bug");
	}

//	while (*top != 0x0) {
//		printf("0x%02x, ", *top++);
//	}

	for (int i = 0; i < size; i++)
		printf("0x%02x, ", top[i]);
	
	//printf("\n");
		while(datatop->next != NULL) {
	//		printf("repeat: %d, color: %x\n", l->repeatcount, l->color);
			dataprev = datatop;	
			datatop = datatop->next;
			free(dataprev);	
		}
	//	printf("repeat: %d, color: %x\n", l->repeatcount, l->color);
		free(datatop);
		//free(top);


	return 0;
}

int main() 
{

	SDL_Surface * rawbmp;
	SDL_Color * color;
	SDL_PixelFormat * fmt;
	int index_r, index_g, index_b;
	int colorcnt = 0;
	int palettebuf[MAX_COLORS];

	rawbmp = SDL_LoadBMP("brick.bmp");
	
	if(!rawbmp) {
		fprintf(stderr, "Can't load bmp file!\n");
		return -1;
	}

	if(rawbmp->format->BitsPerPixel != 24) {
		fprintf(stderr, "Bits per pixel: %d not supported\n", rawbmp->format->BitsPerPixel);
		return -1;
	}

	print_image_stats(rawbmp);
	fmt = rawbmp->format;

	if (fmt->palette) {
		fprintf(stderr, "Can't handle palette BMP!\n");
		return -1;
	}

	/* Padding; currently only 24-bit (3 byte) supported  */
	int padding = ceil((float)rawbmp->w * 3/4) * 4 - (rawbmp->w * 3);


	SDL_LockSurface(rawbmp);
	rawbmp->userdata = rawbmp->pixels;
	SDL_UnlockSurface(rawbmp);
	
	
	for (int h = 0; h < rawbmp->h; h++) {

		uint8_t * top;
		uint8_t * linebuf = (uint8_t *) malloc(rawbmp->w);
		top = linebuf;
		
		for (int w = 0; w < rawbmp->w; w++) {

			SDL_LockSurface(rawbmp);
			/* 24-bit pixel data are loaded as blue, green, red */
			index_b = * (uint8_t * ) rawbmp->userdata++;
			index_g = * (uint8_t * ) rawbmp->userdata++;
			index_r = * (uint8_t * ) rawbmp->userdata++;
			SDL_UnlockSurface(rawbmp);

			/* Check unique color */
			if (check_unique(((index_b << 16) | (index_g << 8) | index_r), palettebuf, colorcnt)) {
				if (colorcnt + 1 > MAX_COLORS) {
					fprintf(stderr, "too many colors in bmp\n");
					return -1;
				}
				if (colorcnt + 1 > 16) {
					fprintf(stderr, "warning: image has over 16 different colors\n");
				}
				palettebuf[colorcnt++] = ((index_b << 16) | (index_g << 8) | index_r);
			}
			
			if (VERBOSE)
				fprintf(stdout, "height:width: %d:%d, B-G-R: %d, %d, %d\n", h, w, index_b, index_g, index_r);

			int idx = get_paletteindex(((index_b << 16) | (index_g << 8) | index_r), palettebuf, colorcnt);
			//printf("idx: %d\n", idx);
			*linebuf++ = idx;
			
		}

		packed_data_t * prev, * c;
		packed_data_t * l = scan_line(top, rawbmp->w);

		c = l;

		pack_line(l);

		l = c;

		SDL_LockSurface(rawbmp);
		rawbmp->userdata += padding;
		SDL_UnlockSurface(rawbmp);
		free(top);
	}
		
	fprintf(stdout, "Image has %d colors\n", colorcnt);
	

	SDL_FreeSurface(rawbmp);

	return 0;
}
