#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


#define stat xv6_stat  // avoid clash with host struct stat
#include "types.h"
#include "fs.h"
#include "stat.h"
#include "buf.h"


int fsfd;
struct superblock sb; // superblockof fsfd
void *ptr; //this will be the file system pointer starting at beginning


	//#2 For in-use inodes, each address that is used by inode is valid (points to a valid datablock address within the image). Note: must check indirect blocks too, when they are in use. 
	
	int validDataBlockCheck(struct dinode current_inode){
		uint end_blocknum=sb.size-1;																					//upper bound is total number of blocks
		uint end_bmap=sb.bmapstart + sb.size / (8 * BSIZE);									//lower bound is end of the bitmap
		if (sb.size%(8*BSIZE) !=0) {end_bmap++;}										//fixes lower bound in case of remainder
		int i;
		for (i=0; i < NDIRECT+1; i++){														//loop through direct ptrs and check addresses
			if(current_inode.addrs[i]!=0 && (current_inode.addrs[i]<end_bmap || current_inode.addrs[i]>end_blocknum ) ){
				return 1;
			}
		}
		if(current_inode.addrs[NDIRECT] != 0){											//if there are indirect ptrs...
			if (lseek(fsfd, current_inode.addrs[NDIRECT] * BSIZE, SEEK_SET)!= current_inode.addrs[NDIRECT] * BSIZE){
				perror("lseek");
				exit(1);
			}
			uint buf;
			//uint bnum;
			int x;
			for(x=0; x<NINDIRECT; x++){														//loop through indirect ptrs
				if (read(fsfd, &buf, sizeof(uint))!= sizeof(uint)){								//read the next entry
					perror("read");
					exit(1);
				}
				//memmove(bnum, buf, sizeof(uint));
				if (buf!=0 && (buf<end_bmap || buf>end_blocknum)){			//if block is out of bounds
					return 1;
				}
			}
		
		}
		return 0;
	
	}
	

	//#3 Root directory exists, and it is inode number 1. 
int checkroot(){
	if (lseek(fsfd, sb.inodestart * BSIZE + sizeof(struct dinode), SEEK_SET)!= sb.inodestart * BSIZE + sizeof(struct dinode)){
		perror("lseek");
		exit(1);
	}
	uchar buf[sizeof(struct dinode)];
	struct dinode temp_inode;
	read(fsfd, buf, sizeof(struct dinode));
	memmove(&temp_inode, buf, sizeof(struct dinode));
	if (temp_inode.type==1){return 0;}
	return 1;
}

	//#4 Each directory contains . and .. entries. ..........ASSUMING PASSING IN current DIR INODE
//#5 also assumes that this function will return not just the inum associated with ".." and "." but checks to make sure that those are the correct inums
int traverse_dir(uint addr, char *name){
	if (lseek(fsfd, addr*BSIZE, SEEK_SET) != addr*BSIZE){
		perror("lseek");
		exit(1);
	}

	struct dirent buf;
	int i;

//loop through every directory entry of current directory
	for(i=0; i<DPB; i++){
		if(read(fsfd, &buf, sizeof(struct dirent))!=sizeof(struct dirent)){
			perror("read");
			exit(1);
		}

//if there exists an entry with the name that matches the parameter "name", return the inum of that entry
		if(0==buf.inum){ continue; }
		if(strncmp(name, buf.name, DIRSIZ)==0){
			//if name == ".", you want to make sure that the inum of buf.name

			return buf.inum;
		}
	}
	return -1;
}	
	
//	takes the current inode and looks for entry with the name "name" returns the inum of that entry if it exists, else returns -1
int checkdir(struct dinode current_inode, char *name){
	int result=-1;
	int i;
	for (i=0; i < NDIRECT; i++){														//loop through direct ptrs
			//if it is an free directory entry, dont do anything
			if (current_inode.addrs[i]==0) {
				continue;
			}
			result=traverse_dir(current_inode.addrs[i], name);
			if( result != -1 ) {
				return result;
			}
	}
	
	if(current_inode.addrs[NDIRECT] != 0){											//if there are indirect ptrs...
			if (lseek(fsfd, current_inode.addrs[NDIRECT] * BSIZE, SEEK_SET)!=current_inode.addrs[NDIRECT] * BSIZE){
				perror("lseek");
				exit(1);
			}
			uint buf2;
			//uint bnum;
			int x;
			for(x=0; x<NINDIRECT; x++){														//loop through indirect ptrs

				if (lseek(fsfd, current_inode.addrs[NDIRECT] * BSIZE + x * sizeof(uint), SEEK_SET) != current_inode.addrs[NDIRECT] * BSIZE + x * sizeof(uint)){
					perror("lseek");
					exit(1);
				}
				if(read(fsfd, &buf2, sizeof(uint))!=sizeof(uint)){//read the next entry
					perror("read");
					exit(1);
				}
				result=traverse_dir(buf2, name);
				if( result != -1 ) {return result;}
				
				}
		}
	
	return result;

}

	//#5 Each .. entry in directory refers to the proper parent inode, and parent inode points back to it. 
	//addr = block number of the directory
int traverse_dir_by_inum(uint addr, ushort inum){   //given dir address, loop throught entries and check for num
	if (lseek(fsfd, addr*BSIZE, SEEK_SET) != addr*BSIZE){              //go to top of directory data block
		perror("lseek");
		exit(1);
}
	struct dirent buf;
	int i;
	for(i=0; i<DPB; i++){															//loop thorugh all dir entries
		read(fsfd, &buf, sizeof(struct dirent)); //read through one directory entry
		//if the inumber of the directory entry matches the inum in parameter
		if(buf.inum==inum){
			return 0;
		}
	}
	return 1;
}	


//checks to see if the parent of passed in inode with inum has a reference to its child
int checkParent(int inum, int parent_inum){
	struct dinode parent;
	int parent_inode_location=sb.inodestart*BSIZE + parent_inum * sizeof(struct dinode);			
	if (lseek(fsfd, parent_inode_location, SEEK_SET) != parent_inode_location){
		perror("lseek");
		exit(1);
  }
	if (read(fsfd, &parent, sizeof(struct dinode)) != sizeof(struct dinode)){ //get parent inode
		perror("read");
		exit(1);
}																						
	int result=1;
	int i;
	for (i=0; i < NDIRECT; i++){														//loop through direct ptrs
			if (parent.addrs[i]==0) {continue;}
			result=traverse_dir_by_inum(parent.addrs[i], (ushort) inum); //pass in the address of directory entry of the "parent" directory and the inum for its current directory entry
			//result will be 0 if you find an entry in parent directory that matches the childs inum
			if(result == 0) {return 0;}
	}
	printf("checking to see if there are indirect pointers\n");
	if(parent.addrs[NDIRECT] != 0){											//if there are indirect ptrs...
			printf("looking through indirect pointers \n");
			if (lseek(fsfd, parent.addrs[NDIRECT] * BSIZE, SEEK_SET)!= parent.addrs[NDIRECT]*BSIZE){
				perror("lseek");
				exit(1);
			}
			uint buf2;
	
			int x;
			for(x=0; x<NINDIRECT; x++){														//loop through indirect ptrs
				if (lseek(fsfd, parent.addrs[NDIRECT] * BSIZE + x * sizeof(uint), SEEK_SET) != parent.addrs[NDIRECT] * BSIZE + x * sizeof(uint)){
					perror("lseek");
					exit(1);
				}
				if (read(fsfd, &buf2, sizeof(uint)) != sizeof(uint)){														//read the next entry
					perror("read");
					exit(1);
				}
				if(0==buf2) {continue;}

				result=traverse_dir_by_inum(buf2,(ushort) inum);
				if( result == 0 ) {return 0;}
				
				}
		}

	return result;

}

//checks to see if each child of a parent points back to its parent
//BY DEFAULT THIS RETURNS TRUE(0) SO MAKE SURE ALL CASES WHERE ITS INCORRECT ARE RETURNING 1
//
int checkChild(struct dinode parent, uint parent_inum){
//printf("entered checkchild function with inode %d\n",parent_inum);
	int i;
	struct dirent child_entry;
	struct dinode child_inode;
	//loop through all the direct pointers of the parent
	for (i=0; i < NDIRECT; i++){														//loop through direct ptrs
			if (parent.addrs[i]==0) {continue;}
						//go to datablock with directory
			int j;
			for(j=0; j<DPB; j++){																//loop through all children dir entries

				if (lseek(fsfd, parent.addrs[i] * BSIZE + j*sizeof(struct dirent), SEEK_SET) !=  parent.addrs[i] * BSIZE + j*sizeof(struct dirent)){ //move to next dir entry
					perror("lseek");
					exit(1);
				}
				if (read(fsfd, &child_entry, sizeof(struct dirent)) != sizeof(struct dirent)){																	 //read dir entry
					perror("read");
					exit(1);
				}
				//printf("child_entry at index %d has inum value is %d\n",j,child_entry.inum);
				if(0==child_entry.inum){continue;}
				if(strncmp(child_entry.name,".",DIRSIZ)==0 && child_entry.inum!=parent_inum) { 
					printf("the '.' entry doesnt match parent  \n");
								return 1;
				}
				//you dont want to recheck same directory
				if(strncmp(child_entry.name, ".", DIRSIZ)==0){ continue;}

			
				if(strncmp(child_entry.name,"..",DIRSIZ)==0){
					//if you are looking at the .. entry in the directory and directory is not root directory but its .. entry == its inum
					if((child_entry.inum !=1 && child_entry.inum == parent_inum)){
								return 1;
					}
					//coontinue because you dont want to check the parent of the parent
					continue;
				}
				
				if (lseek(fsfd, sb.inodestart*BSIZE + child_entry.inum * sizeof(struct dinode), SEEK_SET) !=  sb.inodestart*BSIZE + child_entry.inum * sizeof(struct dinode)){
					perror("lseek");
					exit(1);
				}
				if (read(fsfd, &child_inode, sizeof(struct dinode)) != 	sizeof(struct dinode)){													//read child inode
					perror("read");
					exit(1);
				}
				//if the child inode is not a direcory you dont need to chek it further
				if(child_inode.type!=T_DIR){continue;}

				//you dont want to check the parents parent
			
				int x;
				for (x=0; x < NDIRECT; x++){           //loop through child_indode's directories via direct ptrs
					struct dirent entry;
					if (child_inode.addrs[x] != 0){      //if entry exists		
						int k;
						for(k=0; k<DPB; k++){																//loop through all children dir entries

							if (lseek(fsfd, child_inode.addrs[x] * BSIZE + k*sizeof(struct dirent), SEEK_SET) != child_inode.addrs[x] * BSIZE + k*sizeof(struct dirent)){  //move to next dir entry
								perror("lseek");
								exit(1);
							}
							if (read(fsfd, &entry, sizeof(struct dirent)) != sizeof(struct dirent)){																	 //read dir entry
								perror("read");
								exit(1);
							}
	

							//if your looking at the .. entry and its inum doesnt match the parent
							if(strncmp(entry.name,"..",DIRSIZ)==0 && entry.inum != parent_inum){
								printf("the parent inum DOESNT match the '..' entry in the inum, about to return from checkchild \n");
								return 1;
							}
					
						}
					}
				}
				
				if( child_inode.addrs[NDIRECT] !=0 ){												 //check for CHILD indirect ptrs

					printf("child inode has indirect pointers\n");
					if (lseek(fsfd, child_inode.addrs[NDIRECT] * BSIZE, SEEK_SET) != child_inode.addrs[NDIRECT] * BSIZE){	//go to data block of ptrs
						perror("lseek");
						exit(1);
					}

					int y;
					uint ptr;
					for(y=0; y<NINDIRECT; y++){														//loop through indirect ptrs
						if (lseek(fsfd, child_inode.addrs[NDIRECT] * BSIZE + y * sizeof(uint), SEEK_SET) != child_inode.addrs[NDIRECT] * BSIZE + y * sizeof(uint)){  //move to next dir ptr
							perror("lseek");
							exit(1);
						}
						if (read(fsfd, &ptr, sizeof(uint)) != sizeof(uint)){														//read the next dir ptr
							perror("read");
							exit(1);
						}			
						int q;
						struct dirent i_entry;
						for(q=0; q<DPB; q++){
							if (lseek(fsfd, ptr*BSIZE + sizeof(struct dirent)*q, SEEK_SET) !=  ptr*BSIZE + sizeof(struct dirent)*q){
								perror("lseek");
								exit(1);
							}
							if (read(fsfd, &i_entry, sizeof(struct dirent)) != sizeof(struct dirent)){
								perror("read");
								exit(1);
							} 
							if(strncmp(i_entry.name,"..",DIRSIZ)==0 && i_entry.inum != parent_inum){
								//printf("the parent inum DOESNT match the '..' entry in the inum, about to return from checkchild \n");
								return 1;
							}
						}
					}
				}
			}			
		
		
		//loop through directories pointed to by parent's indirect pointers	
		if (parent.addrs[NDIRECT] !=0){
			printf("parent has indirect pointers \n");
			uint directory_address;
			int z;
			for(z=0; z<NINDIRECT; z++){					//loop through all indirect ptr entires
				if (lseek(fsfd, parent.addrs[NDIRECT]*BSIZE + z*sizeof(uint), SEEK_SET) != parent.addrs[NDIRECT]*BSIZE + z*sizeof(uint)){
					perror("lseek");
					exit(1);
				}
				if (read(fsfd, &directory_address, sizeof(uint)) != sizeof(uint)){
					perror("read");
					exit(1);
				}
				if(directory_address==0) {continue;} 

			int j;
			for(j=0; j<DPB; j++){																//loop through all children dir entries
				if (lseek(fsfd, directory_address * BSIZE + j*sizeof(struct dirent), SEEK_SET)  != directory_address * BSIZE + j*sizeof(struct dirent)){//move to next dir entry
					perror("lseek");
					exit(1);
				}
				if (read(fsfd, &child_entry, sizeof(struct dirent)) != sizeof(struct dirent)){																		 //read dir entry
					perror("read");
					exit(1);
				}

					
					if(0==child_entry.inum){continue;}
				
					if(strncmp(child_entry.name,".",DIRSIZ)==0 && child_entry.inum!=parent_inum) { 
						printf("the '.' entry doesnt match parent  \n");
						return 1;
					}
					//you dont want to recheck same directory
					if(strncmp(child_entry.name, ".", DIRSIZ)==0){ continue;}

			
					if(strncmp(child_entry.name,"..",DIRSIZ)==0){
						//if you are looking at the .. entry in the directory and directory is not root directory but its .. entry == its inum
						if((child_entry.inum !=1 && child_entry.inum == parent_inum)){
								return 1;
						}
						//coontinue because you dont want to check the parent of the parent
						continue;
					}
	
				if (lseek(fsfd, sb.inodestart*BSIZE + child_entry.inum * sizeof(struct dinode), SEEK_SET) != sb.inodestart*BSIZE + child_entry.inum * sizeof(struct dinode)){
					perror("lseek");
					exit(1);
				} 
				if (read(fsfd, &child_inode, sizeof(struct dinode)) != sizeof(struct dinode)){															//read child inode
					perror("read");
					exit(1);
				}

				
					//if the child inode is not a direcory you dont need to chek it further
					if(child_inode.type!=T_DIR){continue;}

					//you dont want to check the parents parent
				
				
					int x;
	
		
				for (x=0; x < NDIRECT; x++){           //loop through child_indode's directories via direct ptrs
					struct dirent entry;
					if (child_inode.addrs[x] != 0){      //if entry exists		
						int k;
						for(k=0; k<DPB; k++){																//loop through all children dir entries
							if (lseek(fsfd, child_inode.addrs[x] * BSIZE + k*sizeof(struct dirent), SEEK_SET) != child_inode.addrs[x] * BSIZE + k*sizeof(struct dirent)){  //move to next dir entry
								perror("lseek");
								exit(1);
							}
							if (read(fsfd, &entry, sizeof(struct dirent)) != sizeof(struct dirent)){																		 //read dir entry
								perror("read");
								exit(1);
							}
							//printf("In the entry %d and looking at entry with name %s and inum %d\n",k,entry.name,entry.inum);
							//printf(".. entry has the inum %d and the passed in parent was %d, these should match\n",entry.inum,parent_inum);

							//if your looking at the .. entry and its inum doesnt match the parent
							if(strncmp(entry.name,"..",DIRSIZ)==0 && entry.inum != parent_inum){
							//	printf("the parent inum DOESNT match the '..' entry in the inum, about to return from checkchild \n");
								return 1;
							}

					
						}
					}
					}

				if( child_inode.addrs[NDIRECT] !=0 ){												 //check for CHILD indirect ptrs

					if (lseek(fsfd, child_inode.addrs[NDIRECT] * BSIZE, SEEK_SET) != child_inode.addrs[NDIRECT] * BSIZE){	//go to data block of ptrs
						perror("lseek");
						exit(1);
					}
					int y;
					uint ptr;
					//struct dirent indireentry;
					for(y=0; y<NINDIRECT; y++){														//loop through indirect ptrs
						if (lseek(fsfd, child_inode.addrs[NDIRECT] * BSIZE + y * sizeof(uint), SEEK_SET) != child_inode.addrs[NDIRECT] * BSIZE + y * sizeof(uint)){ //move to next dir ptr
							perror("lseek");
							exit(1);
						}
						if (read(fsfd, &ptr, sizeof(uint)) != sizeof(uint)){													//read the next dir ptr
							perror("read");
							exit(1);
						}
						int q;
						struct dirent i_entry;
						for(q=0; q<DPB; q++){
							if (lseek(fsfd, ptr*BSIZE + sizeof(struct dirent)*q, SEEK_SET) != ptr*BSIZE + sizeof(struct dirent)*q){
								perror("lseek");
								exit(1);
							}
							if (read(fsfd, &i_entry, sizeof(struct dirent)) != sizeof(struct dirent)){
								perror("read");
								exit(1);
							} 
							if(strncmp(i_entry.name,"..",DIRSIZ)==0 && i_entry.inum != parent_inum){
								printf("the parent inum DOESNT match the '..' entry in the inum, about to return from checkchild \n");
								return 1;

							}
						}
				}
				
				
			
			}
		}
		}
}
}
		return 0;	
}



	//#6 For in-use inodes, each address in use is also marked in use in the bitmap. 

int checkbitmap(struct dinode current_inode){
		int i;
		uint buf;
		uint buf2;
		for (i=0; i < NDIRECT+1; i++){														//loop through direct ptrs and check addresses
			if (current_inode.addrs[i]==0) {continue;}
			if (lseek(fsfd, sb.bmapstart*BSIZE + current_inode.addrs[i]/8,SEEK_SET) != sb.bmapstart*BSIZE + current_inode.addrs[i]/8 ){//set loc to byte where bit is located
				perror("lseek");
				exit(1);
			}
			if (read(fsfd, &buf2, 1) != 1){	//read byte containing bit we want
				perror("read");
				exit(1);
			}
			int shift_amt=current_inode.addrs[i]%8;
			buf2=buf2 >> shift_amt;
			buf2=buf2%2;
			
			if(buf2==0) {return 1;}
		}
		if(current_inode.addrs[NDIRECT] != 0){											//if there are indirect ptrs...
			int x;
			for(x=0; x<NINDIRECT; x++){														//loop through indirect ptrs

				if (lseek(fsfd, current_inode.addrs[NDIRECT] * BSIZE + x*sizeof(uint), SEEK_SET) != current_inode.addrs[NDIRECT] * BSIZE + x*sizeof(uint)){
					perror("lseek");
					exit(1);
				}
				if (read(fsfd, &buf, sizeof(uint)) != sizeof(uint)){														//read the next entry
					perror("read");
					exit(1);
				}
				//memmove(bnum, buf, sizeof(uint));

				if (buf!=0){			//if using address
					if (lseek(fsfd, sb.bmapstart*BSIZE + buf/8,SEEK_SET) != sb.bmapstart*BSIZE + buf/8){ //set loc to byte where bit is located
						perror("lseek");
						exit(1);
					}
					if (read(fsfd, &buf2, 1) != 1){		//read byte containing bit we want
						perror("read");
						exit(1);
					}
					int shift_amt=current_inode.addrs[i]%8;
					buf2=buf2 >> shift_amt;
					buf2=buf2%2;
					if(buf2==0) {return 1;}
				}
			}
		
		}
		return 0;
}

	//#7 For blocks marked in-use in bitmap, actually is in-use in an inode or indirect block somewhere. 
int bitMapCheck(uint* addresses){
	int start=sb.bmapstart*BSIZE + sb.size/8 - sb.nblocks/8;          //starting moving through bitmap at datablock section
	uint bit;
	int count=(sb.bmapstart + 1);				//keep count of what address we're at..start @ beginning of datablocks

	//printf("count: %d\n",count);
	if (lseek(fsfd, start, SEEK_SET) != start){
		perror("lseek");
		exit(1);
	}

	int i;
	int byte;
	for (i=count; i<sb.size; i+=8){
		read(fsfd, &byte, 1);
		int x;
		for (x=0; x<8; x++, count++){
			//printf("for blocknum: %d ", count);
			bit= (byte >> x)%2;
			//printf("found bit %u\n",bit);
			if (bit!=0){
				//printf("addresses array: %d\n", addresses[count]);
				if(addresses[count]==0){
					return 1;
				}
			}
		}
	} 
	return 0;
}

	//#8 For in-use inodes, any address in use is only used once. 
int addressCheck(uint* addresses, struct dinode current_inode){
	int i;
	for(i=0;i<NDIRECT+1;i++){																				//loop through inode's direct ptrs
		if(current_inode.addrs[i] == 0) {continue;}										//if unused continue
		//printf("address: %u\n",current_inode.addrs[i]);
		if(addresses[current_inode.addrs[i]] == 1) {return 1;}				//check to see if address has been used before
		addresses[ current_inode.addrs[i] ]=1;												//if not, mark as used
	}
	
	int j;
	uint address;
	if(current_inode.addrs[NDIRECT] != 0){
		for(j=0; j<NINDIRECT; j++){
			if (lseek(fsfd, current_inode.addrs[NDIRECT] * BSIZE + j*sizeof(uint), SEEK_SET) != current_inode.addrs[NDIRECT] * BSIZE + j*sizeof(uint)){
				perror("lseek");
				exit(1);
			}
			if (read(fsfd, &address, sizeof(uint)) != sizeof(uint)){
				perror("read");
				exit(1);
			}
			if(address==0) {continue;}
			//printf("iaddress: %u\n", address);
			//printf("%u\n",addresses[address]);
			if(addresses[address] == 1) {return 1;}
			addresses[address]=1;
		}
	}
	return 0;
}
	//#9 For inodes marked used in inode table, must be referred to in at least one directory. 
//make sure that nlink field for each inode is not 0
//kernel will only store inode in memory if there are pointers referring to it 'ref' field in struct inode counts number of C pointers referring to the in memory inode, kernel will discard inode from memory if refernce count drops to 0

int inDir(struct dinode current_inode, uint current_inum){
	int inum;
	//if (current_inum==1) {return 0;}
	//printf("looking for inum %u\n", current_inum);
	struct dinode in;
	for (inum=0; inum<sb.ninodes; inum++){																			//loop through all inodes
		//printf("looking at inode %u\n", inum);
		if (inum==current_inum  && inum!=1) {continue;}																		//skip over current inode
		if (lseek(fsfd, sb.inodestart*BSIZE + inum * sizeof(struct dinode), SEEK_SET) != sb.inodestart*BSIZE + inum * sizeof(struct dinode)){	//seek and read inode
			perror("lseek");
			exit(1);
		}
		if (read(fsfd, &in, sizeof(struct dinode)) != sizeof(struct dinode)){
			perror("read");
			exit(1);
		}
		if (in.type!=T_DIR) {continue;}																				//check to see if inode is a dir
		
		int x;
		for(x=0; x<NDIRECT; x++){  //loop through direct ptrs
			//printf("looking at %u's dir at adr %u\n",inum, in.addrs[x]);
			if(in.addrs[x]==0) {continue;}
			//printf("%d\n", traverse_dir_by_inum(in.addrs[x], current_inum) );
			if(0==traverse_dir_by_inum(in.addrs[x], current_inum)){return 0;}
		}
		int y;
		uint directory_address;
		if (in.addrs[NDIRECT]!=0){		//if there is an indirect ptr..
			for(y=0; y<NINDIRECT; y++){
				if (lseek(fsfd, in.addrs[NDIRECT] * BSIZE + y*sizeof(uint), SEEK_SET) != in.addrs[NDIRECT] * BSIZE + y*sizeof(uint)){
					perror("lseek");
					exit(1);
				}
				if (read(fsfd, &directory_address, sizeof(uint)) != sizeof(uint)){
					perror("read");
					exit(1);	
				}
				if(0==directory_address) {continue;}
				if(0==traverse_dir_by_inum(directory_address, current_inum)) {return 0;}
			}
		}
	}
	return 1;

}

	//#10 For inode numbers referred to in a valid directory, actually marked in use in inode table.
	
int checkTable(struct dinode current_inode){
	//printf("inode type is: %u\n",current_inode.type);
	int i;
	struct dirent entry;
	struct dinode temp;
	uint addr;
	for(i=0; i< NDIRECT; i++){		//loop through direct ptrs
		if(current_inode.addrs[i]==0){continue;}
		int x;
		for (x=0; x<DPB; x++){
			if (lseek(fsfd, current_inode.addrs[i] * BSIZE + x*sizeof(struct dirent), SEEK_SET) != current_inode.addrs[i] * BSIZE + x*sizeof(struct dirent)){  //move to directory
				perror("lseek");
				exit(1);
			}
			if (read(fsfd, &entry, sizeof(struct dirent)) != sizeof(struct dirent)){   //read dir entry
				perror("read");
				exit(1);
			}
			if(entry.inum==0) {continue;}
			if (lseek(fsfd, sb.inodestart*BSIZE + entry.inum * sizeof(struct dinode), SEEK_SET) != sb.inodestart*BSIZE + entry.inum * sizeof(struct dinode)){
				perror("lseek");
				exit(1);
			}  
			if (read(fsfd, &temp, sizeof(struct dinode)) != sizeof(struct dinode)){		//read inode
				perror("read");
				exit(1);
			}
			//printf("looking at inode #%u\n", entry.inum);
			//printf("temp's type is %u\n",temp.type);
			if(temp.type==0) {return 1;}		//check type
		}
	}
	
	if(current_inode.addrs[NDIRECT]!=0){
		int y;
		for(y=0; y<NINDIRECT; y++){
			if (lseek(fsfd, current_inode.addrs[NDIRECT] * BSIZE + y*sizeof(uint), SEEK_SET) != current_inode.addrs[NDIRECT] * BSIZE + y*sizeof(uint)){
				perror("lseek");
				exit(1);
			}
			if (read(fsfd, &addr, sizeof(uint)) != sizeof(uint)){
				perror("read");
				exit(1);
			}
			int z;
			for(z=0; z<DPB; z++){
				if (lseek(fsfd, addr*BSIZE + z*sizeof(struct dirent), SEEK_SET) !=addr*BSIZE + z*sizeof(struct dirent)){
					perror("lseek");
					exit(1);
				}
				if (read(fsfd, &entry, sizeof(struct dirent)) != sizeof(struct dirent)){
					perror("read");
					exit(1);
				}
				if(entry.inum==0) {continue;}
				if (lseek(fsfd, sb.inodestart*BSIZE + entry.inum * sizeof(struct dinode), SEEK_SET) !=  sb.inodestart*BSIZE + entry.inum * sizeof(struct dinode)){
					perror("lseek");
					exit(1);
				}
				if (read(fsfd, &temp, sizeof(struct dinode)) != sizeof(struct dinode)){
					perror("read");
					exit(1);
				}
				if(temp.type==0) {return 1;}
			}
		}
	}
	return 0;
} 

	//# 11 Reference counts (number of links) for regular files match the number of times file is referred to in directories (i.e., hard links work correctly). 
	//nlink field in dinode struct counts number of directory entires that refer to this inode
int checkLinks(struct dinode current_inode, uint current_inum){
	//printf("links=%u\n",current_inode.nlink);
	int inum;
	int count=0;
	//if (current_inum==1) {return 0;}
	//printf("looking for inum %u\n", current_inum);
	struct dinode in;
	for (inum=0; inum<sb.ninodes; inum++){																			//loop through all inodes
		//printf("looking at inode %u\n", inum);
		if (inum==current_inum  && inum!=1) {continue;}																		//skip over current inode
		if (lseek(fsfd, sb.inodestart*BSIZE + inum * sizeof(struct dinode), SEEK_SET) != sb.inodestart*BSIZE + inum * sizeof(struct dinode)){	//seek and read inode
			perror("lseek");
			exit(1);
		}
		if (read(fsfd, &in, sizeof(struct dinode)) != sizeof(struct dinode)){
			perror("read");
			exit(1);
		}
		if (in.type!=T_DIR) {continue;}																				//check to see if inode is a dir
		
		int x;
		for(x=0; x<NDIRECT; x++){  //loop through direct ptrs
			//printf("looking at %u's dir at adr %u\n",inum, in.addrs[x]);
			if(in.addrs[x]==0) {continue;}
			//printf("%d\n", traverse_dir_by_inum(in.addrs[x], current_inum) );
			if(0==traverse_dir_by_inum(in.addrs[x], current_inum)){count++;}
		}
		int y;
		uint directory_address;
		if (in.addrs[NDIRECT]!=0){		//if there is an indirect ptr..
			for(y=0; y<NINDIRECT; y++){
				if (lseek(fsfd, in.addrs[NDIRECT] * BSIZE + y*sizeof(uint), SEEK_SET) != in.addrs[NDIRECT] * BSIZE + y*sizeof(uint)){
					perror("lseek");
					exit(1);
				}
				if (read(fsfd, &directory_address, sizeof(uint)) != sizeof(uint)){
					perror("read");
					exit(1);
				}
				if(0==directory_address) {continue;}
				if(0==traverse_dir_by_inum(directory_address, current_inum)) {count++;}
			}
		}
	}
	//printf("done\n");
	return count;
}


	
void
rsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
    perror("lseek");
    exit(1);
  }
  if(read(fsfd, buf, BSIZE) != BSIZE){
    perror("read");
    exit(1);
  }
}
	
int main(int argc, char *argv[]) {
	
  // Confirm filesystem image name is provided as an argument
  if (argc < 2) {
    printf("Usage: chkfs IMAGE\n");
    return 1;
  } 
	
  // Open file system image
  fsfd = open(argv[1], O_RDONLY);
 
  if (fsfd < 0) { //check if file is present
    perror(argv[1]);
    return 1;
  }

	uchar buf[BSIZE];
	rsect(SUPERBLOCK,buf);
	memmove(&sb, buf, sizeof(sb));

	
	//struct dinode *dip;
	int inum;
	struct dinode current_inode;
	

	int data_test=0;//used for result of valid data block check
	uint addresses[sb.size];  //array for error 8
	int q;


//loop through all of the blocks and populate address array 
	for(q=0;q<sb.size;q++) {
		addresses[q]=0;
	}

	for (inum = 0; inum < ((int) sb.ninodes); inum++){  //loops through all inodes
		if (lseek(fsfd, sb.inodestart * BSIZE + inum * sizeof(struct dinode), SEEK_SET) != sb.inodestart * BSIZE + inum * sizeof(struct dinode)){  //move to correct location
			perror("lseek");
    		exit(1);
		}
		if(read(fsfd, buf, sizeof(struct dinode))!=sizeof(struct dinode)){	//read inode info into buffer
			perror("read");
    		exit(1);
		}
		memmove(&current_inode, buf, sizeof(current_inode));//put into struct current_inode

	  
	  
	  //for error 1----------------------------------------------------------------------
		if (current_inode.type!=0 && current_inode.type!=T_FILE && current_inode.type!=T_DIR && current_inode.type!=T_DEV){  //check for valid type
			close(fsfd);
			printf("ERROR: bad inode\n");
			return 1;
		}
		
		
		else if (current_inode.type!=0){
			//for error 2----------------------------------------------------------------------
			data_test = validDataBlockCheck(current_inode);
			if (data_test==1){
				close(fsfd);
				printf("ERROR: bad address in inode\n");
				return 1;
			}
			
			//for error 6----------------------------------------------------------------------
			if(1==checkbitmap(current_inode)){
				close(fsfd);
				printf("ERROR: address used by inode but marked free in bitmap\n");
				return 1;
			}
			
			//for error 8----------------------------------------------------------------------
			if(1==addressCheck(addresses, current_inode)){
				close(fsfd);
				printf("ERROR: address used more than once\n");
				return 1;
			}

			//for error 9-------------------------------------------------------------------------
			if(1==inDir(current_inode, inum) ){
				close(fsfd);
				printf("ERROR: inode marked use but not in directory\n");
				return 1;
			}
			

		}
		

		if (current_inode.type==T_DIR){
			//for error 4----------------------------------------------------------------------
			int inum_dot;
			int inum_doubledot;
			inum_dot=checkdir(current_inode,".");
			inum_doubledot=checkdir(current_inode,"..");
			//printf("current inum is: %u and inum_dot is %u\n", inum, inum_dot);
			if (-1==inum_dot || -1==inum_doubledot){
				//printf("inum_dot: %d, inum_doubledot: %d\n", inum_dot, inum_doubledot);
				close(fsfd);
				printf("ERROR: directory not properly formatted\n");
				return 1;
			}
			
			
			//for error 5----------------------------------------------------------------------
			//if this is the root direcotry check that the inum_dot  == inum
			if(1 == inum){
				//printf("inum is 1 so this is the root directory. dont check child or parent\n");
				if(inum != inum_dot){
					printf("root inode doesnt point to itself\n");
					return 1;
				}
			}
			else{
				int parentcheck = checkParent(inum, inum_doubledot) ;
				if(1 == parentcheck){
					close(fsfd);
					printf("ERROR: parent directory mismatch\n");
					return 1;
				}
				
				//only check children if current inode is a directory
					int childcheck = checkChild(current_inode,inum);
					//printf("parentcheck returns %d and child check returned %d \n",parentcheck, childcheck);
					if(1==childcheck){
					close(fsfd);
					printf("ERROR: parent directory mismatch\n");
					return 1;
					}
				
			}
			
			
			//for error 10------------------------------------------------------------------------
			if(1==checkTable(current_inode)){
				close(fsfd);
				printf("ERROR: inode referred to in directory but marked free\n");
				return 1;
			}
			
			
		}
		//for error 11---------------------------------------------------------------------------
		if(current_inode.type==T_FILE){
			if(current_inode.nlink!=checkLinks(current_inode, inum)){
				close(fsfd);
				printf("ERROR: bad reference count for file\n");
				return 1;
			}
		}
	}
	
	
	
	//for error 3----------------------------------------------------------------------
	int root_dir_test=0;
	root_dir_test=checkroot();
	if(root_dir_test==1){
		close(fsfd);
		printf("ERROR: root directory does not exist\n");
		return 1;
	}
	
	//for error 7------------------------------------------------------------------------
	if (bitMapCheck(addresses)==1){
		close(fsfd);
		printf("ERROR: bitmap marks block in use but it is not in use\n");
		return 1;
	}
	
	
	
	
	
  // Close file system image
  close(fsfd);
  return 0;
}
	
	

