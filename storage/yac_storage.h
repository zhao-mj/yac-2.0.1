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
   | Author: Xinchen Hui <laruence@php.net>                               |
   +----------------------------------------------------------------------+
   */

/* $Id$ */

#ifndef YAC_STORAGE_H
#define YAC_STORAGE_H

#define YAC_STORAGE_MAX_ENTRY_LEN  	(1 << 20)
#define YAC_STORAGE_MAX_KEY_LEN		(48)
#define YAC_STORAGE_FACTOR 			(1.25)
#define YAC_KEY_KLEN_MASK			(255)
#define YAC_KEY_VLEN_BITS			(8)
#define YAC_KEY_KLEN(k)				((k).len & YAC_KEY_KLEN_MASK)
#define YAC_KEY_VLEN(k)				((k).len >> YAC_KEY_VLEN_BITS)
#define YAC_KEY_SET_LEN(k, kl, vl)	((k).len = (vl << YAC_KEY_VLEN_BITS) | (kl & YAC_KEY_KLEN_MASK))
#define YAC_FULL_CRC_THRESHOLD      256

#define USER_ALLOC					emalloc
#define USER_FREE					efree

typedef struct { 
	unsigned long atime; //数据最近更新时间
	unsigned int len; //key和value的长度
	char data[1]; //数据内容
} yac_kv_val;

typedef struct {
	unsigned long h; //hash指
	unsigned long crc; //数据crc32校验值
	unsigned int ttl; //key过期时间
	unsigned int len; //key+value的长度
	unsigned int flag; //数据类型及压缩标识
	unsigned int size; //value的空间大小
	yac_kv_val *val; //value的地址
	unsigned char key[YAC_STORAGE_MAX_KEY_LEN]; //key
} yac_kv_key;

typedef struct _yac_item_list {
	unsigned int index;
	unsigned long h;
	unsigned long crc;
	unsigned int ttl;
	unsigned int k_len;
	unsigned int v_len;
	unsigned int flag;
	unsigned int size;
	unsigned char key[YAC_STORAGE_MAX_KEY_LEN];
	struct _yac_item_list *next;
} yac_item_list;

typedef struct {
	volatile unsigned int pos; 
	unsigned int size;
	void *p;
} yac_shared_segment;

typedef struct {
	unsigned long k_msize;
	unsigned long v_msize;
	unsigned int segments_num;
	unsigned int segment_size;
	unsigned int slots_num;
	unsigned int slots_size;
	unsigned int miss;
	unsigned int fails;
	unsigned int kicks;
	unsigned int recycles;
	unsigned long hits;
} yac_storage_info;

typedef struct {
	yac_kv_key  *slots;
	unsigned int slots_mask; //slots_num-1
	unsigned int slots_num; //slots的数量
	unsigned int slots_size; //slots的大小
	unsigned int miss; //缓存失败次数
	unsigned int fails; //分配内存等操作失败次数
	unsigned int kicks; //踢出缓存单元
	unsigned int recycles;
	unsigned long hits; //缓存命中次数
	yac_shared_segment **segments;
	unsigned int segments_num;//segments个数
	unsigned int segments_num_mask; //segments_num_mask = segments_num-1
	yac_shared_segment first_seg;
} yac_storage_globals;

extern yac_storage_globals *yac_storage;

#define YAC_SG(element) (yac_storage->element)

int yac_storage_startup(unsigned long first_size, unsigned long size, char **err);
void yac_storage_shutdown(void);
int yac_storage_find(char *key, unsigned int len, char **data, unsigned int *size, unsigned int *flag, int *cas, unsigned long tv);
int yac_storage_update(char *key, unsigned int len, char *data, unsigned int size, unsigned int falg, int ttl, int add, unsigned long tv);
void yac_storage_delete(char *key, unsigned int len, int ttl, unsigned long tv);
void yac_storage_flush(void);
const char * yac_storage_shared_memory_name(void);
yac_storage_info * yac_storage_get_info(void);
void yac_storage_free_info(yac_storage_info *info);
yac_item_list * yac_storage_dump(unsigned int limit);
void yac_storage_free_list(yac_item_list *list);
#define yac_storage_exists(ht, key, len)  yac_storage_find(ht, key, len, NULL)

#endif	/* YAC_STORAGE_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
