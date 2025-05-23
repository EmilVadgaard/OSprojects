#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>


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
int dm510fs_truncate(const char *path, off_t new_size);
int dm510fs_utime(const char *path, struct utimbuf *buf);
void* dm510fs_init();
void dm510fs_destroy(void *private_data);

static int freeblock();
static struct dm510_inode *find_inode(const char *path);
static char *get_name(const char *path);
static char *get_parent(const char *path);

#define MAX_FILES 1280
#define MAX_NAME_LEN  64
#define AVG_FILE_SIZE 4096
#define BLOCK_SIZE 512
#define MAX_BLOCK 10240 //((MAX_FILES * AVG_FILE_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE)
#define DATA_SECTION (BLOCK_SIZE - 2*sizeof(int))

#define PATH_MAX 1000
static char disk_path[PATH_MAX];


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

struct dm510_inode {
    int used;                       // 0 if inode not in use
    int isDir;                      // 0 if file, 1 if directory
    char name[MAX_NAME_LEN];        // File or folder name
    char path[MAX_NAME_LEN];        // Path, fx "/dir1/fileA"

    char parent[MAX_NAME_LEN];      // Path inode before this one.

    time_t access_time;             // Time of creation
    time_t modification_time;       // Time of recent change

    size_t size;                    // Amount of bytes in file, 0 if folder.

    int first_block; // Data for file
};

struct block{
    int used;                       // 0 if inode not in use
    char data[DATA_SECTION];        // Data section
    int next_block;                 // Index for next block
};  

static struct block blocks[MAX_BLOCK];

static struct dm510_inode fs_inodes[MAX_FILES];

/*
 * Return file attributes.
 * The "stat" structure is described in detail in the stat(2) manual page.
 * For the given pathname, this should fill in the elements of the "stat" structure.
 * If a field is meaningless or semi-meaningless (e.g., st_ino) then it should be set to 0 or given a "reasonable" value.
 * This call is pretty much required for a usable filesystem.
*/
int dm510fs_getattr( const char *path, struct stat *stbuf ) {
    memset(stbuf, 0, sizeof(struct stat));

    struct dm510_inode *node = NULL;

    node = find_inode(path);        // Locate the specific file / dir

    if (!node) return -ENOENT;      // If file / directory does not exist
    

    if (node->isDir){                                   // If directory
        stbuf->st_mode = S_IFDIR | 0755;                //    Assign directory permissions
        stbuf->st_nlink = 2;                            //    1 in + 1 to itself
        stbuf->st_atime = node->access_time;
        stbuf->st_mtime = node->modification_time;
    } else {                                            // If file
        stbuf->st_mode = S_IFREG | 0644;                //    1 in
        stbuf->st_nlink = 1;                            //    Size of file
        stbuf->st_size = node->size;
        stbuf->st_atime = node->access_time;
        stbuf->st_mtime = node->modification_time;
    }

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

    struct dm510_inode *node = NULL;

    node = find_inode(path);

    if (!node) return -ENOENT;                  // If path is false
    if (!node->isDir) return -ENOTDIR;          // If path leads to a file

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	
    for (int i = 0; i < MAX_FILES; i++){                // Look through all files
        if (!fs_inodes[i].used) continue;               //    If inode is not in use, skip inode
        if (strcmp(fs_inodes[i].parent, path) == 0){    //    If parent equals directory
            filler(buf, fs_inodes[i].name, NULL, 0);    //       Insert onto buffer
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

    for (int i = 0; i < MAX_FILES; i++) {               // Look through all files
        if (!fs_inodes[i].used) continue;               //    If inode is not in use, skip
        if (strcmp(fs_inodes[i].path, path)==0) {       //    If file matches given path

            if (fs_inodes[i].isDir)
                return -EISDIR;

            fi->fh = i;
            return 0;                                   // Success
        }
    }
    return -ENOENT;
}

/*
 * Read size bytes from the given file into the buffer buf, beginning offset bytes into the file. See read(2) for full details.
 * Returns the number of bytes transferred, or 0 if offset was at or beyond the end of the file. Required for any sensible filesystem.
*/
int dm510fs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {

    struct dm510_inode *node = &fs_inodes[fi->fh];

    if (offset >= node->size)                           // Return 0, as there is nothing to read
        return 0;

    if (offset + size > node->size)
        size = node->size - offset;

    /* Offset is within file size range */
    size_t done = 0;
    int block_index = node->first_block;

    size_t block_number = offset / DATA_SECTION;
    size_t block_offset = offset % DATA_SECTION;
    for (size_t i = 0; i < block_number; i++){
        block_index = blocks[block_index].next_block;
    }

    while (done < size && block_index != -1){
        size_t space = DATA_SECTION - block_offset;
        size_t data = (size - done < space) ? (size - done) : space;

        memcpy(buf + done, blocks[block_index].data + block_offset, data);

        done += data;
        block_offset = 0;
        block_index = blocks[block_index].next_block;
    }

    node->access_time = time(NULL);

    return (int)done;
}

int dm510fs_write(const char *path, const char *buf,
    size_t size, off_t offset, struct fuse_file_info *fi){

        struct dm510_inode *node = &fs_inodes[fi->fh];

        if (node->isDir)                                // Not a writeable object
            return -EISDIR; 

        if (node->first_block == -1){                   // If no block has been allocated already
            int block_index = freeblock();              //    Create new data block
            if (block_index == -1) return -ENOSPC;      //    Return error if block wasn't made
            node->first_block = block_index;
        } 

        /* Allocate data blocks */
        size_t block_number = offset / DATA_SECTION;    // Calculate amount of blocks needed
        size_t block_offset = offset % DATA_SECTION;

        int block_index = node->first_block;

        for (size_t i = 0; i < block_number; i++){              // Create blocks if needed
            if (blocks[block_index].next_block == -1){          //    If more blocks is needed
                int next_block = freeblock();                   //       Create new blocks
                if (next_block < 0) return -ENOSPC;             //       Return error if block was not made
                blocks[block_index].next_block = next_block;
            }
            block_index = blocks[block_index].next_block;
        }

        int amount_left = size;
        int bytes_written = 0;

        /* Write into data blocks */
        while (amount_left > 0) {
            size_t space = DATA_SECTION - block_offset;
            size_t data = (amount_left < space) ? amount_left : space;

            memcpy(blocks[block_index].data + block_offset, buf + bytes_written, data);

            bytes_written += data;
            amount_left -= data;
            block_offset = 0;

            if (amount_left > 0){
                if (blocks[block_index].next_block == -1){
                    int next_block = freeblock();
                    if (next_block < 0) return -ENOSPC;
                    blocks[block_index].next_block = next_block;
                }
                block_index = blocks[block_index].next_block;
            }
        }

        /* Update inode information */
        if ((size_t)(offset + bytes_written) > node->size){
            node->size = offset + bytes_written;
        }

        time_t current_time = time(NULL);
        node->access_time = current_time;
        node->modification_time = current_time;

        return (int)bytes_written;
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
    char *name = get_name(path);
    char *parent = get_parent(path);

    if (find_inode(path) != NULL) return -EEXIST;       // Check if directory already exists at location

    mode = 755;

    /* Making of directory & Filling out inode */
    for (int i = 0; i < MAX_FILES; i++){
        if (!fs_inodes[i].used){                                    // Find a free inode which can be used
            memset(&fs_inodes[i], 0, sizeof(struct dm510_inode));
            fs_inodes[i].used = 1;
            fs_inodes[i].isDir = 1;
            fs_inodes[i].access_time = time(NULL);
            fs_inodes[i].modification_time = time(NULL);

            strcpy(fs_inodes[i].name, name);

            strcpy(fs_inodes[i].path, path);

            strcpy(fs_inodes[i].parent, parent);
            break;
        }
    }
    free(parent);
    free(name);
    return 0;
}

int dm510fs_mknod(const char *path, mode_t mode, dev_t dev){
    char *name = get_name(path);
    char *parent = get_parent(path);

    if (find_inode(path) != NULL) return -EEXIST;

    if (!S_ISREG(mode) && !S_ISFIFO(mode))              // Return error if file is not regular, FIFO or pipe.
        return -EINVAL;

    mode = 755;

    /* Making of file & Filling out inode */
    for (int i = 0; i < MAX_FILES; i++){
        if (!fs_inodes[i].used){                        // Find the first free inode, which can be used
            memset(&fs_inodes[i], 0, sizeof(struct dm510_inode));
            fs_inodes[i].used = 1;
            fs_inodes[i].isDir = 0;
            fs_inodes[i].access_time = time(NULL);
            fs_inodes[i].modification_time = time(NULL);

            strcpy(fs_inodes[i].name, name);

            strcpy(fs_inodes[i].path, path);

            strcpy(fs_inodes[i].parent, parent);

            fs_inodes[i].size = 0;

            fs_inodes[i].first_block = -1;

            break;
        }
    }

    free(parent);
    free(name);

    return 0;
}

int dm510fs_unlink(const char *path){
    struct dm510_inode *node = NULL;

    node = find_inode(path);

    if (!node) return -ENOENT;                  // Return if path leads to nothing
    if (node->isDir) return -EISDIR;            // Return if not a file

    int block_index = node->first_block;

    /* Free up all blocks used by file */
    while (block_index != -1){
        int next_block = blocks[block_index].next_block;
        blocks[block_index].used = 0;
        blocks[block_index].next_block = -1;
        block_index = next_block;
    }

    memset(node, 0, sizeof(struct dm510_inode));
    
    return 0;
}

int dm510fs_rmdir(const char *path){
    struct dm510_inode *node = NULL;

    node = find_inode(path);

    if (!node) return -ENOENT;                  // Error if path leads to nothing
    if (!node->isDir) return -ENOTDIR;          // Error if not directory
    if (strcmp("/", path) == 0) return -EBUSY;  // Error if dir is root

    for (int i = 0; i < MAX_FILES; i++){                                        // Check all files
        if (!fs_inodes[i].used) continue;                                       //    If file / dir is in use, skip
        if (strcmp(fs_inodes[i].parent, node->path) == 0) return -ENOTEMPTY;    //    If directory is not empty, return error
    }

    memset(node, 0, sizeof(struct dm510_inode));
    
    return 0;
}

int dm510fs_truncate(const char *path, off_t new_size){
    struct dm510_inode *node = NULL;

    node = find_inode(path);

    if(!node) return -ENOENT;                   // Return if path leads to nothing
    if(node->isDir) return -EISDIR;             // Return if not a file

    size_t blocks_needed = (new_size ? (new_size - 1) / DATA_SECTION : 0);

    /* If size increases */
    if ((size_t)new_size > node->size){
        int block_index = node->first_block;
        if (block_index == -1){                         // If current block has no allocated block
            block_index = freeblock();                  //    Create new block
            node->first_block = block_index;
            if (block_index < 0) return -ENOSPC;
        }

        for (size_t i = 0; i < blocks_needed; i++){     // If more blocks are needed, create more until satisfied
            if (blocks[block_index].next_block == -1){
                int next_block = freeblock();
                if (next_block < 0) return -ENOSPC;
                blocks[block_index].next_block = next_block;
            }
            block_index = blocks[block_index].next_block;
        }
    }

    /* If size decreases */
    else {
        int block_index = node->first_block;
        int prev_block = -1;

        for (size_t i = 0; i < blocks_needed; i++){     // Remove blocks until new size has been fulfilled
            if (block_index == -1) break;
            prev_block = block_index;
            block_index = blocks[block_index].next_block;
        }

        if (prev_block != -1){
            blocks[prev_block].next_block = -1;
        } else {
            node->first_block = -1;
        }

        while (block_index != -1) {
            int next_block = blocks[block_index].next_block;
            blocks[block_index].used = 0;
            blocks[block_index].next_block = -1;
            block_index = next_block;
        }
    }

    node->size = new_size;
    return 0;
}

int dm510fs_utime(const char *path, struct utimbuf *buf){

    struct dm510_inode *node = NULL;

    node = find_inode(path);

    if(!node) return -ENOENT;

    node->access_time = buf->actime;
    node->modification_time = buf->modtime;

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
    FILE *f = fopen(disk_path, "rb");               // Retrieve information on file system

    if (f) {                                        // If saved information exists
        memset(fs_inodes, 0, sizeof(fs_inodes));
        fread(fs_inodes, sizeof(fs_inodes), 1, f);
        fread(blocks, sizeof(blocks), 1, f);
        fclose(f);
    } else {                                        // If file system is opened for the first time
        memset(fs_inodes, 0, sizeof(fs_inodes));
        memset(blocks, 0, sizeof(blocks));

        /* Creation of root */
        fs_inodes[0].used = 1;
        fs_inodes[0].isDir = 1;

        strcpy(fs_inodes[0].name, "root");
        strcpy(fs_inodes[0].path, "/");
        strcpy(fs_inodes[0].parent, "");
    }
    
    return 0;
}

/*
 * Finds the first free block available to give
 */
static int freeblock(){
    int res = -ENOSPC;
    for (int i = 0; i < MAX_BLOCK; i++){            // Find a free block to give
        if (!blocks[i].used) {
            res = i;
            blocks[i].used = 1;
            blocks[i].next_block = -1;
            memset(blocks[i].data, 0, DATA_SECTION);
            return res;
        }
    }

    return res;
}

/*
 * Returns inode of a given path
 */
static struct dm510_inode *find_inode(const char *path){
    struct dm510_inode *node = NULL;

    for (int i = 0; i < MAX_FILES; i++){            // Find specific file / dir matching path
        if (!fs_inodes[i].used) continue;

        if (strcmp(fs_inodes[i].path, path) == 0){
            node = &fs_inodes[i];
            return node;
        }
    }

    return node;
}

/*
 * Retrieves name part of a path
 */
static char *get_name(const char *path)
{
    const char *slash = strrchr(path, '/');         // Last part of path

    const char *name = (slash ? slash + 1 : path);

    if (*name == '\0') 
        return strdup("");  

    return strdup(name);
}

/*
 * Finds parent, according to given path
 */
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

    FILE *f = fopen(disk_path, "wb");               // Open file in which file system information is stored

    if (f){                                         // Write over all information to file
        fwrite(fs_inodes, sizeof(fs_inodes), 1, f);
        fwrite(blocks, sizeof(blocks), 1, f);
        fclose(f);
    }
}



int main( int argc, char *argv[] ) {
    char current_directory[PATH_MAX];
    getcwd(current_directory, sizeof(current_directory));
    snprintf(disk_path, sizeof(disk_path), "%s/fs_data.dat", current_directory);

	fuse_main( argc, argv, &dm510fs_oper );

	return 0;
}