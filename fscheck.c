#include <stdio.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/io.h>
#include <sys/mman.h>
#include <assert.h>

#define BSIZE 512
#define NDIRECT 12

#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Device

typedef unsigned int uint;

struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};

#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

#define MAX_INUM 10
struct bitmapNetwork_
{
    ushort count;
    ushort inum[MAX_INUM];
};

struct inodesNetwork_
{
    ushort count;
    ushort parentInum[MAX_INUM];
};

unsigned int toMachineUint(unsigned int *x)
{
	unsigned char *con = (char *)x;
	unsigned int mach;
	mach  = con[3] << 24;
	mach += con[2] << 16;
	mach += con[1] << 8;
	mach += con[0];
	return mach;
}

unsigned short toMachineUshort(unsigned short *x)
{
	unsigned char *con = (char *)x;
	unsigned short mach;
	mach  = con[1] << 8;
	mach += con[0];
	return mach;
}

int main(int argc, char *argv[]) 
{
    int fd;
    fd = open(argv[1], O_RDONLY);
    if(fd == -1)
    {
        printf("ERROR: image not found \n");
        return 1;
    }

    struct stat s;
    int status = fstat (fd, & s);
    int size = s.st_size;

    char *content = (char*) mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);

	// Boot block check
    for(int i=0;i<BSIZE;i++)
        assert(0 == content[i]);

	// Super block check
	struct superblock *sb = (struct superblock *) (content + 1 * BSIZE);
    sb->ninodes    = toMachineUint(&(sb->ninodes));
    sb->inodestart = toMachineUint(&(sb->inodestart));
    sb->bmapstart  = toMachineUint(&(sb->bmapstart));

	int fsize, data_blocks_start, max_block_no; // (in blocks)
	fsize = toMachineUint(&(sb->size));
    max_block_no = fsize/BSIZE - 1;
    data_blocks_start = sb->bmapstart + 3;

    struct bitmapNetwork_ bitmapNet[max_block_no - data_blocks_start + 1];
    struct inodesNetwork_ inodesNetwork[toMachineUint(&(sb->ninodes))];
    
	char *inodesstart, *logstart, *bmapstart, *datastart;
	inodesstart = content + sb->inodestart * BSIZE;
	logstart    = content + sb->logstart   * BSIZE;
 	bmapstart   = content + sb->bmapstart  * BSIZE;

    unsigned int icount = toMachineUint(&(sb->ninodes));
    struct dinode *inode = (struct dinode *)inodesstart;


    if(!isValidRootInode(inodesstart + 1))
    {
        printf("ERROR MESSAGE: root directory does not exist.\n");
    }


	for(int i=0; i < icount; i++, inode++) {

        inode = getInodeNumberContent(content, i);

        if(!isValidInodeType(inode->type)) 
        {
            printf("ERROR: bad inode\n");
            continue;
        }

        int nblocksForFile = inode->size/BSIZE + 1;
        int indirectBlockNumber = inode->addrs[NDIRECT];
        unsigned int *indirectPointers = NULL;
        if(nblocksForFile > NDIRECT)
        {
            indirectPointers = content + indirectBlockNumber * BSIZE;
        }
        for(int j = 0; j < nblocksForFile; j++)
        {
            // Find block number
            int blocknumber;
            if(j < NDIRECT)
            {
                blocknumber = inode->addrs[j];
            }
            else
            {
                blocknumber = toMachineUint(&(indirectPointers + (NDIRECT - j)));
            }

            if(blocknumber >= data_blocks_start && blocknumber < max_block_no)
            {
                printf("ERROR: bad address in inode\n");
            }

            if(!getBitmapValueForBlock(blocknumber))
            {
                printf("ERROR: address used by inode but marked free in bitmap.\n");
            }

            int bitmapIndex = block_number - data_blocks_start;
            bitmapNet[bitmapIndex].inum[bitmapNet[bitmapIndex].count++] = inode->inum;
        }
       

        if(inode->type == T_DIR)
        {
            if(!isValidDirEntry(inode))
            {
                printf("ERROR: directory not properly formatted\n");
            }

            // Get each child's 
            for(int child = 0; i < nblocksForFile; i++)
            {
               int blocknumber = inode->addrs[child];
               // Check this
               if(blocknumber == 0)
                   continue;

               struct dirent *entry = (struct dirent *)(content + blocknumber*BSIZE);
               for(int k=0; k < (BSIZE/sizeof(struct dirent)); k++)
               {
                   // Check this
                   if(entry->inum == 0)
                       continue;

                   ushort index = entry->inum;
                   inodesNetwork[index].parentInum[inodesNetwork[index].count++] = inode->num;
                   printf("%s\n", entry->name);
                   entry++;
               }
            }
	    } 
    }

     

    return 0;
}
