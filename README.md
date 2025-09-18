/********************************************
 *                                          *
 *    Linux Memory Scanner Kernel Module    *
 *    Author: Themistoklis Georgiadis       *
 *                                          *
 ********************************************/

This kernel module attaches to a target process (by name or PID) and
scans the process address space for specified values. A user-space
controller (user.c) is provided to simplify interaction with the module.

Features:
 - Attach by PID or process name
 - Scan memory regions for given patterns/values
 - User-mode program for easy module interaction