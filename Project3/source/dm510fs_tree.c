#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

int dm510fs_getattr( const char *, struct stat * );
int dm510fs_readdir( const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info * );
int dm510fs_mkdir(const char *, mode_t);
int dm510fs_mknod(const char *, mode_t, dev_t);
int dm510fs_open( const char *, struct fuse_file_info * );
int dm510fs_read( const char *, char *, size_t, off_t, struct fuse_file_info * );
int dm510fs_write(const char *path, const char *buf,
    size_t size, off_t offset, struct fuse_file_info *fi);
int dm510fs_release(const char *path, struct fuse_file_info *fi);
int dm510fs_unlink(const char *path);
int dm510fs_rmdir(const char *path);
int dm510fs_truncate(const char *path, off_t size);
int dm510fs_utime(const char *path, struct utimbuf buf);
void* dm510fs_init();
void dm510fs_destroy(void *private_data);

static dm_inode *new_inode(const char *name, bool isDir);
static struct dm510_inode *find_inode(const char *path);
static char *get_name(const char *path);
static char *get_parent(const char *path);

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
	.mknod = dm510fs_mknod,
	.mkdir      = dm510fs_mkdir,
	.unlink = dm510fs_unlink,
	.rmdir = dm510fs_rmdir,
	.truncate = dm510fs_truncate,
	.open	= dm510fs_open,
	.read	= dm510fs_read,
	.release = dm510fs_release,
	.write = dm510fs_write,
	.rename = NULL,
	.utime = dm510fs_utime,
	.init = dm510fs_init,
	.destroy = dm510fs_destroy
};

typedef struct dm510_inode {
    int used;                // 0 if inode not in use
    int isDir;               // 0 if file, 1 if directory
    char name[MAX_NAME_LEN]; // File or folder name
    
    struct dm510_inode *parent; // Path inode before this one.
    struct dm510_inode *first_child;
    struct dm510_inode *sibling;

    size_t size;             // amount of bytes in file, 0 if folder.

    char data[MAX_FILE_SIZE]; // Data for file
    // maybe more?
} dm_inode;

static dm_inode *root = NULL;

/*
 * Return file attributes.
 * The "stat" structure is described in detail in the stat(2) manual page.
 * For the given pathname, this should fill in the elements of the "stat" structure.
 * If a field is meaningless or semi-meaningless (e.g., st_ino) then it should be set to 0 or given a "reasonable" value.
 * This call is pretty much required for a usable filesystem.
*/
int dm510fs_getattr( const char *path, struct stat *stbuf ) {
	//printf("getattr: (path=%s)\n", path);

	memset(stbuf, 0, sizeof(struct stat));

    struct dm510_inode *node = NULL;

    node = find_inode(path);

    if (!node) return -ENOENT;
    

    if (node->isDir){
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = node->size;
    }
    //printf("succes\n");
	return 0;
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
	//printf("readdir: (path=%s)\n", path);

    struct dm510_inode *node = NULL;

    node = find_inode(path);

    if (!node) return -ENOENT;
    if (!node->isDir) return -ENOTDIR;

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

    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs_inodes[i].used) continue;
        if (strcmp(fs_inodes[i].path, path)==0) {

            if (fs_inodes[i].isDir)
                return -EISDIR;

            fi->fh = i;
            return 0;                /* success */
        }
    }
    return -ENOENT;
}

/*
 * Read size bytes from the given file into the buffer buf, beginning offset bytes into the file. See read(2) for full details.
 * Returns the number of bytes transferred, or 0 if offset was at or beyond the end of the file. Required for any sensible filesystem.
*/
int dm510fs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {
    printf("read: (path=%s)\n", path);

    struct dm510_inode *node = &fs_inodes[fi->fh];

    if ((size_t)offset >= node->size)
        return 0;

    size_t bytes = node->size - offset;
    if (bytes > size) bytes = size;

    memcpy(buf, node->data, node->size);
	
	return (int)bytes;
}

int dm510fs_write(const char *path, const char *buf,
    size_t size, off_t offset, struct fuse_file_info *fi){
        printf("write: (path=%s)\n", path);
        struct dm510_inode *node = &fs_inodes[fi->fh];

        if (offset + size > MAX_FILE_SIZE)
        size = MAX_FILE_SIZE - offset;

        memcpy(node->data + offset, buf, size);

        if (offset + size > node->size)
            node->size = offset + size;

        return (int)size;
    }

/*
 * This is the only FUSE function that doesn't have a directly corresponding system call, although close(2) is related.
 * Release is called when FUSE is completely done with a file; at that point, you can free up any temporarily allocated data structures.
 */
int dm510fs_release(const char *path, struct fuse_file_info *fi) {
	printf("release: (path=%s)\n", path);
	return 0;
}

//Tjek mappe navnet for sikkerhed
//tjek om den findes i forvejen.
int dm510fs_mkdir(const char *path, mode_t mode){
    //check if path exists
    char *name = get_name(path);
    char *parent = get_parent(path);
    //printf("%s\n", path);
    //printf("%s\n", name);
    //printf("%s\n", parent);

    mode = 755;

    //Make directory
    for (int i = 0; i < MAX_FILES; i++){
        if (!fs_inodes[i].used){
            memset(&fs_inodes[i], 0, sizeof(struct dm510_inode));
            fs_inodes[i].used = 1;
            fs_inodes[i].isDir = 1;
            //printf("test1 %s\n",name);
            strcpy(fs_inodes[i].name, name);
            //printf("test2 %s\n",path);
            strcpy(fs_inodes[i].path, path);

            //printf("test3 %s\n",parent);
            strcpy(fs_inodes[i].parent, parent);
            break;
        }
    }
    return 0;
}

//Tjek længden på fil navn for security.
int dm510fs_mknod(const char *path, mode_t mode, dev_t dev){
    char *name = get_name(path);
    char *parent = get_parent(path);

    if (!S_ISREG(mode) && !S_ISFIFO(mode))
        return -EINVAL;

    mode = 755;

    for (int i = 0; i < MAX_FILES; i++){
        if (!fs_inodes[i].used){
            memset(&fs_inodes[i], 0, sizeof(struct dm510_inode));
            fs_inodes[i].used = 1;
            fs_inodes[i].isDir = 0;
            //printf("test1 %s\n",name);
            strcpy(fs_inodes[i].name, name);
            //printf("test2 %s\n",path);
            strcpy(fs_inodes[i].path, path);

            //printf("test3 %s\n",parent);
            strcpy(fs_inodes[i].parent, parent);

            fs_inodes[i].size = 0;

            memset(fs_inodes[i].data, 0, MAX_FILE_SIZE);

            break;
        }
    }

    return 0;
}

int dm510fs_unlink(const char *path){
    struct dm510_inode *node = NULL;

    node = find_inode(path);

    if (!node) return -ENOENT;

    memset(node, 0, sizeof(struct dm510_inode));
    
    return 0;
}

int dm510fs_rmdir(const char *path){
    struct dm510_inode *node = NULL;

    node = find_inode(path);

    if (!node) return -ENOENT;
    if (!node->isDir) return -ENOTDIR;
    if (strcmp("/", path) == 0) return -EBUSY;

    for (int i = 0; i < MAX_FILES; i++){
        if (!fs_inodes[i].used) continue;
        if (strcmp(fs_inodes[i].parent, node->path)) return -ENOTEMPTY;
    }

    memset(node, 0, sizeof(struct dm510_inode));
    
    return 0;
}

int dm510fs_truncate(const char *path, off_t size){
    struct dm510_inode *node = NULL;

    node = find_inode(path);

    if(!node) return -ENOENT;
    if(node->isDir) return -EISDIR;
    if(size > MAX_FILE_SIZE) return -EFBIG;

    if ((size_t)size > node->size)
        memset(node->data + node->size, 0, size - node->size);

    node->size = size;

    return 0;
}

int dm510fs_utime(const char *path, struct utimbuf buf){

    return 0;
}

static dm_inode *new_inode(const char *name, bool isDir){
    dm_inode *node = calloc(1, sizeof(node));
    if (!node) = return NULL;

    node->isDir = isDir;
    strncpy(node->name, name, MAX_NAME_LEN-1);
    return node;
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
        //memset(fs_inodes, 0, sizeof(fs_inodes));
        //fread(fs_inodes, sizeof(fs_inodes), 1, f); //Maybe needs to be fixed
        //corruption?? 
    } else {
        dm_inode root = new_inode("root", true);
    }
    fclose(f);
    return 0;
}

static struct dm510_inode *find_inode(const char *path){
    struct dm_inode *node = NULL;

    for (int i = 0; i < MAX_FILES; i++){
        if (!fs_inodes[i].used) continue;

        if (strcmp(fs_inodes[i].path, path) == 0){
            node = &fs_inodes[i];
            return node;
        }
    }

    return node;
}

static char *get_name(const char *path)
{
    const char *slash = strrchr(path, '/');

    const char *name = (slash ? slash + 1 : path);

    if (*name == '\0') 
        return strdup("");  

    return strdup(name);
}

static char *get_parent(const char *path){
    const char *slash = strrchr(path, '/');
    if (slash == path)
        return strdup("/");

    size_t len = slash - path;
    char *parent = malloc(len + 1);
    strncpy(parent, path, len);
    parent[len] = '\0';
    return parent;
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



int main( int argc, char *argv[] ) {
	fuse_main( argc, argv, &dm510fs_oper );

	return 0;
}