/**
 * File              : packit.c
 * Author            : Robin Krens <robin@robinkrens.nl>
 * Date              : 15.07.2020
 * Last Modified Date: 15.07.2020
 * Last Modified By  : Robin Krens <robin@robinkrens.nl>
 *
 * Packit! is a simple tool for Atari Lynx to convert
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
	uint8_t color; /* Palette color 0-16 */
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

int data_packet_line(int rowlength, uint8_t * data, int data_len)
{

	int offset = 0;
	uint8_t * top;
	uint8_t * linesprite = (uint8_t *) malloc(256);
	bzero(linesprite, 0);

	top = linesprite;

	/* offset to next line of sprite */
	*linesprite = rowlength/2 + 3;
	linesprite++;
	offset++;

	/* first bit: literal */
	*linesprite = 1 << 7;

	/* number of pixels + 1 (rowlength) */
	*linesprite |= rowlength-1 << 3;

	//linesprite++;
	//offset++;

	/* pixel data */
	for (int i = 0; i < data_len;) {
		//*linesprite = *data << 7;
		*linesprite |= *data >> 1;
		linesprite++;
		offset++;

		*linesprite = (*data & 0x01) << 7;

		data++;

		if (i + 1 < data_len) {
			*linesprite |= *data << 3;
		//	linesprite++;
		//	offset++;
		}

		data++;
		
		i += 2;
	}

	/* padding */
	//linesprite++;
	offset++;

	for (int i = 0; i < offset; i++)
		printf("%0X:", top[i]);

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

	for (int h = 0; h < rawbmp->h; h++) {

		uint8_t * top;
		uint8_t * linebuf = (uint8_t *) malloc(rawbmp->w);
		top = linebuf;
		
		for (int w = 0; w < rawbmp->w; w++) {

			SDL_LockSurface(rawbmp);
			/* 24-bit pixel data are loaded as blue, green, red */
			index_b = * (uint8_t * ) rawbmp->pixels++;
			index_g = * (uint8_t * ) rawbmp->pixels++;
			index_r = * (uint8_t * ) rawbmp->pixels++;
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

		packed_data_t * prev;
		packed_data_t * l = scan_line(top, rawbmp->w);

		printf("new line\n");

		while(l->next != NULL) {
			printf("repeat: %d, color: %x\n", l->repeatcount, l->color);
			prev = l;	
			l = l->next;
			free(prev);	
		}
		printf("repeat: %d, color: %x\n", l->repeatcount, l->color);
		free(l);
		free(top);

		rawbmp->pixels += padding;
	}
		
	fprintf(stdout, "Image has %d colors\n", colorcnt);
	

//	uint8_t buff[8];
//	buff[0] = 0xAA;
//	buff[1] = 0x22;
//	buff[2] = 0x22;
//	buff[3] = 0x33;
//	buff[4] = 0x33;
//	buff[5] = 0x33;
//	buff[6] = 0x33;
//	buff[7] = 0xAA;



//	uint8_t buf[5];
//
//	buf[0] = 0x0F;
//	buf[1] = 0x0F;
//	buf[2] = 0x0F;
//	buf[3] = 0x0F;
//	buf[4] = 0x0F;
//
//	data_packet_line(5, buf, 5);
//
	//SDL_FreeSurface(rawbmp);

	return 0;
}
