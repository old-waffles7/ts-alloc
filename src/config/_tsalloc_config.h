
#pragma	once
#ifndef	_TSALLOC_CONFIG_H
#define	_TSALLOC_CONFIG_H



#include	"../internal/common.h"


struct tsalloc_slab_info
{
	uint32_t	block_size;
	uint32_t	slab_size;
	uint32_t	nblocks;
};
typedef	struct tsalloc_slab_info	tsalloc_slab_info_t;

struct tsalloc_config
{
	uint64_t	page_size;
	uint64_t	min_align;
	uint32_t	epoch;
	uint64_t	slab_alloc_max;
	uint64_t	alloc_max;
	uint64_t	new_span_size;
	uint32_t	min_align_shift;
	uint32_t	epoch_shift;
	uint32_t	nbytes_bitmap;
	uint32_t	nszclasses;
	uint32_t	nszclasses_slab;

	const uint64_t*				sz_class_max_nbytes;
	const uint16_t*				sz_class_of_nbytes;
	const tsalloc_slab_info_t*	slab_infos;
	const uint32_t*				tcache_info;
};
typedef	struct tsalloc_config	tsalloc_config_t;


#endif	//_TSALLOC_CONFIG_H
