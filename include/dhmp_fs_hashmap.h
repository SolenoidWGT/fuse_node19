#ifndef DHMP_FS_HASHMAP_H
#define DHMP_FS_HASHMAP_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#define DEFAULT_MAP_SIZE 50
typedef struct hashMap HashMap;
typedef int(*HashCode)(HashMap*	hashMap, void* key);
typedef int(*Equal)(void* key1, void* key2);
typedef int(*Exists)(HashMap *hashMap, void*  key);
typedef void(*Put)(HashMap*	hashMap, void*  key, void* value);
typedef void*(*Get)(HashMap*hashMap, void*  key);
typedef void*(*Remove)(HashMap* hashMap, void* key);
typedef void(*Clear)(HashMap* hashMap);

// hash表项
typedef struct entry {
	void * key;				
	void * value;				
	struct entry* next;	
}Entry;

typedef struct hashMap {
	int size;			// 总长度
	int listSize;		// 固定长度
	Entry * hash_list;
	HashCode hashCode;
	Equal equal;
	Exists exists;
	Get get;
	Put put;
	Remove remove;
	Clear clear;
}HashMap;

HashMap* createHashMap(HashCode hashCode, Equal equal);


#endif // !__HASHMAP_H__