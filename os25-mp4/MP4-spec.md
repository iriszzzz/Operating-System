# MP4 Filesystem

## Introduction

- In this MP, you will learn the fundamental knowledge of the file system by adding two features to xv6: large files and symbolic links. We strongly recommend you read Chapter 8 (file system) in [xv6 hand book ](https://pdos.csail.mit.edu/6.828/2023/xv6/book-riscv-rev3.pdf)while you trace code. This gives you a quick overview of how xv6 implements its file system.

### Instructions

- Refer to link [student guide](https://git.lsalab.cs.nthu.edu.tw/os25/os25_shared_xv6/src/branch/student/) to launch the development environment.

## Part 1: Large Files

### Description:

In this part, you are required to extend the xv6 file system to support large files by increasing the maximum number of data blocks that a file can use. The core idea is to implement **doubly-indirect blocks**. Each doubly-indirect block will contain 256 pointers to singly-indirect blocks, and each singly-indirect block will contain 256 pointers to data blocks. Pleace replace one direct block with a doubly-indirect block to increase the file capacity. However, to reach our target of **up to 66666 data blocks**, you will need to incorporate extra doubly-indirect blocks in your design.

### Trace Code:

**Part A: Writing to a Large File** 

A user program writes a large amount of data. It calls the `write()` system call.
1. `kernel/sysfile.c/sys_write()`
2. `kernel/file.c/filewrite() `
3. `kernel/fs.c/writei()`
4. `kernel/fs.c/bmap()`

**Part B: Deleting a Large File** 

The user deletes the large file using the rm utility.
1. `kernel/sysfile.c/sys_unlink()`
2. `kernel/fs.c/iunlockput()/iput()` 
3. `kernel/fs.c/itrunc()`

### Question:

1. What is the current maximum file size xv6 can support? How many numbers of direct and indirect blocks are there?
2. How many blocks increase when replacing a direct block with a doubly-indirect block?
3. The `bmap` and `itrunc` functions make use of the buffer cache via `bread`, `brelse`, and `log_write`. Explain the above function .
4. Explain the importance of calling `brelse` after you are done with a buffer from `bread`. 
5. To ensure crash safety, xv6 uses a write-ahead log. Trace the lifecycle of a file system transaction when creating a new file. Start from a system call like sys_open() in kernel/sysfile.c that calls create(). Explain the roles of `begin_op()`, `log_write()`, and `end_op()` from `kernel/log.c`. Describe what gets written to the on-disk log and what constitutes a "commit". Finally, explain how the recovery code (`recover_from_log`) uses the log to ensure the operation is atomic after a crash. We strongly encourage you to read the logging part(8.4) in xv6 handbook to get familiar with loggin mechanism.
6. Explain the roles of the in-memory inode functions `iget()` and `iput()`. Describe the purpose of the reference count (`ref`) in `struct inode`.

## Guidelines and Hints

1. `kernel/fs.h` describes the structure of an on-disk inode. The address of the data block is stored in
addrs. Note that the length of addrs is always 13.
2. If you change the definition of `NDIRECT`, make sure to run make clean to delete fs.img. Then run make qemu to create a new fs.img, since mkfs uses `NDIRECT` to build the file system.
3. You should allocate indirect blocks and doubly-indirect blocks only as needed, like the original `bmap()`.
4. Make sure `itrunc()` frees all blocks of a file, including doubly-indirect blocks.
5. You can pass problem 1 with modifying only: `fs.c`, `fs.h` and `file.h`.

## Part 2:  Symbolic Links to Files

### Description

In this problem you will add symbolic links to xv6. Symbolic links (or soft links) refer to a linked file by pathname; when a symbolic link is opened, the kernel follows the link to the referred file. Implementing this system call is a good exercise to understand how pathname lookup works. You will implement the `symlink(char *target, char *path)` system call, which creates a new symbolic link at path that refers to a file named target. In addition, you also need to handle `open` when encountering symbolic links. If the target is also a symbolic link, you must recursively follow it until a non-link file is reached. If the links form a cycle, you must return an error code. You may approximate this by returning an error code `-3` if the depth of links reaches threshold 5. 
There's a new flag `O_NOFOLLOW` in open mode. When it's specified, `open()` should not follow the symbolic link. If the target is a symbolic link, `return -2`.

## Guidelines and Hints

1. Checkout `kernel/sysfile.c`. There is an unimplemented function `sys_symlink`. Note that system call
symlink is already added in xv6, so you don’t need to worry about that.
2. Checkout `kernel/stat.h`. There is a new file type `T_SYMLINK`, which represents a symbolic link.
3. Checkout `kernel/fcntl.h`. There is a new flag `O_NOFOLLOW` that can be used with the open system call.
4. The target does not need to exist for the system call to succeed.
5. You will need to store the target path in a symbolic link file, for example, in inode data blocks.
6. `symlink` should return an integer representing 0(success) or -1(failure) similar to link and unlink.
7. Modify the open system call to handle paths with symbolic links. If the file does not exist, open must fail.
8. Don’t worry about other system calls (e.g., link and unlink). They must not follow symbolic links; these system calls operate on the symbolic link itself.
9. You do not have to handle symbolic links to directories in this part.
10. Consider only `absolute path`, you don't need to handle relative path.
11. You can pass problem 2 with modifying only: `sysfile.c`.

## Part 3:  Symbolic Links to Directories

### Description

Instead of just implementing symbolic links to files, now you should also consider symbolic links to directories. We expect that a symbolic link to a directory should have these properties:

1. It can be part of a path, and will redirect to what it links to.
2. You can cd a symbolic link if it links to a directory.
For example, symlink("/y/", "/x/a") creates a symbolic link /x/a links to /y/. The actual path of
/x/a/b should be /y/b. Thus, if you write to /x/a/b, you actually write to /y/b. Also, if you cd into /x/a, your working directory should become /y/.

Note: Cycle detection is required when resolving symbolic link paths. For implementation simplicity in this part, please return a standard error code of -1 upon detecting a cycle(depth threshold 5).

## Guidelines and Hints

1. Checkout `TODO` in the skeleton code.
2. You can leave sys symlink function unchanged, since symbolic links store paths as strings. There is no difference between a file path and a directory path.
3. You have to handle paths that consist of symbolic links. Check `namex` function in fs.c.
4. You have to handle symbolic links in `sys_chdir` function. Like problem 2, you need to avoid infinite
loops.
5. Consider only `absolute path`, you don't need to handle relative path.
6. You can pass problem 3 with modifying only: `sysfile.c` and `fs.c`.

## Grading

1. Implementation correctness (60%)
    - Passing all public testcases gets (60% * 70% = 42%)
    - you can execute ./grade-mp4-public to test your code.
    - Passing the remaining private testcases gets the rest (60% * 30% = 18%)
    - Each part accounts for 20%.
2. Report (20%)
    - Section including team members, team member contribution.
    - Section answering the questions/explanation of the trace code section.
    - Section explaining your implementation.
    - Write the report in "MP4_report_[GroupNumber].md".
3. Demo (20%)
    - Answer questions from TAs in 20 minutes.
4. Bonus (5%)
   - Refer to section "Rule for bonus" in [student guide](https://git.lsalab.cs.nthu.edu.tw/os25/os25_shared_xv6/src/branch/student/)
5. Plagiarism check
   - Never show your code to others.
   - If your code is found to be similar to others, including sources from the internet, and you cannot answer questions properly about your code during the demo, you will be considered as plagiarizing and will receive a score of 0 for the assignment.

## References

1. [xv6: a simple, Unix-like teaching operating system](https://pdos.csail.mit.edu/6.828/2022/xv6/book-riscv-rev3.pdf)
2. [MIT xv6 labs](https://pdos.csail.mit.edu/6.828/2022/reference.html)
3. [RISC-V Instruction Set Specifications](https://msyksphinz-self.github.io/riscv-isadoc/html/index.html)
4. Operating Systems Course in National Taiwan University
