#include <stdio.h>
#include <string.h>
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

#define IPB           (BSIZE / sizeof(struct dinode))
#define IBLOCK(i, sb)     ((i) / IPB + sb->inodestart)

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

#define getBlockContent(content, blockno) (content + blockno * BSIZE)

#define MAX_INUM 10
struct bitmapNetwork_
{
    ushort count;
    ushort inum[MAX_INUM];
};

struct inodesNetwork_
{
    int isDir;
    int nlink;
    int inuse;
    int dotdot_inum;
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

int isValidInodeType(struct dinode *inode)
{
    return inode != NULL && inode->type >= 0 && inode->type <= T_DEV;
}

int isValidRootInode(char *content, struct dinode *rootInode)
{
    if(rootInode->type != T_DIR)
        return 0;

    int nblocksForFile = rootInode->size/BSIZE + 1;
    for(int child = 0; child < nblocksForFile; child++)
    {
       int blocknumber = rootInode->addrs[child];

       struct dirent *entry = (struct dirent *)
                                getBlockContent(content, blocknumber);
       for(int k=0; k < (BSIZE/sizeof(struct dirent)); k++)
       {
           if(entry->inum == 0)
               continue;

           ushort index = entry->inum;
           if(strcmp(entry->name, "..") == 0)
           {
               if(index != 1)
                   return 0;
           }

           if(strcmp(entry->name, ".") == 0)
           {
               if(index != 1)
                   return 0;
           }

           entry++;
       }
    }

    return 1;
}

#define getBitmapBlock(content, sb) (content + sb->bmapstart)

int getBitmapValueForBlock(char*               content, 
                           struct superblock*  sb, 
                           int                 blocknumber)
{
    char *bitmapBlock = content + sb->bmapstart;
    int byte_offset   = blocknumber / 8;
    int byte_value    = *(bitmapBlock + byte_offset);
    int bit_shift_len = 7 - (blocknumber % 8);
    return (byte_value >> bit_shift_len) & 1;
}

struct dinode *getInodeContent(char*              content,
                               struct superblock* sb,
                               int                inum)
{
    int bn = IBLOCK(inum, sb);
    return (struct dinode *)getBlockContent(content, bn);
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
	struct superblock *sb;
    struct superblock sbb = *(struct superblock *) (content + 1 * BSIZE);
    sb = &sbb;
    sb->size       = toMachineUint(&(sb->size));
    sb->nblocks    = toMachineUint(&(sb->nblocks));
    sb->ninodes    = toMachineUint(&(sb->ninodes));
    sb->nlog       = toMachineUint(&(sb->nlog));
    sb->logstart   = toMachineUint(&(sb->logstart));
    sb->inodestart = toMachineUint(&(sb->inodestart));
    sb->bmapstart  = toMachineUint(&(sb->bmapstart));

	int fsize, first_data_blk_no, max_block_no, 
        data_blocks_count, inodes_count; // (in blocks)
	char *inodesstart, *logstart, *bmapstart, *datastart;
#define MAX_DATA_BLOCKS_COUNT 1000
#define MAX_INODES_COUNT      10000
    struct bitmapNetwork_ bitmapNet[MAX_DATA_BLOCKS_COUNT] = {};
    struct inodesNetwork_ inodesNetwork[MAX_INODES_COUNT] = {};
    struct dinode *inodePtr, *inode;

	fsize             = sb->size;
    max_block_no      = (fsize / BSIZE) - 1;
    first_data_blk_no = sb->bmapstart + 3;
    data_blocks_count = max_block_no - first_data_blk_no + 1;
    
	inodesstart = content + sb->inodestart * BSIZE;
	logstart    = content + sb->logstart   * BSIZE;
 	bmapstart   = content + sb->bmapstart  * BSIZE;
    inodePtr    = (struct dinode *)inodesstart;

    if(!isValidRootInode(content, inodePtr + 1))
        printf("ERROR MESSAGE: root directory does not exist.\n");
    else
        printf("Valid root node\n");

	for(int i=0; i < sb->ninodes; i++) 
    {
        inode = getInodeContent(content, sb, i);

        if(!isValidInodeType(inode)) 
        {
            printf("ERROR: bad inode\n");
            continue;
        }

        if(inode->type == 0)
            continue;

        inodesNetwork[i].inuse = 1; 
        inodesNetwork[i].nlink = inode->nlink; 
        inodesNetwork[i].isDir = (inode->type == T_DIR);

        int nblocksForFile = inode->size/BSIZE + 1;
        nblocksForFile = (nblocksForFile > 13) ? 13 : nblocksForFile;
        int indirectBlockNumber = inode->addrs[NDIRECT];
        unsigned int *indirectPointers = NULL;
        if(nblocksForFile > NDIRECT)
        {
            indirectPointers = (unsigned int *) (content + indirectBlockNumber * BSIZE);
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
                blocknumber = toMachineUint(indirectPointers + (NDIRECT - j));
            }

            if((blocknumber >= first_data_blk_no) && 
               (blocknumber <  (first_data_blk_no + data_blocks_count)))
            {
                printf("ERROR: bad address in inode\n");
            }

            if(!getBitmapValueForBlock(content, sb, blocknumber))
            {
                printf("ERROR: address used by inode but marked free in bitmap.\n");
            }

            int bitmapIndex = blocknumber - first_data_blk_no;
            bitmapNet[bitmapIndex].inum[bitmapNet[bitmapIndex].count++] = i;
        }
       

        if(inode->type == T_DIR)
        {
            // Get each child's 
            for(int child = 0; child < nblocksForFile; child++)
            {
               // Find block number
               int blocknumber;
               if(child < NDIRECT)
               {
                   blocknumber = inode->addrs[child];
               }
               else
               {
                   blocknumber = toMachineUint(indirectPointers + (NDIRECT - child));
               }


               int hasDot = 0, hasDotdot = 0;
               struct dirent *entry = 
                   (struct dirent *)getBlockContent(content, blocknumber);
               for(int k=0; k < (BSIZE/sizeof(struct dirent)); k++)
               {
                   // Check this
                   if(entry->inum == 0)
                       continue;

                   ushort index = entry->inum;
                   if(strcmp(entry->name, "..") == 0)
                   {
                       hasDotdot = 1;
                       inodesNetwork[i].dotdot_inum = index;
                   }
                   else if(strcmp(entry->name, ".") == 0)
                   {
                       hasDot = 1;
                   }
                   else 
                   {
                       inodesNetwork[index].parentInum[inodesNetwork[index].count++] = i;
                   }

                   printf("%s\n", entry->name);
                   entry++;
               }

               if(!hasDot || !hasDotdot)
                   printf("ERROR: directory not properly formatted");
            }
	    } 
    }

    char *bitmapBlock = getBitmapBlock(content, sb);
    for(int i = 0; i < data_blocks_count; i++)
    {
        if(bitmapNet[1].count > 1)
        {
            printf("ERROR: address used more than once\n");
        }

        int byte_offset = i / 8;
        int shift_len   = 7 - (i % 8);
        int bitmapValue = (*(bitmapBlock + byte_offset) >> shift_len) & 1;
        if(bitmapValue != (bitmapNet[1].count > 1))
        {
            printf("ERROR: bitmap marks block in use but it is not in use");
        }
    }

    for(int i = 0; i < sb->ninodes; i++)
    {
        if(inodesNetwork[i].inuse && (inodesNetwork[i].count == 0))
        {
            printf("ERROR: inode marked use but not found in a directory.\n");
        }

        if(!inodesNetwork[i].inuse && (inodesNetwork[i].count >= 1))
        {
            printf("ERROR: inode referred to in directory but marked free.\n");
        }

        if(inodesNetwork[i].nlink != inodesNetwork[i].count)
        {
            printf("ERROR: bad reference count for file.");
        }

        if(inodesNetwork[i].isDir && (inodesNetwork[i].count > 1))
        {
            printf("ERROR: directory appears more than once in file system.");
        }

        if(!inodesNetwork[i].isDir)
            continue;

        int parent_inode_no = inodesNetwork[i].dotdot_inum;
        int mutual_ref = 0;
        for(int j = 0; j < inodesNetwork[i].count; j++)
        {
            if(inodesNetwork[i].parentInum[j] == parent_inode_no)
            {
                mutual_ref = 1;
            }
        }
        if(mutual_ref == 0)
        {
            printf("ERROR: parent directory mismatch\n");
        }
    }

    return 0;
}
