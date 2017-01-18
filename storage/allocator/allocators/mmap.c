/*
   +----------------------------------------------------------------------+
   | Yet Another Cache                                                    |
   +----------------------------------------------------------------------+
   | Copyright (c) 2013-2013 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Xinchen Hui <laruence@php.net>                              |
   +----------------------------------------------------------------------+
   */

#include "storage/yac_storage.h"
#include "storage/allocator/yac_allocator.h"

#ifdef USE_MMAP

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#if defined(MAP_ANON) && !defined(MAP_ANONYMOUS)
# define MAP_ANONYMOUS MAP_ANON
#endif

#ifndef MAP_FAILED
#define MAP_FAILED (void *)-1
#endif

typedef struct  {
	yac_shared_segment common;
	unsigned long size;
} yac_shared_segment_mmap;

static int create_segments(unsigned long k_size, unsigned long v_size, yac_shared_segment_mmap **shared_segments_p, int *shared_segments_count, char **error_in) /* {{{ */ {
	unsigned long allocate_size, occupied_size =  0;
	unsigned int i, segment_size, segments_num = 1024;
	yac_shared_segment_mmap first_segment;

	k_size = YAC_SMM_ALIGNED_SIZE(k_size);
	v_size = YAC_SMM_ALIGNED_SIZE(v_size);
	//#define YAC_SMM_SEGMENT_MIN_SIZE    (4*1024*1024)
	//v_size默认 64M
	while ((v_size / segments_num) < YAC_SMM_SEGMENT_MIN_SIZE) {
		segments_num >>= 1;
	}

	segment_size = v_size / segments_num;
	++segments_num;

	allocate_size = k_size + v_size;
	/**
		mmap()
		函数功能：mmap将一个文件或者其它对象映射进内存。文件被映射到多个页上，如果文件的大小不是所有页的大小之和，最后一个页不被使用的空间将会清零。mmap在用户空间映射调用系统中作用很大。
		函数条件：必须以PAGE_SIZE为单位进行映射，而内存也只能以页为单位进行映射，若要映射非PAGE_SIZE整数倍的地址范围，要先进行内存对齐，强行以PAGE_SIZE的倍数大小进行映射。
		函数原型：void* mmap(void* start,size_t length,int prot,int flags,int fd,off_t offset);

			prot：期望的内存保护标志，不能与文件的打开模式冲突。是以下的某个值，可以通过or运算合理地组合在一起
				PROT_EXEC //页内容可以被执行
				PROT_READ //页内容可以被读取
				PROT_WRITE //页可以被写入
				PROT_NONE //页不可访问

			flags：指定映射对象的类型，映射选项和映射页是否可以共享。它的值可以是一个或者多个以下位的组合体
				MAP_FIXED //使用指定的映射起始地址，如果由start和len参数指定的内存区重叠于现存的映射空间，重叠部分将会被丢弃。如果指定的起始地址不可用，操作将会失败。并且起始地址必须落在页的边界上。
				MAP_SHARED //与其它所有映射这个对象的进程共享映射空间。对共享区的写入，相当于输出到文件。直到msync()或者munmap()被调用，文件实际上不会被更新。
				MAP_PRIVATE //建立一个写入时拷贝的私有映射。内存区域的写入不会影响到原文件。这个标志和以上标志是互斥的，只能使用其中一个。
				MAP_DENYWRITE //这个标志被忽略。
				MAP_EXECUTABLE //同上
				MAP_NORESERVE //不要为这个映射保留交换空间。当交换空间被保留，对映射区修改的可能会得到保证。当交换空间不被保留，同时内存不足，对映射区的修改会引起段违例信号。
				MAP_LOCKED //锁定映射区的页面，从而防止页面被交换出内存。
				MAP_GROWSDOWN //用于堆栈，告诉内核VM系统，映射区可以向下扩展。
				MAP_ANONYMOUS //匿名映射，映射区不与任何文件关联。
				MAP_ANON //MAP_ANONYMOUS的别称，不再被使用。
				MAP_FILE //兼容标志，被忽略。
				MAP_32BIT //将映射区放在进程地址空间的低2GB，MAP_FIXED指定时会被忽略。当前这个标志只在x86-64平台上得到支持。
				MAP_POPULATE //为文件映射通过预读的方式准备好页表。随后对映射区的访问不会被页违例阻塞。
				MAP_NONBLOCK //仅和MAP_POPULATE一起使用时才有意义。不执行预读，只为已存在于内存中的页面建立页表入口。

			fd：有效的文件描述词。一般是由open()函数返回，其值也可以设置为-1，此时需要指定flags参数中的MAP_ANON,表明进行的是匿名映射。
			off_toffset：被映射对象内容的起点。
	*/
	first_segment.common.p = mmap(0, allocate_size, PROT_READ | PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	if (first_segment.common.p == MAP_FAILED) {
		*error_in = "mmap";
		return 0;
	}
	first_segment.size = allocate_size;
	first_segment.common.size = k_size;
	first_segment.common.pos = 0;
	/**
	函数名称：calloc
	函数原型：void *calloc(size_t n, size_t size)；
	函数功能：在内存的动态存储区中分配n个长度为size的连续空间，函数返回一个指向分配起始地址的指针；如果分配不成功，返回NULL。
	*/
	*shared_segments_p = (yac_shared_segment_mmap *)calloc(1, segments_num * sizeof(yac_shared_segment_mmap));
	if (!*shared_segments_p) {
		munmap(first_segment.common.p, first_segment.size);
		*error_in = "calloc";
		return 0;
	} else {
		*shared_segments_p[0] = first_segment;
	}
	*shared_segments_count = segments_num;

	occupied_size = k_size;
	for (i = 1; i < segments_num; i++) {
		(*shared_segments_p)[i].size = 0;
		(*shared_segments_p)[i].common.pos = 0;
		(*shared_segments_p)[i].common.p = first_segment.common.p + occupied_size;
		//设置各个shared_segments_p.common.p的指向
		if ((allocate_size - occupied_size) >= YAC_SMM_ALIGNED_SIZE(segment_size)) {
			(*shared_segments_p)[i].common.size = YAC_SMM_ALIGNED_SIZE(segment_size);
			occupied_size += YAC_SMM_ALIGNED_SIZE(segment_size);
		} else {
			(*shared_segments_p)[i].common.size = (allocate_size - occupied_size);
			break;
		}
	}

	return 1;
}
/* }}} */

static int detach_segment(yac_shared_segment *shared_segment) /* {{{ */ {
	if (shared_segment->size) {
		//定义函数 int munmap(void *start,size_t length);
		//函数说明 munmap()用来取消参数start所指的映射内存起始地址，参数length则是欲取消的内存大小;
		munmap(shared_segment->p, shared_segment->size);
	}
	return 0;
}
/* }}} */

static unsigned long segment_type_size(void) /* {{{ */ {
	return sizeof(yac_shared_segment_mmap);
}
/* }}} */

yac_shared_memory_handlers yac_alloc_mmap_handlers = /* {{{ */ {
	(create_segments_t)create_segments,
	detach_segment,
	segment_type_size
};
/* }}} */

#endif /* USE_MMAP */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
