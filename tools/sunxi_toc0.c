// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2018 Arm Ltd.
 */

#include "imagetool.h"
#include <image.h>
#include "mkimage.h"

//#include "../arch/arm/include/asm/arch-sunxi/spl.h"

/* checksum initialiser value */
#define STAMP_VALUE                     0x5F0A6C39
#define TOC0_SIGNATURE			"TOC0.GLH"
#define TOC0_MAGIC			0x89119800
#define TOC0_HEAD_END			"MIE;"
/* toc0_head + 2 * toc0_item + padding + certificate + padding */
#define TOC0_HEADER_LENGTH		0x320

#define	TOC0_CERTIFICATE_LENGTH		0x282

/* One toc0_head, then <num_items> of struct toc0_items. */

struct toc0_head {
	uint8_t signature[8];
	uint32_t magic;
	uint32_t check_sum;
	uint32_t serial;		/* 0x10 */
	uint32_t status;
	uint32_t num_items;
	uint32_t length;
	uint32_t boot_media;		/* 0x20 */
	uint8_t reserved[8];
	uint8_t end_marker[4];
};

#define ITEM_ID_CERTIFICATE		0x00010101
#define ITEM_ID_BOOT_CODE		0x00010202
#define ITEM_TYPE_CERTIFICATE		1
#define ITEM_TYPE_CODE			2
#define ITEM_END			"IIE;"

struct toc0_item {
	uint32_t id;
	uint32_t offset;
	uint32_t length;
	uint32_t status;
	uint32_t type;
	uint32_t load_addr;
	uint8_t reserved[4];
	uint8_t end_marker[4];
};

/*
 * NAND requires 8K padding. SD/eMMC gets away with 512 bytes,
 * but let's use the larger padding to cover both.
 */
#define PAD_SIZE			8192

static int toc0_check_params(struct image_tool_params *params)
{
	return !params->dflag;
}

static int toc0_verify_header(unsigned char *ptr, int image_size,
			      struct image_tool_params *params)
{
	const struct toc0_head *header = (void *)ptr;

	if (memcmp(header->signature, TOC0_SIGNATURE, 8))
		return EXIT_FAILURE;
	if (header->magic != TOC0_MAGIC)
		return EXIT_FAILURE;

	/* Must be at least 512 byte aligned. */
	if (header->length & 511)
		return EXIT_FAILURE;

	/*
	 * Image could also contain U-Boot proper, so could be bigger.
	 * But it most not be shorter.
	 */
	if (image_size < header->length)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static void toc0_print_header(const void *buf)
{
	const struct toc0_head *header = buf;

	printf("Allwinner TOC0 header, size: %d bytes\n", header->length);
	printf("\t%d items\n", header->num_items);
}

static void toc0_set_header(void *buf, struct stat *sbuf, int infd,
			    struct image_tool_params *params)
{
	struct toc0_head *header = buf;
	struct toc0_item *item;
	uint32_t *buf32 = buf;
	uint32_t checksum = 0;
	int i;

	memset(buf, 0, 0x80);

	/* First the TOC0 header, announcing two items. */
	memcpy(header->signature, TOC0_SIGNATURE, 8);
	header->magic = TOC0_MAGIC;
	header->check_sum = STAMP_VALUE;
	header->length = (sbuf->st_size + (PAD_SIZE - 1)) & ~(PAD_SIZE -1);
	header->num_items = 2;
	memcpy(header->end_marker, TOC0_HEAD_END, 4);

	/* The first item contains the certificate. */
	item = buf + 0x30;	/* right after the header */
	item->id = ITEM_ID_CERTIFICATE;
	item->offset = 0x80;	/* 32 byte aligned after the header */
	item->length = TOC0_CERTIFICATE_LENGTH;
	item->type = ITEM_TYPE_CERTIFICATE;
	memcpy(item->end_marker, ITEM_END, 4);

	/* The second item contains the actual boot code. */
	item++;
	item->id = ITEM_ID_BOOT_CODE;
	item->offset = TOC0_HEADER_LENGTH;
	item->length = sbuf->st_size - TOC0_HEADER_LENGTH;
	item->type = ITEM_TYPE_CODE;
	item->load_addr = params->addr;
	memcpy(item->end_marker, ITEM_END, 4);

	/* TODO: create the certificate */

	/* Calculate the checksum. Yes, it's that simple. */
	for (i = 0; i < sbuf->st_size / 4; i++)
		checksum += buf32[i];
	header->check_sum = checksum;
}

static int toc0_check_image_type(uint8_t type)
{
	return type == IH_TYPE_SUNXI_TOC0 ? 0 : 1;
}

static int toc0_vrec_header(struct image_tool_params *params,
			    struct image_type_params *tparams)
{
	tparams->hdr = calloc(TOC0_HEADER_LENGTH, 1);

	/* Return padding to 8K blocks. */
	return PAD_SIZE - (params->file_size & (PAD_SIZE - 1));
}

U_BOOT_IMAGE_TYPE(
	sunxi_toc0,
	"Allwinner TOC0 Boot Image support",
	TOC0_HEADER_LENGTH,
	NULL,
	toc0_check_params,
	toc0_verify_header,
	toc0_print_header,
	toc0_set_header,
	NULL,
	toc0_check_image_type,
	NULL,
	toc0_vrec_header
);
