#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <fuse.h>
#include <time.h>
#include <errno.h>

#include "storage.h"
#include "bitmap.h"
#include "pages.h"
#include "inode.h"
#include "util.h"
#include "directory.h"


void storage_init(const char* path){
    printf("Initialize Storage: %s\n", path);
    pages_init(path);
    directory_init();
}

int
storage_stat(const char* path, struct stat* st){
	int n = tree_lookup(path);
	if (n < 0 ){
		printf("NO MATCHING FROM GIVEN PATH\n");
		return n;
	}
	inode* in = get_inode(n);
	st->st_mode = in->mode;
	st->st_size = in->size;
	st->st_uid = getuid();
    st->st_atime = in->atime;
    st->st_mtime = in->mtime;
    st->st_ctime = in->ctime;
	st->st_ino = n;
	return 0;
}

int storage_read(const char* path, char* buf, size_t size, off_t offset) {
    // TODO: for ch03 with directory
    
    // make sure file size < 4k
    if (offset + size > 4096) return -1;

    int n = tree_lookup(path); // always returns 0 since root for now
    inode* in = get_inode(n);

    // TODO: size > 4k
    // need to account for bigger case
    int pnum = inode_get_pnum(in, 0);
    char* data = (char*)pages_get_page(pnum);

    strncpy(buf, data, size);
    
    // update the access time when read
    time_t now = time(0);
    in->atime = now;

    return size;
}

int storage_write(const char* path, const char* buf, size_t size, off_t offset){
    // TODO: for ch03 with directory
    int n = tree_lookup(path); // always returns 0 since root for now
    inode* in = get_inode(n);
    
    int grow = in->size + size - offset ;
    grow_inode(in, grow);

    // TODO: write file > 4k
    int pnum = inode_get_pnum(in, 0);
    char* data = (char*)pages_get_page(pnum);

    strncpy(data, buf , size);

    // update the time when written
    time_t now = time(0);
    in->atime = now;
    in->ctime = now;
    in->mtime = now;


    return size;
}

int storage_link(const char *from, const char *to){
    // TODO: for ch03 with directory
    int n = tree_lookup(from); // always returns 0 since root for now
    inode* in = get_inode(n);

    char* name = (char*)malloc(strlen(to));
    strcpy(name,to);
    in->refs++;

    directory_put(in, name, n);
    free(name);
    return 0;
}


int
storage_unlink(const char* path){
	char* name = (char*)malloc(strlen(path));
	char* temp_n = name;
	strcpy(name,path);
	name = strrchr(name,'/');
	name += 1;
	int rv = 0;

	int inum = tree_lookup(path);
	if (inum < 0){
		return inum;
	}
	inode* node = get_inode(inum);

	free_page(node->ptrs[0]);
    
	void* bm = get_inode_bitmap();
	bitmap_put(bm,inum, 0);

	rv = directory_delete(get_inode(0),name);
	free(temp_n);
	return rv;
}

int storage_rename(const char* from, const char* to){
    // TODO: for ch03 with directory
    int rv = storage_link(from,to);
    if (rv < 0){
        return rv;
    }
    return storage_unlink(from);
}

int storage_mknod(const char* path, int mode){
    char* name = (char*)malloc(strlen(path));
    char* parent = (char*)malloc(strlen(path));

    strcpy(name, path);
    name = strrchr(name, '/'); // get the last file name after '/'
    
    // minus one for '/'
    // remove that name to get parent directory
    int loc = strlen(path) - strlen(name) - 1;
    parent[loc] = 0;
	
    int pn = tree_lookup(parent);
    inode* p_in = get_inode(pn);

    int inum = alloc_inode();
    if (inum == -1) {
        printf("ERROR: NO free inode!\n");
        return -1;
    }

    inode* in = get_inode(inum);
    in->mode = mode;
    in->size = 0;
    in->refs = 1;
    in->ptrs[0] = alloc_page();
    // update the time when written
    time_t now = time(0);
    in->atime = now;
    in->ctime = now;
    in->mtime = now;


    int rv = directory_put(p_in, name, inum);


    free(name);
    free(parent);
    return rv;
}

int storage_set_time(const char* path, const struct timespec ts[2]){
    int n = tree_lookup(path);
    inode* in = get_inode(n);
    in->atime = ts[0].tv_sec;
    in->mtime = ts[1].tv_sec;
    return 0;
}

int storage_chmod(const char* path, mode_t mode) {
    int n = tree_lookup(path);
    if (n < 0) {
        return -ENOENT;
    }
    inode* in = get_inode(n);
    in->mode = mode;
    return 0;
}