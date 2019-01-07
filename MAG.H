
#if !defined(MAG_H)
# define MAG_H

#include "types.h"

#define MAG_ID			"MAKI02  "

#define MAG_200L		0x01
#define MAG_8C			0x02
#define MAG_DIGITAL		0x04
#define MAG_256C		0x80

typedef struct {
	uchar	dummy;
	uchar	machine_id;
	uchar	machine_dependent_flag;
	uchar	screen_mode;
	uint	start_x;
	uint	start_y;
	uint	end_x;
	uint	end_y;
	ulong	flag_a_pos;
	ulong	flag_b_pos;
	ulong	flag_b_size;
	ulong	pixel_pos;
	ulong	pixel_size;
} maghead_t;

typedef struct {
	uchar	g;
	uchar	r;
	uchar	b;
} palette_t;

#endif	/* MAG_H */

