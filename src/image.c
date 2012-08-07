
#include "image.h"

#define IMAGE_CHECK(image) do { if ( ! image || image->fd < 0 ) return; } while (0)
#define IMAGE_STORE_CUR(image) do { if ( shared_fd ) image->cur = lseek(image->fd, 0, SEEK_CUR) - image->offset; } while (0)
#define IMAGE_RESTORE_CUR(image) do { if ( shared_fd ) lseek(image->fd, image->offset + image->cur, SEEK_SET); } while (0)

static void image_append(struct image * image, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout) {

	IMAGE_CHECK(image);

	image->hash = image_hash_from_data(image);
	image->device = device_from_string(device);

	if ( type )
		image->type = image_type_from_string(type);
	else
		image->type = image_type_from_data(image);

	if ( hwrevs )
		image->hwrevs = strdup(hwrevs);

	if ( version )
		image->version = strdup(version);

	if ( layout )
		image->layout = strdup(layout);

}

static struct image * image_alloc(void) {

	struct image * image = calloc(1, sizeof(struct image));
	if ( ! image ) {
		perror("Cannot allocate memory");
		return NULL;
	}
	return image;

}

struct image * image_alloc_from_file(const char * file, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout) {

	struct image * image = image_alloc();
	if ( ! image )
		return NULL;

	image->shared_fd = 0;
	image->fd = open(file, O_RDONLY);
	if ( image->fd < 0 ) {
		perror("Cannot open file");
		free(image);
		return NULL;
	}

	image->size = lseek(image->fd, 0, SEEK_END);
	image->offset = 0;
	image->cur = 0;

	image_append(image, type, device, hwrevs, version, layout);
	return image;

}

struct image * image_alloc_from_fiasco(struct fiasco * fiasco, size_t size, size_t offset, uint16_t hash, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout) {

	struct image * image = image_alloc();
	if ( ! image )
		return NULL;

	image->shared_fd = 1;
	image->fd = fiasco->fd;
	image->size = size;
	image->offset = offset;
	image->cur = 0;

	image_append(image, type, device, hwrevs, version, layout);

	if ( image->hash != hash ) {
		fprintf(stderr, "Image hash mishmash");
		image_free(image);
		return NULL;
	}

	return image;

}

void image_free(struct image * image) {

	IMAGE_CHECK(image);

	if ( ! image->shared_fd ) {
		close(image->fd);
		image->fd = -1;
	}

	free(image->hwrevs);
	free(image->version);
	free(image->layout);

	free(image);

}

void image_seek(struct image * image, whence) {

	IMAGE_CHECK(image);
	lseek(image->fd, image->offset + whence, SEEK_SET);
	IMAGE_STORE_CUR(image)

}

size_t image_read(struct image * image, void * buf, size_t count) {

	IMAGE_CHECK(image);
	IMAGE_RESTORE_CUR(image);

	if ( image->cur + count > image->size ) {
		if ( image->size < image->cur )
			return 0;
		count = image->size - image->cur;
	}

	count = read(image->fd, buf, count);

	IMAGE_STORE_CUR(image);
	return count;

}

/*size_t image_write(struct image * image, void * buf, size_t count) {

	IMAGE_CHECK(image);
	IMAGE_RESTORE_CUR(image);
	count = write(image->fd, buf, count);
	IMAGE_STORE_CUR(image);
	return count;

}*/


void image_list_add(struct image_list ** list, struct image * image) {

	IMAGE_CHECK(image);
	struct image_list * first = calloc(1, sizeof(struct image_list));
	if ( ! next )
		return;

	first->image = image;
	first->next = *list;

	if ( *list ) {
		first->prev = *list->prev;
		if ( *list->prev )
			*list->prev->next = first;
		*list->prev = first;
	} else {
		*list = first;
	}

}

void image_list_del(struct image_list * list) {

	if ( ! list )
		return;

	if ( list->prev )
		list->prev->next = list->next;

	if ( list->next )
		list->next->prev = list->prev;

	image_free(list->image);
	free(list);

}


uint16_t image_hash_from_data(struct image * image) {

	/* TODO */

}

static const char * image_types[] = {
	[IMAGE_XLOADER] = "xloader",
	[IMAGE_2ND] = "2nd",
	[IMAGE_SECONDARY] = "secondary",
	[IMAGE_KERNEL] = "kernel",
	[IMAGE_INITFS] = "initfs",
	[IMAGE_ROOTFS] = "rootfs",
	[IMAGE_OMAP_NAND] = "omap-nand",
	[IMAGE_MMC] = "mmc",
	[IMAGE_CMT_2ND] = "cmt-2nd",
	[IMAGE_CMT_ALGO] = "cmt-algo",
	[IMAGE_CMT_MCUSW] = "cmt-mcusw",
};

enum image_type image_type_from_data(struct image * image) {

	IMAGE_CHECK(image);
	unsigned char buf[512];

	image_seek(image, 0);
	image_read(image, buf, sizeof(buf));

	// 2nd      : +0x34 = 2NDAPE
	// secondary: +0x04 = NOLOScnd
	// x-loader : +0x14 = X-LOADER
	// xloader8 : +0x0c = NOLOXldr
	// kernel   : +0x00 = 0000 a0e1 0000 a0e1
	// initfs   : <2M...be sure with 3M 0x300000

	if ( memcmp(buf+0x34, "2NDAPE", 6) == 0 )
		return IMAGE_2ND;
	else if ( memcmp(buf+0x04, "NOLOScnd", 8) == 0 )
		return IMAGE_SECONDARY;
	else if ( memcmp(buf+0x14, "X-LOADER", 8) == 0 )
		return IMAGE_XLOADER;
	else if ( memcmp(buf+0x0c, "NOLOXldr", 8) == 0 )
		return IMAGE_XLOADER;
	else if ( memcmp(buf+4, "NOLOXldr", 8) == 0 )
		// TODO: this is xloader800, not valid on 770?
		return IMAGE_2ND;
	else if ( memcmp(buf, "\x00\x00\xa0\xe1\x00\x00\xa0\xe1", 8) == 0 )
		return IMAGE_KERNEL;
	else if ( memcmp(buf, "\x21\x01\x01", 3) == 0 )
		return IMAGE_KERNEL;
	else if ( memcmp(b+0x00, "\x85\x19", 2) == 0 ) {
		// JFFS2 MAGIC
		if ( image->size < 0x300000 )
			return IMAGE_INITFS;
		else
			return IMAGE_ROOTFS;
	}

	/* TODO: Add support for UBIFS rootfs and other types */

	return IMAGE_UNKNOWN;

}

enum image_type image_type_from_string(const char * type) {

	size_t i;

	if ( ! type )
		return IMAGE_UNKNOWN;

	for ( i = 0; i < sizeof(image_types)/sizeof(image_types[0]); ++i )
		if ( image_types[i] && strcmp(image_types[i], type) == 0 )
			return i;

	return IMAGE_UNKNOWN;

}

const char * image_type_to_string(enum image_type type) {

	if ( type > sizeof(image_types) )
		return NULL;

	return image_types[type];

}
