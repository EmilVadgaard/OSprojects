#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int dm510fs_getattr( const char *, struct stat * );
int dm510fs_readdir( const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info * );
int dm510fs_open( const char *, struct fuse_file_info * );
int dm510fs_read( const char *, char *, size_t, off_t, struct fuse_file_info * );
int dm510fs_release(const char *path, struct fuse_file_info *fi);
void* dm510fs_init();
void dm510fs_destroy(void *private_data);

#define MAX_FILES 100
#define MAX_NAME_LEN  64
#define MAX_FILE_SIZE 4096

/*
 * See descriptions in fuse source code usually located in /usr/include/fuse/fuse.h
 * Notice: The version on Github is a newer version than installed at IMADA
 */
static struct fuse_operations dm510fs_oper = {
	.getattr	= dm510fs_getattr,
	.readdir	= dm510fs_readdir,
	.mknod = NULL,
	.mkdir = NULL,
	.unlink = NULL,
	.rmdir = NULL,
	.truncate = NULL,
	.open	= dm510fs_open,
	.read	= dm510fs_read,
	.release = dm510fs_release,
	.write = NULL,
	.rename = NULL,
	.utime = NULL,
	.init = dm510fs_init,
	.destroy = dm510fs_destroy
};

struct dm510_inode {
    int used;                // 0 if inode not in use
    int isDir;               // 0 if file, 1 if directory
    char name[MAX_NAME_LEN]; // File or folder name
    char path[MAX_NAME_LEN]; // path, fx "/dir1/fileA"

    char parent[MAX_NAME_LEN]; // Path inode before this one.

    size_t size;             // amount of bytes in file, 0 if folder.

    char data[MAX_FILE_SIZE]; // Data for file
    // maybe more?
};

static struct dm510_inode fs_inodes[MAX_FILES];

/*
 * Return file attributes.
 * The "stat" structure is described in detail in the stat(2) manual page.
 * For the given pathname, this should fill in the elements of the "stat" structure.
 * If a field is meaningless or semi-meaningless (e.g., st_ino) then it should be set to 0 or given a "reasonable" value.
 * This call is pretty much required for a usable filesystem.
*/
int dm510fs_getattr( const char *path, struct stat *stbuf ) {
	int res = 0;
	printf("getattr: (path=%s)\n", path);

	memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/.") == 0 || strcmp(path, "/..") == 0) {
        path = "/";
    }
    if (strcmp(path, "/.") == 0 || strcmp(path, "/..") == 0) {
        path = "/";
    }

    struct dm510_inode *node = NULL;

    for (int i = 0; i < MAX_FILES; i++){
        if (strcmp(fs_inodes[i].path, path) == 0){
            node = &fs_inodes[i];
            break;
        }
    }
    if (node == NULL){
        return -ENOENT;
    }

    if (node->isDir){
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = node->size;
    }

	return res;
}

/*
 * Return one or more directory entries (struct dirent) to the caller. This is one of the most complex FUSE functions.
 * Required for essentially any filesystem, since it's what makes ls and a whole bunch of other things work.
 * The readdir function is somewhat like read, in that it starts at a given offset and returns results in a caller-supplied buffer.
 * However, the offset not a byte offset, and the results are a series of struct dirents rather than being uninterpreted bytes.
 * To make life easier, FUSE provides a "filler" function that will help you put things into the buffer.
 *
 * The general plan for a complete and correct readdir is:
 *
 * 1. Find the first directory entry following the given offset (see below).
 * 2. Optionally, create a struct stat that describes the file as for getattr (but FUSE only looks at st_ino and the file-type bits of st_mode).
 * 3. Call the filler function with arguments of buf, the null-terminated filename, the address of your struct stat
 *    (or NULL if you have none), and the offset of the next directory entry.
 * 4. If filler returns nonzero, or if there are no more files, return 0.
 * 5. Find the next file in the directory.
 * 6. Go back to step 2.
 * From FUSE's point of view, the offset is an uninterpreted off_t (i.e., an unsigned integer).
 * You provide an offset when you call filler, and it's possible that such an offset might come back to you as an argument later.
 * Typically, it's simply the byte offset (within your directory layout) of the directory entry, but it's really up to you.
 *
 * It's also important to note that readdir can return errors in a number of instances;
 * in particular it can return -EBADF if the file handle is invalid, or -ENOENT if you use the path argument and the path doesn't exist.
*/
int dm510fs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
	(void) offset;
	(void) fi;
	printf("readdir: (path=%s)\n", path);

    if (strcmp(path, "/.") == 0 || strcmp(path, "/..") == 0) {
        path = "/";
    }
    if (strcmp(path, "/.") == 0 || strcmp(path, "/..") == 0) {
        path = "/";
    }

	struct dm510_inode *node = NULL;

    for (int i = 0; i < MAX_FILES; i++){
        if (strcmp(fs_inodes[i].path, path) == 0){
            node = &fs_inodes[i];
            break;
        }
    }
    if (node == NULL){
        return -ENOENT;
    }
    if (node->isDir == 0){
        return -ENOTDIR;
    }

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	
    for (int i = 0; i < MAX_FILES; i++){
        if (!fs_inodes[i].used) continue;
        if (strcmp(fs_inodes[i].parent, path) == 0){
            filler(buf, fs_inodes[i].name, NULL, 0);
        }
    }

	return 0;
}

/*
 * Open a file.
 * If you aren't using file handles, this function should just check for existence and permissions and return either success or an error code.
 * If you use file handles, you should also allocate any necessary structures and set fi->fh.
 * In addition, fi has some other fields that an advanced filesystem might find useful; see the structure definition in fuse_common.h for very brief commentary.
 * Link: https://github.com/libfuse/libfuse/blob/0c12204145d43ad4683136379a130385ef16d166/include/fuse_common.h#L50
*/
int dm510fs_open( const char *path, struct fuse_file_info *fi ) {
    printf("open: (path=%s)\n", path);
	return 0;
}

/*
 * Read size bytes from the given file into the buffer buf, beginning offset bytes into the file. See read(2) for full details.
 * Returns the number of bytes transferred, or 0 if offset was at or beyond the end of the file. Required for any sensible filesystem.
*/
int dm510fs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {
    printf("read: (path=%s)\n", path);
	memcpy( buf, "Hello\n", 6 );
	return 6;
}

/*
 * This is the only FUSE function that doesn't have a directly corresponding system call, although close(2) is related.
 * Release is called when FUSE is completely done with a file; at that point, you can free up any temporarily allocated data structures.
 */
int dm510fs_release(const char *path, struct fuse_file_info *fi) {
	printf("release: (path=%s)\n", path);
	return 0;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the `private_data` field of
 * `struct fuse_context` to all file operations, and as a
 * parameter to the destroy() method. It overrides the initial
 * value provided to fuse_main() / fuse_new().
 */
void* dm510fs_init() {
    printf("init filesystem\n");
    FILE *f = fopen("/root/dm510/Project3/fs_data.dat", "rb");

    if (f) {
        fread(fs_inodes, sizeof(fs_inodes), 1, f);
        fclose(f);
        //corruption??
    } else {
        memset(fs_inodes, 0, sizeof(fs_inodes));

        fs_inodes[0].used = 1;
        fs_inodes[0].isDir = 1;

        strcpy(fs_inodes[0].name, "/");
        strcpy(fs_inodes[0].path, "/");
        strcpy(fs_inodes[0].parent, "");
    }
    return NULL;
}

/**
 * Clean up filesystem
 * Called on filesystem exit.
 */
void dm510fs_destroy(void *private_data) {
    printf("destroy filesystem\n");

    FILE *f = fopen("/root/dm510/Project3/fs_data.dat", "wb");

    if (f){
        fwrite(fs_inodes, sizeof(fs_inodes), 1, f);
        fclose(f);
    }
}

//Helper functions.
char *normilize_path(const char path){

}

int main( int argc, char *argv[] ) {
	fuse_main( argc, argv, &dm510fs_oper );

	return 0;
}