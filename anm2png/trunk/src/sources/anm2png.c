#include <stdio.h>
#include <stdlib.h>
#include "png.h"

typedef signed __int8		int8_t;
typedef unsigned __int8		uint8_t;
typedef signed __int16		int16_t;
typedef unsigned __int16	uint16_t;
typedef signed __int32		int32_t;
typedef unsigned __int32	uint32_t;

typedef struct {
	uint32_t regnum;
	uint32_t attrnum;
	uint32_t resd08;
	uint32_t width;
	uint32_t height;
	uint32_t resd14;
	uint32_t resd18;
	uint32_t nameofs;
	uint32_t resd20;
	uint32_t resd24;
	uint32_t version;
	uint32_t resd2c;
	uint32_t entofs;
	uint32_t resd34;
	uint32_t nextofs;
	uint32_t resd3c;
} ANM_ENTRY_HEADER;

typedef struct {
	uint32_t version;
	uint16_t regnum;
	uint16_t attrnum;
	uint16_t resw08;
	uint16_t width;
	uint16_t height;
	uint16_t resw0e;
	uint32_t nameofs;
	uint32_t resd14;
	uint32_t resd18;
	uint32_t entofs;
	uint32_t resd20;
	uint32_t nextofs;
	uint8_t  resx28[24];
} ANM_ENTRY_HEADER_TH11;

typedef struct {
	uint32_t magic;
	uint16_t resd04;
	uint16_t type;
	uint16_t width;
	uint16_t height;
	uint32_t size;
} ANM_THTX_HEADER;

typedef struct {
	uint32_t id;
	float left;
	float top;
	float right;
	float bottom;
} ANM_REGION;

int thtx_to_png(ANM_THTX_HEADER *head, png_byte *bmp, char *outname)
{
	char basename[_MAX_FNAME];
	char ext[_MAX_EXT];
	char newoutname[_MAX_PATH];
	char *poutname;
	FILE *fp;
	png_structp	png_ptr;
	png_infop info_ptr;
	png_byte **image;
	png_byte *row;
	int i, j;

	poutname = outname;
	fp = fopen(poutname, "r");
	if(fp != NULL) {
		fclose(fp);
		_splitpath(poutname, NULL, NULL, basename, ext);
		for(i = 0; i < INT_MAX; ++i) {
			sprintf(newoutname, "%s_%d%s", basename, i, ext);
			fp = fopen(newoutname, "r");
			if(fp == NULL) break;
			fclose(fp);
		}
		if(i == INT_MAX) return 1;
		poutname = newoutname;
	}

	fp = fopen(poutname, "wb");
	if(fp == NULL) {
		perror("fopen");
		return 1;
	}
	png_ptr  = png_create_write_struct(
		PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	info_ptr = png_create_info_struct(png_ptr);
	png_init_io(png_ptr, fp);

	switch(head->type) {
	case 1:
		png_set_IHDR(png_ptr, info_ptr, head->width, head->height,
			8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_set_bgr(png_ptr);
		png_write_info(png_ptr, info_ptr);
			
		image = malloc(sizeof(png_bytep) * head->height);
		for(i = 0; i < head->height; ++i) 
			image[i] = bmp + i * head->width * 4;
		png_write_image(png_ptr, image);
		free(image);
		break;
	case 3:
		png_set_IHDR(png_ptr, info_ptr, head->width, head->height,
			8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_write_info(png_ptr, info_ptr);
		row = malloc(sizeof(png_bytep) * head->width * 3);
		for(i = 0; i < head->height; ++i) {
			for(j = 0; j < head->width; ++j) {
				char *p = bmp + (i * head->width + j) * 2;
				row[j * 3 + 0] = p[1] & 0xF8;
				row[j * 3 + 1] = (p[1] << 5) | ((p[0] & 0xe0) >> 3);
				row[j * 3 + 2] = p[0] << 3;
			}
			png_write_row(png_ptr, row);
		}
		free(row);
		break;
	case 5: 
		png_set_IHDR(png_ptr, info_ptr, head->width, head->height,
			8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_write_info(png_ptr, info_ptr);
		row = malloc(sizeof(png_bytep) * head->width * 4);
		for(i = 0; i < head->height; ++i) {
			png_byte *r, *g, *b, *a;
			for(j = 0; j < head->width; ++j) {
				char *p = bmp + (i * head->width + j) * 2;
				unsigned long int value=0;
				r= &row[j * 4 + 0], g=&row[j * 4 + 1], b=&row[j * 4 + 2], a=&row[j * 4 + 3];
				/*anm- BGRA4444 format*/

				*b= 0x0F & p[0];
				*b= *b |*b << 4;
				
				*g= 0xF0 & p[0];
				*g= *g |*g >> 4;

				*r= 0x0F & p[1];
				*r= *r | *r << 4;

				*a= 0xF0 & p[1];
				*a= *a | *a >> 4;


			}
			png_write_row(png_ptr, row);
		}
		free(row);
		break;
	}

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
	printf("succeeded.\n");
	return 0;
}

int read_region_info(FILE *fp, uint32_t baseofs, uint32_t count, ANM_REGION **reg)
{
	uint32_t   *ofsbuf;
	ANM_REGION *regbuf;
	uint32_t i;

	if(count == 0) return 0;

	ofsbuf = malloc(count * sizeof(uint32_t));
	if(!ofsbuf) {
		perror("malloc");
		return 1;
	}
	if(fread(ofsbuf, sizeof(uint32_t), count, fp) != count) {
		if(ferror(fp)) perror("fread");
		else fprintf(stderr, "error : file may be corrupted\n");
		return 1;
	}
	regbuf = malloc(count * sizeof(ANM_REGION));
	if(!regbuf) {
		perror("malloc");
		free(ofsbuf);
		return 1;
	}
	for(i = 0; i < count; ++i) {
		if(fseek(fp, baseofs + ofsbuf[i], SEEK_SET)) {
			perror("fseek");
			free(regbuf);
			free(ofsbuf);
			return 1;
		}
		if(fread(regbuf + i, sizeof(ANM_REGION), 1, fp) != 1) {
			if(ferror(fp)) perror("fread");
			else fprintf(stderr, "error : file may be corrupted\n");
			free(regbuf);
			free(ofsbuf);
			return 1;
		}
	}

	*reg = regbuf;
	return 0;
}

int read_cstr(char *buf, int max, FILE *fp)
{
	int i, c = -1;
	for(i = 0; i < (max - 1) && c; ++i) {
		c = fgetc(fp);
		if(c == EOF) {
			if(ferror(fp)) return -2;
			if(feof(fp)) return -1;
			return -3;
		}
		buf[i] = c;
	}
	buf[i] = 0;
	return i;
}

void remap_header_th11(ANM_ENTRY_HEADER *anm)
{
	ANM_ENTRY_HEADER_TH11 nanm;
	memcpy(&nanm, anm, sizeof(nanm));
	anm->attrnum = nanm.attrnum;
	anm->regnum  = nanm.regnum;
	anm->width   = nanm.width;
	anm->height  = nanm.height;
	anm->nameofs = nanm.nameofs;
	anm->version = nanm.version;
	anm->entofs  = nanm.entofs;
	anm->nextofs = nanm.nextofs;
}

int main(int argc, char *argv[])
{
	FILE *fp;
	ANM_ENTRY_HEADER anm;
	ANM_THTX_HEADER  thtx;
	ANM_REGION *regbuf;
	char name[_MAX_PATH];
	char *filename;
	char *entbuf;
	long filelen;
	long namelen;
	uint32_t i;
	uint32_t baseofs;

	if (argc != 2) {
		printf(
			"usage: anm2png FILE\n"
			"Convert Touhou Animation FILE to PNG\n");
		return EXIT_SUCCESS;
	}
	
	fp = fopen(argv[1], "rb");
	if(fp == NULL) {
		perror("fopen");
		return EXIT_FAILURE;
	}
	if(fseek(fp, 0, SEEK_END)) {
		perror("fseek");
		return EXIT_FAILURE;
	}
	if((filelen = ftell(fp)) == -1) {
		perror("ftell");
		return EXIT_FAILURE;
	}
	rewind(fp);

	baseofs = 0x0;
	do {
		if(fseek(fp, baseofs, SEEK_SET)) {
			perror("fseek");
			return EXIT_FAILURE;
		}
		if(fread(&anm, 0x40, 1, fp) != 1) {
			if(ferror(fp)) perror("fread");
			else fprintf(stderr, "error : file may be corrupted\n");
			return EXIT_FAILURE;
		}
		if(anm.version != 2 && anm.version != 3 && anm.version != 4) {
			remap_header_th11(&anm);
			if(anm.version != 0x7) {
				fprintf(stderr, "error : version not supported. version: %d\n", anm.version);
				return EXIT_FAILURE;
			}
		}
		regbuf = NULL;
		if(read_region_info(fp, baseofs, anm.regnum, &regbuf)) {
			return EXIT_FAILURE;
		}
		if(fseek(fp, baseofs + anm.nameofs, SEEK_SET)) {
			perror("fseek");
			return EXIT_FAILURE;
		}
		namelen = read_cstr(name, _MAX_PATH, fp);
		if(namelen < 0) {
			if(namelen != -1) perror("read_cstr");
			else fprintf(stderr, "error : file may be corrupted\n");
			return EXIT_FAILURE;
		}
		if(fseek(fp, baseofs + anm.entofs, SEEK_SET)) {
			perror("fseek");
			return EXIT_FAILURE;
		}
		if(fread(&thtx, sizeof(thtx), 1, fp) != 1) {
			perror("fread");
			return EXIT_FAILURE;
		}
		if(thtx.magic == 'XTHT' && (thtx.type == 1 || thtx.type == 3 || thtx.type == 5)) {
			entbuf = malloc(thtx.size);
			if(!entbuf) {
				perror("malloc");
				return EXIT_FAILURE;
			}
			if(fread(entbuf, thtx.size, 1, fp) != 1) {
				if(ferror(fp)) perror("fread");
				else fprintf(stderr, "error : file may be corrupted\n");
				return EXIT_FAILURE;
			}
			printf("----------------\n");
			printf("Filename: %s\n", name);
			printf("Type    : %s\n", 
				thtx.type == 1 ? "1/BGRA8888":
				thtx.type == 3 ? "3/BGR565":
				thtx.type == 5 ? "5/BGRA4444":
				"unknown");
			printf("Size    : %d x %d\n", thtx.width, thtx.height);
			printf("Regions :\n");
			for(i = 0; i < anm.regnum; ++i) {
				printf("ID: %08x (%.f,%.f)-Step(%.f,%.f)\n",
					regbuf[i].id,
					regbuf[i].left, regbuf[i].top, 
					regbuf[i].right, regbuf[i].bottom);
			}
			filename = strrchr(name, '/');
			if(filename != NULL) ++filename;
			else filename = name;
			
			printf("Converting...\n", name);
			thtx_to_png(&thtx, entbuf, filename);
			free(entbuf);
		}
		free(regbuf);

		baseofs += anm.nextofs;
	} while (anm.nextofs);

	fclose(fp);
}

