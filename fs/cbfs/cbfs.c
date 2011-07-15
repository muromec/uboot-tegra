/*
 * Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <config.h>
#include <cbfs.h>
#include <malloc.h>
#include <asm/byteorder.h>

#define CBFS_TYPE_STAGE      0x10
#define CBFS_TYPE_PAYLOAD    0x20
#define CBFS_TYPE_OPTIONROM  0x30
#define CBFS_TYPE_BOOTSPLASH 0x40
#define CBFS_TYPE_RAW        0x50
#define CBFS_TYPE_VSA        0x51
#define CBFS_TYPE_MBI        0x52
#define CBFS_TYPE_MICROCODE  0x53
#define CBFS_COMPONENT_CMOS_DEFAULT 0xaa
#define CBFS_COMPONENT_CMOS_LAYOUT 0x01aa

typedef struct CbfsHeader {
	u32 magic;
	u32 version;
	u32 romSize;
	u32 bootBlockSize;
	u32 align;
	u32 offset;
	u32 pad[2];
} __attribute__((packed)) CbfsHeader;

typedef struct CbfsFileHeader {
	u8 magic[8];
	u32 len;
	u32 type;
	u32 checksum;
	u32 offset;
} __attribute__((packed)) CbfsFileHeader;

typedef struct CbfsCacheNode {
	CbfsFileHeader header;
	struct CbfsCacheNode *next;
	void *data;
	char *name;
} __attribute__((packed)) CbfsCacheNode;


static const u32 goodMagic = 0x4f524243;
static const u8 goodFileMagic[] = "LARCHIVE";


static int initialized;
static struct CbfsHeader header;
static CbfsCacheNode *fileCache;

static void swap_header(CbfsHeader *dest, CbfsHeader *src)
{
	dest->magic = be32_to_cpu(src->magic);
	dest->version = be32_to_cpu(src->version);
	dest->romSize = be32_to_cpu(src->romSize);
	dest->bootBlockSize = be32_to_cpu(src->bootBlockSize);
	dest->align = be32_to_cpu(src->align);
	dest->offset = be32_to_cpu(src->offset);
}

static void swap_file_header(CbfsFileHeader *dest, CbfsFileHeader *src)
{
	memcpy(&dest->magic, &src->magic, sizeof(dest->magic));
	dest->len = be32_to_cpu(src->len);
	dest->type = be32_to_cpu(src->type);
	dest->checksum = be32_to_cpu(src->checksum);
	dest->offset = be32_to_cpu(src->offset);
}

static int init_check(void)
{
	if (!initialized) {
		printf("CBFS not initialized.\n");
		return 1;
	}
	return 0;
}

static int file_cbfs_fill_cache(u8 *start, u32 size, u32 align)
{
	CbfsCacheNode *cacheNode;
	CbfsCacheNode *newNode;
	CbfsCacheNode **cacheTail = &fileCache;

	/* Clear out old information. */
	cacheNode = fileCache;
	while (cacheNode) {
		CbfsCacheNode *oldNode = cacheNode;
		cacheNode = cacheNode->next;
		free(oldNode->name);
		free(oldNode);
	}
	fileCache = NULL;

	while (size >= align) {
		CbfsFileHeader *fileHeader = (CbfsFileHeader *)start;
		u32 nameLen;
		u32 step;

		/* Check if there's a file here. */
		if (memcmp(goodFileMagic, &(fileHeader->magic),
				sizeof(fileHeader->magic))) {
			size -= align;
			start += align;
			continue;
		}

		newNode = (CbfsCacheNode *)malloc(sizeof(CbfsCacheNode));
		swap_file_header(&newNode->header, fileHeader);
		if (newNode->header.offset < sizeof(CbfsFileHeader) ||
				newNode->header.offset > newNode->header.len) {
			printf("Bad file in CBFS.\n");
			return 1;
		}
		newNode->next = NULL;
		newNode->data = start + newNode->header.offset;
		nameLen = newNode->header.offset - sizeof(CbfsFileHeader);
		/* Add a byte for a NULL terminator. */
		newNode->name = (char *)malloc(nameLen + 1);
		strncpy(newNode->name,
			((char *)fileHeader) + sizeof(CbfsFileHeader),
			nameLen);
		newNode->name[nameLen] = 0;
		*cacheTail = newNode;
		cacheTail = &newNode->next;

		step = newNode->header.len;
		if (step % align)
			step = step + align - step % align;

		size -= step;
		start += step;
	}
	return 0;
}

int file_cbfs_init(uintptr_t endOfRom)
{
	CbfsHeader *headerInRom;
	u8 *startOfRom;
	initialized = 0;

	headerInRom = (CbfsHeader *)(uintptr_t)*(u32 *)(endOfRom - 3);
	swap_header(&header, headerInRom);

	if (header.magic != goodMagic || header.offset >
			header.romSize - header.bootBlockSize) {
		printf("Bad CBFS header.\n");
		return 1;
	}

	startOfRom = (u8 *)(endOfRom + 1 - header.romSize);

	if (file_cbfs_fill_cache(startOfRom + header.offset,
			header.romSize, header.align)) {
		return 1;
	}

	initialized = 1;
	return 0;
}

long file_cbfs_read(const char *filename, void *buffer, unsigned long maxsize)
{
	struct CbfsCacheNode *cacheNode = fileCache;
	u32 size;

	if (init_check())
		return 1;

	while (cacheNode) {
		if (!strcmp(filename, cacheNode->name))
			break;
		cacheNode = cacheNode->next;
	}
	if (!cacheNode) {
		printf("File %s not found.\n", filename);
		return -1;
	}

	printf("reading %s\n", filename);

	size = cacheNode->header.len;
	if (maxsize && size > maxsize)
		size = maxsize;

	memcpy(buffer, cacheNode->data, size);

	return size;
}

int file_cbfs_ls(void)
{
	struct CbfsCacheNode *cacheNode = fileCache;
	int files = 0;

	if (init_check())
		return 1;

	printf("     size              type  name\n");
	printf("------------------------------------------\n");
	while (cacheNode) {
		char *typeName = NULL;
		printf(" %8d", cacheNode->header.len);

		switch (cacheNode->header.type) {
		    case CBFS_TYPE_STAGE:
			typeName = "stage";
			break;
		    case CBFS_TYPE_PAYLOAD:
			typeName = "payload";
			break;
		    case CBFS_TYPE_OPTIONROM:
			typeName = "option rom";
			break;
		    case CBFS_TYPE_BOOTSPLASH:
			typeName = "boot splash";
			break;
		    case CBFS_TYPE_RAW:
			typeName = "raw";
			break;
		    case CBFS_TYPE_VSA:
			typeName = "vsa";
			break;
		    case CBFS_TYPE_MBI:
			typeName = "mbi";
			break;
		    case CBFS_TYPE_MICROCODE:
			typeName = "microcode";
			break;
		    case CBFS_COMPONENT_CMOS_DEFAULT:
			typeName = "cmos default";
			break;
		    case CBFS_COMPONENT_CMOS_LAYOUT:
			typeName = "cmos layout";
			break;
		    case -1UL:
			typeName = "null";
			break;
		}
		if (typeName)
			printf("  %16s", typeName);
		else
			printf("  %16d", cacheNode->header.type);

		if (cacheNode->name[0])
			printf("  %s\n", cacheNode->name);
		else
			printf("  %s\n", "(empty)");
		cacheNode = cacheNode->next;
		files++;
	}

	printf("\n%d file(s)\n\n", files);
	return 0;
}

int file_cbfs_fsinfo(void)
{
	if (init_check())
		return 1;

	printf("\n");
	printf("CBFS version: %#x\n", header.version);
	printf("ROM size: %#x\n", header.romSize);
	printf("Boot block size: %#x\n", header.bootBlockSize);
	printf("CBFS size: %#x\n",
		header.romSize - header.bootBlockSize - header.offset);
	printf("Alignment: %d\n", header.align);
	printf("Offset: %#x\n", header.offset);
	printf("\n");
	return 0;
}
