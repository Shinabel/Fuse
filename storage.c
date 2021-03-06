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
    st->st_nlink = in->refs;
	st->st_size = in->size;
	st->st_uid = getuid();
    st->st_atime = in->atime;
    st->st_mtime = in->mtime;
    st->st_ctime = in->ctime;
	st->st_ino = n;
	return 0;
}

int storage_read(const char* path, char* buf, size_t size, off_t offset) {
    int n = tree_lookup(path);
    inode* in = get_inode(n);

    int index = 0;
    int oindex = offset;
    int leftover = size;

    while (leftover > 0) {
        int pn = inode_get_pnum(in, oindex / 4096);
        char* data = (char*)pages_get_page(pn);

        int off_amount = oindex % 4096;

        data += off_amount;
        int amount = min(4096 - off_amount, leftover);
        strncpy(buf + index, data, amount);
        index += amount;
        oindex += amount;
        leftover -= amount;
    }
    
    // update the access time when read
    time_t now = time(0);
    in->atime = now;

    return size;
}

int storage_write(const char* path, const char* buf, size_t size, off_t offset){
    int n = tree_lookup(path);
    inode* in = get_inode(n);
    
    int grow = size + offset;
    if (grow > in->size) {
        grow_inode(in, grow);
    }

    int index = 0;
    int oindex = offset;
    int leftover = size;
    fflush(stdout);
    while (leftover > 0) {
        int pn = inode_get_pnum(in, oindex / 4096);
        char* data = (char*) pages_get_page(pn);

        int off_amount = oindex % 4096;

        data += off_amount;
        int amount = min(4096 - off_amount, leftover);

        strncpy(data, buf + index, amount);
        index += amount;
        oindex += amount;
        leftover -= amount;
    }

    // update the time when written
    time_t now = time(0);
    in->atime = now;
    in->mtime = now;

    return size;
}

int storage_link(const char *from, const char *to){

	int inum = tree_lookup(from);

    char* name = (char*)malloc(strlen(to));
    char* parent = (char*)malloc(strlen(to));
	char* temp = name;

    strcpy(name, to);
    strcpy(parent, to);
    name = strrchr(name, '/'); // get the last file name after '/'
    name++;

    // minus one for '/'
    // remove that name to get parent directory
    int loc = strlen(from) - strlen(name);
    parent[loc] = 0;

    int p_in = tree_lookup(parent);

	if (inum < 0 || p_in < 0){
        printf("LINK CAUSED A PROBLEM\n");
        int n = inum < 0 ? inum : p_in;
		return n;
	}
    // from node
	inode* in = get_inode(inum);
    in->refs++;
    // parent to node
    inode* pnode = get_inode(p_in);

    directory_put(pnode, name, inum);
    free(temp);
    free(parent);
    return 0;
}


int
storage_unlink(const char* path){
	char* name = (char*)malloc(strlen(path));
    char* parent = (char*)malloc(strlen(path));
	char* temp = name;

    strcpy(name, path);
    strcpy(parent, path);
    name = strrchr(name, '/'); // get the last file name after '/'
    name++;

    // minus one for '/'
    // remove that name to get parent directory
    int loc = strlen(path) - strlen(name);
    parent[loc] = 0;

    int p_in = tree_lookup(parent);
	int inum = tree_lookup(path);

	if (inum < 0 || p_in < 0){
        printf("UNLINK CAUSED A PROBLEM\n");
        int n = inum < 0 ? inum : p_in;
		return n;
	}

    int rv = 0;
	inode* in = get_inode(inum);
    inode* pnode = get_inode(p_in);

    if (in->refs > 1) {
        in->refs--;
    } else {
        // TODO: inode_free()
        free_page(in->ptrs[0]);
        void* bm = get_inode_bitmap();
        bitmap_put(bm,inum, 0);
    }

    rv = directory_delete(pnode, name);
    free(temp);
    free(parent);
    return rv;
}

int storage_rename(const char* from, const char* to){
    int rv = storage_link(from,to);
    if (rv < 0){
        return rv;
    }
    return storage_unlink(from);
}

int storage_mknod(const char* path, int mode){
    char* name = (char*)malloc(strlen(path));
    char* parent = (char*)malloc(strlen(path));
    char* temp = name;

    strcpy(name, path);
    strcpy(parent, path);
    name = strrchr(name, '/'); // get the last file name after '/'
    name++;

    // minus one for '/'
    // remove that name to get parent directory
    int loc = strlen(path) - strlen(name);
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

    free(temp);
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

int storage_truncate(const char *path, off_t size) {
    int n = tree_lookup(path);
    if (n < 0) {
        printf("TRUNCATION ERROR\n");
        return -ENOENT;
    }
    inode* in = get_inode(n);
    if (in->size > size) {
        shrink_inode(in, size);
    } else {
        grow_inode(in, size);
    }
    return 0;
}
