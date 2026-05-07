/**
   Configuration header file.
  
   This file defines fundamental constants shared across all modules
   of the filesystem project. These constants establish basic parameters
   that must remain consistent throughout the entire system.
  
   Contents:
    - BLOCK_SIZE: fundamental unit of disk I/O operations
    - MAX_FILENAME: maximum length for file and directory names
  
   This configuration file is designed to be included by both low-level
   modules (disk emulator) and high-level modules (filesystem structures)
   without creating circular dependencies.
  
   Note: Changing these values requires careful consideration, as they
   affect the layout of on-disk structures and must be coordinated across
   the entire codebase.
 */

#pragma once

#include <stdio.h>

#define BLOCK_SIZE 512
#define MAX_FILENAME 58  // so that the dentry structure is exactly 64 bytes

// === DEBUG PRINTS ===
#ifdef DEBUG_MODE
    #define DEBUG_PRINT(fmt, ...) printf("[DEBUG] " fmt, ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(fmt, ...) do {} while (0)
#endif