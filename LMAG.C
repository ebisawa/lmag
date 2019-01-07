
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include "mag.h"
#include "types.h"

#define MODE_SHOW_COMMENT	0x01
#define MODE_SHOW_HEADER	0x02
#define MODE_SHOW_PALETTE	0x04
#define MODE_SHOW_PICTURE	0x08

char *MachineNameTable[] = {
	"unknown",
	"pc98/x68/etc",		/* 0x00 */
	"msx",				/* 0x03 */
	"pst68",			/* 0x68 */
	"pc88",				/* 0x88 */
	"pc98",				/* 0x98 */
	"esequisse/mps",	/* 0xff */
};
ulong HeaderTopPos;

FILE *mag_open(char *msg, maghead_t *head, palette_t *pal, char *filename);
void mag_close(FILE *fp);
int mag_getcolors(uint scr_mode);
int mag_getwidth(maghead_t *head);
int mag_getheight(maghead_t *head);
void mag_palette4bit(palette_t *pal);
void disp_graph(maghead_t *head, palette_t *palette, FILE *fp);
void disp_header(maghead_t *head);
void disp_palette(palette_t *palette);
char *get_machine_name(uint machine_id);
void error(char *msg);
void check_magtype(maghead_t *head);
void out_of_memory(int i);
void set_palette(palette_t *pal);
void graph_on(void);
void graph_off(void);
void graph_init(void);
void graph_showpage(int plane);
void graph_accesspage(int plane);
void usage(void);

/* magla.asm */
void mag_decode_flag(void *flag_x, void *flag_a, void *flag_b);
void mag_decode_pixel(void far *vvram, void *flag_x, void *pixel);
void mag_convert_vram(void far *hvram, void far *vvram);
void mag_copy_vram(void far *hvram);

int main(int argc, char *argv[])
{
	FILE *fp;
	char comment[128], filename[128];
	int i, lmag_mode = MODE_SHOW_PICTURE;
	maghead_t head;
	palette_t palette[16];

	for (i = 1; i < argc; i++) {
		if (*argv[i] == '-') {
			switch (*(argv[i] + 1)) {
			case 'h': case '?':
				usage();
				return EXIT_SUCCESS;
			case 'c':
				lmag_mode ^= MODE_SHOW_COMMENT;
				break;
			case 'v':
				lmag_mode ^= MODE_SHOW_HEADER;
				break;
			case 'p':
				lmag_mode ^= MODE_SHOW_PALETTE;
				break;
			case 'd':
				lmag_mode ^= MODE_SHOW_PICTURE;
				break;
			}
		} else {
			strcpy(filename, argv[i]);
			strlwr(filename);
			if (strchr(filename, '.') == NULL)
				strcat(filename, ".mag");

			printf("(%s)\n", filename);
			if ((fp = mag_open(comment, &head, palette, filename)) == NULL)
				error("MAG ファイルがオープンできません");
			if (lmag_mode & MODE_SHOW_COMMENT)
				printf("%s\n\n", comment);
			if (lmag_mode & MODE_SHOW_HEADER)
				disp_header(&head);
			if (lmag_mode & MODE_SHOW_PALETTE)
				disp_palette(palette);
			if (lmag_mode & MODE_SHOW_PICTURE)
				disp_graph(&head, palette, fp);

			mag_close(fp);
		}
	}
	return EXIT_SUCCESS;
}

FILE *mag_open(char *msg, maghead_t *head, palette_t *pal, char *filename)
{
	FILE *fp;
	uchar *p = msg;

	if ((fp = fopen(filename, "rb")) == NULL)
		return NULL;
	while ((*p = fgetc(fp)) != 0x1a)
		p++;

	*p = 0;
	if (strncmp(msg, MAG_ID, strlen(MAG_ID)) != 0) {
		fclose(fp);
		return NULL;
	}

	HeaderTopPos = ftell(fp);
	if (fread(head, sizeof(maghead_t), 1, fp) == 0) {
		fclose(fp);
		return NULL;
	}
	if (fread(pal, sizeof(palette_t), 16, fp) != 16) {
		fclose(fp);
		return NULL;
	}
	return fp;
}

void mag_close(FILE *fp)
{
	fclose(fp);
}

int mag_getcolors(uint scr_mode)
{
	if (scr_mode & MAG_8C)
		return 8;
	if (scr_mode & MAG_256C)
		return 256;

	return 16;
}

int mag_getwidth(maghead_t *head)
{
	return head->end_x - head->start_x + 1;
}

int mag_getheight(maghead_t *head)
{
	return head->end_y - head->start_y + 1;
}

void mag_palette4bit(palette_t *pal)
{
	int i;

	for (i = 0; i < 16; i++) {
		pal[i].r >>= 4;
		pal[i].g >>= 4;
		pal[i].b >>= 4;
	}
}

void disp_graph(maghead_t *head, palette_t *palette, FILE *fp)
{
	uint flag_a_size, vvram_segm, hvram_segm;
	uchar *flag_a, *flag_b, *flag_x, *pixel;
	uchar far *vvram, far *hvram;

	check_magtype(head);

	/* フラグデータ展開 */
	flag_a_size = head->flag_b_pos - head->flag_a_pos;
	if ((flag_x = malloc(32000 + 80)) == NULL)
		out_of_memory(1);
	if ((flag_a = malloc(flag_a_size)) == NULL)
		out_of_memory(2);
	if ((flag_b = malloc(head->flag_b_size)) == NULL)
		out_of_memory(3);

	fseek(fp, HeaderTopPos + head->flag_a_pos, SEEK_SET);
	fread(flag_a, flag_a_size, 1, fp);
	fseek(fp, HeaderTopPos + head->flag_b_pos, SEEK_SET);
	fread(flag_b, head->flag_b_size, 1, fp);

	mag_decode_flag(flag_x, flag_a, flag_b);

	free(flag_b);
	free(flag_a);

	/* ピクセルデータ展開 */
	if ((pixel = malloc(head->pixel_size)) == NULL)
		out_of_memory(4);
	if (_dos_allocmem((ulong)640 * 400 / 2 / 16, &hvram_segm) != 0)
		out_of_memory(5);
	if (_dos_allocmem((ulong)640 * 400 / 2 / 16, &vvram_segm) != 0)
		out_of_memory(6);

	hvram = MK_FP(hvram_segm, 0);
	vvram = MK_FP(vvram_segm, 0);
	fseek(fp, HeaderTopPos + head->pixel_pos, SEEK_SET);
	fread(pixel, head->pixel_size, 1, fp);

	mag_decode_pixel(vvram, flag_x + 80, pixel);

	free(pixel);

	/* VRAM 垂直→水平変換 */
	mag_convert_vram(hvram, vvram);

	_dos_freemem(vvram_segm);

	/* VRAM 転送 */
	graph_off();
	graph_init();
	graph_accesspage(0);
	graph_showpage(1);
	mag_palette4bit(palette);
	set_palette(palette);

	mag_copy_vram(hvram);

	graph_showpage(0);
	graph_on();

	_dos_freemem(hvram_segm);
}

void disp_header(maghead_t *head)
{
	printf("[header]\n");
	printf("  machine id : %02x (%s)\n",
		head->machine_id, get_machine_name(head->machine_id));
	printf("        flag : %02x\n", head->machine_dependent_flag);
	printf(" screen mode : %02x (%d colors%s%s)\n",
		head->screen_mode, mag_getcolors(head->screen_mode),
		(head->screen_mode & MAG_DIGITAL) ? ",digital"  : "",
		(head->screen_mode & MAG_200L)    ? ",200lines" : "");
	printf("      region : (%d,%d)-(%d,%d)\n",
		head->start_x, head->start_y, head->end_x, head->end_y);
	printf("  flag a pos : %-5lu (%lx)\n", head->flag_a_pos, head->flag_a_pos);
	printf("  flag b pos : %-5lu (%lx)\n", head->flag_b_pos, head->flag_b_pos);
	printf("        size : %lu\n", head->flag_b_size);
	printf("   pixel pos : %-5lu (%lx)\n", head->pixel_pos, head->pixel_pos);
	printf("        size : %lu\n", head->pixel_size);
	putchar('\n');
}

void disp_palette(palette_t *palette)
{
	int i;

	printf("[palette]\n");
	for (i = 0; i < 8; i ++) {
		printf(" %2d: r=%02x(%2d) g=%02x(%2d) b=%02x(%2d)", i,
			palette[i].r, (palette[i].r >> 4),
			palette[i].g, (palette[i].g >> 4),
			palette[i].b, (palette[i].b >> 4));
		printf("    %2d: r=%02x(%2d) g=%02x(%2d) b=%02x(%2d)\n", i + 8,
			palette[i + 8].r, (palette[i + 8].r >> 4),
			palette[i + 8].g, (palette[i + 8].g >> 4),
			palette[i + 8].b, (palette[i + 8].b >> 4));
	}
	putchar('\n');
}

char *get_machine_name(uint machine_id)
{
	switch (machine_id) {
	case 0x00:
		return MachineNameTable[1];
	case 0x03:
		return MachineNameTable[2];
	case 0x68:
		return MachineNameTable[3];
	case 0x88:
		return MachineNameTable[4];
	case 0x98:
		return MachineNameTable[5];
	case 0xff:
		return MachineNameTable[6];
	default:
		return MachineNameTable[0];
	}
}

void error(char *msg)
{
	printf("Error: %s\n", msg);
	exit(EXIT_FAILURE);
}

void check_magtype(maghead_t *head)
{
	if (mag_getcolors(head->screen_mode) != 16)
		error("16色画像にしか対応していません");
	if (mag_getwidth(head) != 640 || mag_getheight(head) != 400)
		error("640x400 画像にしか対応していません");
}

void out_of_memory(int i)
{
	printf("(%d) ", i);
	error("メモリが足りません");
}

void set_palette(palette_t *pal)
{
	int i;

	for (i = 0; i < 16; i++) {
		outportb(0xa8, i);
		outportb(0xaa, pal[i].g);
		outportb(0xac, pal[i].r);
		outportb(0xae, pal[i].b);
	}
}

void graph_on(void)
{
	_asm {
		mov		ah, 40h
		int		18h
	}
}

void graph_off(void)
{
	_asm {
		mov		ah, 41h
		int		18h
	}
}

void graph_init(void)
{
	_asm {
		mov		ah, 42h
		mov		ch, 0c0h
		int		18h
	}
	outportb(0x6a, 1);
}

void graph_showpage(int plane)
{
	outportb(0xa4, plane);
}

void graph_accesspage(int plane)
{
	outportb(0xa6, plane);
}

void usage(void)
{
	puts("lmag 0.4 - (c)ebisawa 1996, ncdw project.");
	puts("usage: lmag [options..] [filenames..]");
	puts("option: -c   show comment");
	puts("        -v   show header");
	puts("        -p   show palette");
	puts("        -d * show picture");
}

