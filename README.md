# fsck
## Overview
A file system checker for the xv6 operating system.  
  
This file system checker will check/validate an operating system to make sure itsatisfies the following conditions:  
* Each inode is either unallocated or one of the valid types (T_FILE, T_DIR, T_DEV).
**ERROR:** *bad inode*  
* For in-use inodes, each address that is used by inode is valid (points to a valid datablock address within the image). Note: must check indirect blocks too, when they are in use.  
**ERROR:** *bad address in inode*  
* Root directory exists, and it is inode number 1.  
**ERROR:** *root directory does not exist*  
* Each directory contains . and .. entries.  
**ERROR:** *directory not properly formatted*  
* Each .. entry in directory refers to the proper parent inode, and parent inode points back to it.  
**ERROR:** *parent directory mismatch*  
* For in-use inodes, each address in use is also marked in use in the bitmap.  
**ERROR:** *address used by inode but marked free in bitmap*  
* For blocks marked in-use in bitmap, actually is in-use in an inode or indirect block somewhere.  
**ERROR:** *bitmap marks block in use but it is not in use*  
* For in-use inodes, any address in use is only used once.  
**ERROR:** *address used more than once*  
* For inodes marked used in inode table, must be referred to in at least one directory.  
**ERROR:** *inode marked use but not found in a directory*  
* For inode numbers referred to in a valid directory, actually marked in use in inode table.  
**ERROR:** *inode referred to in directory but marked free*  
* Reference counts (number of links) for regular files match the number of times file is referred to in directories (i.e., hard links work correctly).  
**ERROR:** *bad reference count for file*  
  
## Testing/Usage
First, run this on an uncorrupted file system:  
```
make qemu-nox //run xv6  
mkdir xyz //create some files and/or directories  
exit xv6 (ctrl+a followed by x)  
run chkfs on fs.img  
```
Then, try it out on a corrupted filesystem:
```
cp fs.img corrupted.img  
./corruptfs corrupted.img TYPE  
chkfs corrupted.img  
```
TYPE is respective to the errors lsited above e.g. bad inode would be type 1  
  
The corruptfs program randomly chooses which directory, inode, etc. to corrupt. To make the random selection deterministic, include a random seed (i.e., a positive integer) as an additional command line argument to corruptfs. Also, because of the random nature of corruptfs, it will occasionally make a random choice such that it is impossible to introduce the specified type of corruption. If this occurs, corruptfs will indicate that it failed to corrupt the image; simply run corruptfs again (with a different random seed) so it makes a different random choice.
