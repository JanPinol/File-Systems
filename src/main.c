#include <stdio.h>
#include <string.h>

#include "../include/ext2.h"
#include "../include/fat16.h"

#define ERR_OPEN_FILE "Error opening the file\n"

/**
* Phase 1 of the project. META-DATA RETRIEVAL.
* This function retrieves the metadata of the file system.
* It checks the file system type and calls the appropriate function to retrieve the metadata.
* @param fileName The name of the file system image.
*/
void phase1(const char *fileName) {
    if (is_ext2(fileName)) metadata_ext2(fileName);
    else if (is_fat16(fileName)) metadata_fat16(fileName);
    else printf(ERR_OPEN_FILE);
}

/**
 * Phase 2 of the project. FILE SYSTEM TREE.
 * This function retrieves the file system tree.
 * It checks the file system type and calls the appropriate function to retrieve the tree.
 * @param fileName The name of the file system image.
 */
void phase2(const char *fileName) {
    if (is_ext2(fileName)) tree_ext2(fileName);
    else if (is_fat16(fileName)) tree_fat16(fileName, FALSE, "");
    else printf(ERR_OPEN_FILE);
}

/**
 * Phase 3 of the project. FILE CONTENTS RETRIEVAL.
 * This function retrieves the contents of a file.
 * It checks the file system type and calls the appropriate function to retrieve the contents.
 * @param fileName The name of the file system image.
 * @param file The name of the file to retrieve contents from.
 */
void phase3(const char *fileName, const char *file) {
    if (is_ext2(fileName)) printf("cat for ext2 still not implemented (phase 4)\n");
    else if (is_fat16(fileName)) cat_fat16(fileName, file);
    else printf(ERR_OPEN_FILE);
}

int main(int argc, char *argv[]) {
    // PHASE 1
    // ./fsutils --info <file system>

    // PHASE 2
    // ./fsutils --tree <file system>

    // PHASE 3
    // ./fsutils --cat <FAT16 file system> <file>

    // PHASE 4
    // ./fsutils --cat <EXT2 file system> <file>

    char *fullPath = malloc(strlen("res/") + strlen(argv[2]) + 1);
    strcpy(fullPath, "res/"); strcat(fullPath, argv[2]);

    if (argc == 3) {
        if (strcmp(argv[1], "--info") == 0) phase1(fullPath);
        else if (strcmp(argv[1], "--tree") == 0) phase2(fullPath);
        else printf("Error arguments\n");
    } else if (argc == 4) {
        if (strcmp(argv[1], "--cat") == 0) {
            if (is_ext2(fullPath)) {
                cat_ext2(fullPath, argv[3]);
            }
            else if (is_fat16(fullPath)) {
                cat_fat16(fullPath, argv[3]);
            }
            else {
                printf(ERR_OPEN_FILE);
            }
        }
        else {
            printf("Error arguments\n");
        }
    } else {
        printf("Error arguments\n");
    }

    free(fullPath);
  return 0;
}
