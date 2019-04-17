#include "inode.h"
#include "pages.h"
#include "storage.h"
#include "bitmap.h"

#include "util.h"

inode*
get_inode(int inum){
	void* in = (void*)(pages_get_page(0) + 64);
	return in + (inum * sizeof(inode));
}

int
alloc_inode(){
	void* inode_bm = get_inode_bitmap();
	// from 0 to PAGECOUNT
	for(int i = 0; i < 256; ++i){
		if(!bitmap_get(inode_bm, i)){
			bitmap_put(inode_bm, i, 1);
			return i;
		}
	}
	return -1;
}

int grow_inode(inode* node, int size){
	int new_size = bytes_to_pages(size);
	int start = bytes_to_pages(node->size);
	for (int ii = start; ii < new_size; ii++){
		if (ii < 2) 
		{
			node->ptrs[ii] = alloc_page();
		}
		else
		{
			if (node->iptr == 0)
			{
				node->iptr = alloc_page();
			}
			int* iptrs = pages_get_page(node->iptr);
			iptrs[ii - 2] = alloc_page();
		} 
	}
	node->size = size;
	return 0;
}

int shrink_inode(inode* node, int size) {
	int new_size = bytes_to_pages(size);
	int start = bytes_to_pages(node->size);

	for (int ii = start; ii > new_size; ii--){
		if (ii < 2) 
		{
			memset(pages_get_page(node->ptrs[ii]), 0, 4096);
			free_page(node->ptrs[ii]);
		}
		else
		{
			int* iptrs = pages_get_page(node->iptr);
			memset(pages_get_page(iptrs[ii - 2]), 0, 4096);
			free_page(iptrs[ii - 2]);
			if (node->iptr == 2)
			{
				memset(pages_get_page(node->iptr), 0, 4096);
				free_page(node->iptr);
			}
		} 
	}
	node->size = size;
	return 0;
}

// get the parent node number
int inode_get_pnum(inode* node, int fpn) {
    if (fpn < 2) return node->ptrs[fpn];
    int* plist = pages_get_page(node->iptr);
    return plist[fpn - 2];
}

void print_inode(inode* node) {
	printf("LOCATION: %p\n", node);
	printf("Ref Count: %d\n", node->refs);
	printf("MODE: %d\n", node->mode);
	struct tm* tmp;
	char ts[20];
	tmp = localtime(&node->ctime);
	strftime(ts, 20, "%x - %I:%M%p", tmp);
	printf("Creation Time: %s\n", ts);
	tmp = localtime(&node->atime);
	strftime(ts, 20, "%x - %I:%M%p", tmp);
	printf("Acess Time: %s\n", ts);
	tmp = localtime(&node->mtime);
	strftime(ts, 20, "%x - %I:%M%p", tmp);
	printf("Modification Time: %s\n", ts);
}