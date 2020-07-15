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

int main() 
{
	SDL_Surface * bmpfile;
	SDL_Color * color;
	SDL_PixelFormat * fmt;
	int index_r, index_g, index_b;
	int colorcnt = 0;
	int palettebuf[MAX_COLORS];

	bmpfile = SDL_LoadBMP("brick.bmp");

	if(!bmpfile) {
		fprintf(stderr, "Can't load bmp file!\n");
		return -1;
	}

	if(bmpfile->format->BitsPerPixel != 24) {
		fprintf(stderr, "Bits per pixel: %d not supported\n", bmpfile->format->BitsPerPixel);
		return -1;
	}

	print_image_stats(bmpfile);
	fmt = bmpfile->format;

	if (fmt->palette) {
		fprintf(stderr, "Can't handle palette BMP!\n");
		return -1;
	}

	/* Padding; currently only 24-bit (3 byte) supported  */
	int padding = ceil((float)bmpfile->w * 3/4) * 4 - (bmpfile->w * 3);

	for (int h = 0; h < bmpfile->h; h++) {

		for (int w = 0; w < bmpfile->w; w++) {

			SDL_LockSurface(bmpfile);
			/* 24-bit pixel data are loaded as blue, green, red */
			index_b = * (uint8_t * ) bmpfile->pixels++;
			index_g = * (uint8_t * ) bmpfile->pixels++;
			index_r = * (uint8_t * ) bmpfile->pixels++;
			SDL_UnlockSurface(bmpfile);

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
		}

		bmpfile->pixels += padding;
	}
		
	fprintf(stdout, "Image has %d colors\n", colorcnt);

	//SDL_FreeSurface(bmpfile);

	return 0;
}
