
#include <stdio.h>

#define SRC_I1	0x80
#define SRC_G1	0x40
#define SRC_R1	0x20
#define SRC_B1	0x10
#define SRC_I2	0x08
#define SRC_G2	0x04
#define SRC_R2	0x02
#define SRC_B2	0x01

#define DST1_I1	0x0200
#define DST1_I2	0x0100
#define DST1_G1	0x0002
#define DST1_G2	0x0001
#define DST2_R1	0x0200
#define DST2_R2	0x0100
#define DST2_B1	0x0002
#define DST2_B2	0x0001

main()
{
	int i, data;

	printf("cvtbl1");
	for (i = 0; i < 256; i++) {
		data = 0;
		if (i % 8 == 0)
			printf("\tdw\t");
		if (i & SRC_I1)
			data |= DST1_I1;
		if (i & SRC_I2)
			data |= DST1_I2;
		if (i & SRC_G1)
			data |= DST1_G1;
		if (i & SRC_G2)
			data |= DST1_G2;

		if ((i + 1) % 8 == 0 || i == 255)
			printf("%5d\n", data);
		else
			printf("%5d, ", data);
	}

	printf("cvtbl2");
	for (i = 0; i < 256; i++) {
		data = 0;
		if (i % 8 == 0)
			printf("\tdw\t");
		if (i & SRC_R1)
			data |= DST2_R1;
		if (i & SRC_R2)
			data |= DST2_R2;
		if (i & SRC_B1)
			data |= DST2_B1;
		if (i & SRC_B2)
			data |= DST2_B2;

		if ((i + 1) % 8 == 0 || i == 255)
			printf("%5d\n", data);
		else
			printf("%5d, ", data);
	}
}

