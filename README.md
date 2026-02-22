# Operating System

This repository contains my course projects and study notes for the Operating Systems course at National Tsing Hua University (NTHU). It includes hands-on implementations based on the xv6 RISC-V kernel and concurrent programming with Pthreads.

## Course Modules
* **Introduction**: OS structures and System Calls.
* **Processes & Threads**: Process management, context switching, and multithreading.
* **CPU Scheduling**: Scheduling algorithms (RR, Priority, MFQS).
* **Process Synchronization**: Locks, Semaphores, and Monitors.
* **Deadlocks**: Characterization, Prevention, and Avoidance.
* **Memory Management**: Paging, Segmentation, and Address Translation.
* **Virtual Memory**: Demand Paging, Page Replacement, and Copy-on-Write.
* **Storage & File Systems**: Disk structures, I/O scheduling, and Inodes.

## Programming Assignments
* [os25-mp1](os25-mp1) Shell & System Calls
* [os25-mp2](os25-mp2) CPU Scheduling
* [os25-mp3](os25-mp3) Virtual Memory Management
* [os25-mp4](os25-mp4) File System
* [pthreads](pthreads) Concurrency Control

## Environment Requirements
* **Operating System**: Linux
* **Architecture**: RISC-V (for xv6 assignments).
* **Tools**:
    * `gcc-riscv64-unknown-elf`: Cross-compiler for xv6.
    * `qemu-system-riscv64`: Emulator for running the xv6 kernel.
    * `g++` & `pthread` library: For concurrent programming assignments.
    * `make`: Build automation tool.
