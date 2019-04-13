#include <assert.h>
#include "directory.h"
#include "pages.h"
#include "slist.h"
#include <errno.h>
#include <string.h>

#include "util.h"

void directory_init(){
	int rootn = alloc_inode();
	assert(rootn > -1);
	inode* root = get_inode(rootn);
	root->refs = 1;
	root->mode = 040755;
	root->size = 0;
	root->ptrs[0] = alloc_page();
	printf("root mode %d \n", root->mode);
}

int directory_lookup(inode* dd, const char* name){
	dirent* dirs = (dirent*)pages_get_page(dd->ptrs[0]);
	for(int ii = 0; ii < 64; ++ii){
		dirent curr = dirs[ii];
		if(strcmp(name, curr.name) == 0){
			return curr.inum;
		}
	}
	return -ENOENT;
}

int tree_lookup(const char* path){
	slist* parts = s_split(path, '/');
	int dn = 0;

	while(parts != NULL){
		inode* dir = get_inode(dn);
		dn = directory_lookup(dir, parts->data);
		parts = parts->next;
	}

	s_free(parts);
	return dn;
}

int directory_put(inode* dd, const char* name, int inum) {
    dirent* dirs = (dirent*)pages_get_page(dd->ptrs[0]);
    for (int i = 0; i < 64; i++) {
        // find the next empty spot and place the inode
        if (streq(dirs[i].name, "")) {
            strcpy(dirs[i].name, name);
            dirs[i].inum = inum;
            return 0;
        }
    }
    return -ENOENT;
}

int directory_delete(inode* dd, const char* name) {
    // TODO: ch03
    dirent* dirs = (dirent*)pages_get_page(dd->ptrs[0]);
    for (int i = 0; i < 64; i++) {
        // find the empty spot and delete
        if (streq(dirs[i].name, name)) {
            dirs[i].name[0] = 0;
            dirs[i].inum = 0;
            return 0;
        }
    }
    return -ENOENT;
}

