#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "ext2.h"

// Global variables for superblock and block size
ext2_superblock sb;
uint32_t block_size;

static int file_found_flag   = FALSE;
static uint32_t file_found_inode  = 0;

// Forward declarations
static uint32_t scan_dir_block(FILE *fp, uint32_t block, const char *name);
static uint32_t scan_indirect_blocks(FILE *fp, uint32_t block, int level, const char *name);
static uint32_t find_inode_in_dir(FILE *fp, ext2_inode *inode, const char *name);
static uint32_t find_inode_by_path(FILE *fp, const char *path);
static void search_dir(FILE *fp, ext2_inode *inode, const char *target);
static void search_dir_block(FILE *fp, uint32_t block, const char *target);
static void search_indirect(FILE *fp, uint32_t block, int level, const char *target);
static void read_dir(FILE *fp, ext2_inode *inode, int depth);
static void tree_ext2_subdir(FILE *fp, ext2_inode *inode, const char *prefix);

/**
 * Read the EXT2 superblock from the filesystem image.
 *
 * @param filename Path to the image file containing the EXT2 filesystem.
 * @param sbo      Pointer to an ext2_superblock structure to fill.
 * @return TRUE (1) if the superblock was read successfully, FALSE (0) on error.
 */
int read_ext2_superblock(const char *filename, ext2_superblock *sbo) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return FALSE;
    if (fseek(fp, BASE_OFFSET, SEEK_SET) != 0) { fclose(fp); return FALSE; }
    if (fread(sbo, sizeof(ext2_superblock), 1, fp) != 1) {
        printf(ERR_READ_SUPERBLOCK);
        fclose(fp);
        return FALSE;
    }
    fclose(fp);
    return TRUE;
}

/**
* Function that checks if the file is an ext2 filesystem
* @param filename: the name of the file
* @return 1 if the file is an ext2 filesystem, 0 otherwise
*/
int is_ext2(const char *filename) {
    ext2_superblock tmp;
    if (!read_ext2_superblock(filename, &tmp)) return FALSE;
    return (tmp.s_magic == EXT2_SUPER_MAGIC);
}

/**
* Function that prints the metadata of an ext2 filesystem
* @param filename: the name of the file
*/
void metadata_ext2(const char *filename) {
    ext2_superblock sb;

    if (!read_ext2_superblock(filename, &sb)) return;

    printf("\n------ Filesystem Information ------\n");
    printf("\nFilesystem: EXT2\n");

    printf("\nINODE INFO\n");
    printf("  Size.............: %d\n", sb.s_inode_size);
    printf("  Num Inodes.......: %d\n", sb.s_inodes_count);
    printf("  First Inode......: %d\n", sb.s_first_ino);
    printf("  Inodes per Group.: %d\n", sb.s_inodes_per_group);
    printf("  Free Inodes......: %d\n", sb.s_free_inodes_count);

    printf("\nBLOCK INFO\n");
    printf("  Block Size.......: %d\n", 1024 << sb.s_log_block_size);
    printf("  Reserved Blocks..: %d\n", sb.s_r_blocks_count);
    printf("  Free Blocks......: %d\n", sb.s_free_blocks_count);
    printf("  Total Blocks.....: %d\n", sb.s_blocks_count);
    printf("  First Block......: %d\n", sb.s_first_data_block);
    printf("  Blocks per Group.: %d\n", sb.s_blocks_per_group);
    printf("  Group Flags......: %d\n", sb.s_feature_compat);

    printf("\nVOLUME INFO\n");
    printf("  Volume Name......: %s\n", sb.s_volume_name);
    printf("  Last Checked.....: %s\n", format_time(sb.s_lastcheck));
    printf("  Last Mounted.....: %s\n", format_time(sb.s_mtime));
    printf("  Last Written.....: %s\n\n", format_time(sb.s_wtime));
}

/**
 * Read group descriptor for given block group.
 *
 * @param fp          Puntero al fichero de imagen EXT2.
 * @param block_group Índice del grupo de bloques.
 * @param group       Salida donde se almacenará el descriptor leído.
 * @return 0 si tiene éxito, -1 en caso de error.
 */
int read_group_desc_ext2(FILE *fp, uint16_t block_group, ext2_group_desc *group) {
    if (!fp || !group) return -1;
    uint32_t table_block = sb.s_first_data_block + 1;
    uint64_t offset = (uint64_t)table_block * block_size
                        + (uint64_t)block_group * sizeof(ext2_group_desc);
    if (fseek(fp, offset, SEEK_SET) != 0) return -1;
    if (fread(group, sizeof(*group), 1, fp) != 1) return -1;
    return 0;
}

/**
 * Read an inode by its number.
 *
 * @param fp         Puntero al fichero de imagen EXT2.
 * @param inode_num  Número de inodo a leer (comienza en 1).
 * @param inode      Salida donde se almacenará la información del inodo.
 * @return 0 si tiene éxito, -1 en caso de error.
 */
int read_inode_ext2(FILE *fp, uint32_t inode_num, ext2_inode *inode) {
    if (!fp || inode_num < 1 || !inode) return -1;
    uint32_t ing = sb.s_inodes_per_group;
    uint32_t gi = (inode_num - 1) / ing;
    uint32_t li = (inode_num - 1) % ing;
    ext2_group_desc gd;
    if (read_group_desc_ext2(fp, gi, &gd) != 0) return -1;
    uint64_t off = (uint64_t)gd.bg_inode_table * block_size
                    + (uint64_t)li * sb.s_inode_size;
    if (fseek(fp, off, SEEK_SET) != 0) return -1;
    if (fread(inode, sizeof(*inode), 1, fp) != 1) return -1;
    return 0;
}

/**
 * Traverse and print entries in a single directory block (skipping “.” and “..”).
 *
 * @param fp        Puntero al fichero de imagen EXT2.
 * @param block_num Número de bloque de datos que contiene entradas de directorio.
 * @param depth     Nivel de anidamiento para dibujar el prefijo ASCII.
 */
static void traverse_dir_block(FILE *fp, uint32_t block_num, int depth) {
    if (block_num == 0) return;

    /* Sitúate al principio del bloque de datos */
    if (fseek(fp, (uint64_t)block_num * block_size, SEEK_SET) != 0)
        return;

    uint8_t *buf = malloc(block_size);
    if (!buf) return;
    if (fread(buf, block_size, 1, fp) != 1) {
        free(buf);
        return;
    }

    uint32_t off = 0;
    while (off < block_size) {
        ext2_dir_entry *e = (ext2_dir_entry *)(buf + off);
        if (e->rec_len == 0 || off + e->rec_len > block_size) break;

        /* Copiamos nombre y lo NUL-terminamos */
        char name[256] = {0};
        int len = e->name_len < 255 ? e->name_len : 255;
        memcpy(name, e->name, len);
        name[len] = '\0';

        /* Saltamos entradas inválidas o "." / ".." */
        if (e->inode != 0 && strcmp(name, ".") && strcmp(name, "..")) {
            /* Prefijo de árbol */
            for (int i = 0; i < depth - 1; i++)
                printf("│   ");

            int is_last = (off + e->rec_len >= block_size);
            printf(is_last ? "└── %s\n" : "├── %s\n", name);

            /* Detectar si es directorio */
            int is_dir = (e->file_type == EXT2_FT_DIR);
            if (!is_dir) {
                /* Fallback: leer inode y comprobar modo */
                ext2_inode tmp;
                if (read_inode_ext2(fp, e->inode, &tmp) == 0) {
                    if (S_ISDIR(tmp.i_mode))
                        is_dir = 1;
                }
            }

            if (is_dir) {
                ext2_inode sub;
                if (read_inode_ext2(fp, e->inode, &sub) == 0) {
                    read_dir(fp, &sub, depth + 1);
                }
            }
        }

        off += e->rec_len;
    }

    free(buf);
}

/**
 * Read and print all directory entries for the given inode.
 * Recorre bloques directos e indirectos y llama a traverse_dir_block.
 *
 * @param fp    Puntero al fichero de imagen EXT2.
 * @param inode Inodo de directorio cuyas entradas se listarán.
 * @param depth Nivel de anidamiento para los prefijos ASCII.
 */
static void read_dir(FILE *fp, ext2_inode *inode, int depth) {
    uint32_t ptrs = block_size / sizeof(uint32_t);
    uint32_t *ib = NULL;

    // Direct blocks
    for (int i = 0; i < EXT2_NDIR_BLOCKS; i++) {
        if (!inode->i_block[i]) break;
        traverse_dir_block(fp, inode->i_block[i], depth);
    }

    // Single, double, triple indirect blocks
    for (int lvl = 1; lvl <= 3; lvl++) {
        int idx = (lvl == 1 ? EXT2_IND_BLOCK : lvl == 2 ? EXT2_DIND_BLOCK : EXT2_TIND_BLOCK);
        if (!inode->i_block[idx]) continue;
        ib = malloc(block_size);
        if (!ib) continue;
        if (fseek(fp, (uint64_t)inode->i_block[idx] * block_size, SEEK_SET) == 0
            && fread(ib, block_size, 1, fp) == 1) {
            for (uint32_t j = 0; j < ptrs; j++) {
                if (ib[j]) traverse_dir_block(fp, ib[j], depth);
            }
        }
        free(ib);
    } 
}

/**
 * Main entry point for “--tree” on an EXT2 image.
 * Carga el superbloque, el inodo raíz y arranca la impresión en forma de árbol.
 *
 * @param filename Ruta al archivo de imagen EXT2.
 */
void tree_ext2(const char *filename) {
    if (!read_ext2_superblock(filename, &sb)) return;
    block_size = 1024 << sb.s_log_block_size;

    FILE *fp = fopen(filename, "rb");
    if (!fp) { perror("fopen"); return; }

    ext2_inode root;
    if (read_inode_ext2(fp, EXT2_ROOT_INO, &root) != 0) {
        fclose(fp);
        return;
    }

    printf(".\n");
    tree_ext2_subdir(fp, &root, "");
    fclose(fp);
}

/**
 * Internal recursive helper for tree_ext2.
 * Recorre un inodo de directorio y sus subdirectorios imprimiendo con el prefijo dado.
 *
 * @param fp     Puntero al fichero de imagen EXT2.
 * @param inode  Inodo de directorio actual.
 * @param prefix Prefijo ASCII-art para este nivel (p.ej. "│   " o "    ").
 */
static void tree_ext2_subdir(FILE *fp, ext2_inode *inode, const char *prefix) {
    // Direct blocks
    for (int i = 0; i < EXT2_NDIR_BLOCKS; i++) {
        uint32_t blk = inode->i_block[i];
        if (!blk) continue;

        uint8_t *buf = malloc(block_size);
        if (!buf) return;
        if (fseek(fp, (uint64_t)blk * block_size, SEEK_SET) != 0 ||
            fread(buf, block_size, 1, fp) != 1) {
            free(buf);
            return;
        }

        uint32_t off = 0;
        while (off < block_size) {
            ext2_dir_entry *e = (ext2_dir_entry *)(buf + off);
            if (e->rec_len == 0 || off + e->rec_len > block_size) break;

            // Nombre NUL-terminado
            char name[256] = {0};
            int len = e->name_len < 255 ? e->name_len : 255;
            memcpy(name, e->name, len);
            name[len] = '\0';

            // Saltar ".", ".." y entradas inválidas
            if (e->inode != 0 &&
                strcmp(name, ".")   != 0 &&
                strcmp(name, "..")  != 0)
            {
                int is_last = (off + e->rec_len >= block_size);
                printf("%s%s%s\n",
                       prefix,
                       is_last ? "└── " : "├── ",
                       name);

                // Detectar directorio (vía file_type o fallback S_ISDIR)
                int is_dir = (e->file_type == EXT2_FT_DIR);
                if (!is_dir) {
                    ext2_inode tmp;
                    if (read_inode_ext2(fp, e->inode, &tmp) == 0 &&
                        S_ISDIR(tmp.i_mode))
                    {
                        is_dir = 1;
                    }
                }

                if (is_dir) {
                    // Nuevo prefix para nivel inferior
                    size_t L = strlen(prefix) + 4 + 1;
                    char *p2 = malloc(L);
                    strcpy(p2, prefix);
                    strcat(p2, is_last ? "    " : "│   ");

                    ext2_inode sub;
                    if (read_inode_ext2(fp, e->inode, &sub) == 0) {
                        tree_ext2_subdir(fp, &sub, p2);
                    }
                    free(p2);
                }
            }

            off += e->rec_len;
        }

        free(buf);
    }

    // Single / double / triple indirect blocks
    uint32_t ptrs = block_size / sizeof(uint32_t);
    for (int lvl = 1; lvl <= 3; lvl++) {
        int idx = (lvl == 1 ? EXT2_IND_BLOCK :
                   lvl == 2 ? EXT2_DIND_BLOCK : EXT2_TIND_BLOCK);
        uint32_t iblk = inode->i_block[idx];
        if (!iblk) continue;

        uint32_t *ind = malloc(block_size);
        if (!ind) return;
        if (fseek(fp, (uint64_t)iblk * block_size, SEEK_SET) == 0 &&
            fread(ind, block_size, 1, fp) == 1)
        {
            for (uint32_t j = 0; j < ptrs; j++) {
                if (!ind[j]) continue;

                uint8_t *buf = malloc(block_size);
                if (!buf) continue;
                if (fseek(fp, (uint64_t)ind[j] * block_size, SEEK_SET)==0 &&
                    fread(buf, block_size, 1, fp)==1)
                {
                    uint32_t off = 0;
                    while (off < block_size) {
                        ext2_dir_entry *e = (ext2_dir_entry*)(buf + off);
                        if (e->rec_len == 0 || off + e->rec_len > block_size) break;

                        char name[256] = {0};
                        int len = e->name_len < 255 ? e->name_len : 255;
                        memcpy(name, e->name, len);
                        name[len] = '\0';

                        if (e->inode!=0 &&
                            strcmp(name,".") &&
                            strcmp(name,".."))
                        {
                            int is_last = (off + e->rec_len >= block_size);
                            printf("%s%s%s\n",
                                   prefix,
                                   is_last ? "└── " : "├── ",
                                   name);

                            int is_dir = (e->file_type == EXT2_FT_DIR);
                            if (!is_dir) {
                                ext2_inode tmp;
                                if (read_inode_ext2(fp, e->inode, &tmp)==0 &&
                                    S_ISDIR(tmp.i_mode))
                                    is_dir = 1;
                            }
                            if (is_dir) {
                                size_t L = strlen(prefix)+4+1;
                                char *p2 = malloc(L);
                                strcpy(p2,prefix);
                                strcat(p2, is_last ? "    " : "│   ");
                                ext2_inode sub;
                                if (read_inode_ext2(fp,e->inode,&sub)==0)
                                    tree_ext2_subdir(fp,&sub,p2);
                                free(p2);
                            }
                        }
                        off += e->rec_len;
                    }
                }
                free(buf);
            }
        }
        free(ind);
    }
}

/**
 * Escanea un bloque de directorio buscando una entrada con nombre dado.
 * @param fp       Puntero al fichero de imagen EXT2.
 * @param block    Número de bloque a leer.
 * @param name     Nombre de la entrada a buscar.
 * @return Número de inodo si se encuentra, 0 en caso contrario.
 */
static uint32_t scan_dir_block(FILE *fp, uint32_t block, const char *name) {
    uint8_t *buf = malloc(block_size);
    if (!buf) return 0;
    if (fseek(fp, block * block_size, SEEK_SET) ||
        fread(buf, block_size, 1, fp)!=1) {
        free(buf);
        return 0;
    }
    uint32_t off = 0;
    while (off < block_size) {
        ext2_dir_entry *e = (ext2_dir_entry*)(buf + off);
        if (e->rec_len==0) break;
        char en[256]={0};
        int len = e->name_len<255?e->name_len:255;
        memcpy(en, e->name, len);
        en[len]='\0';
        if (e->inode && strcmp(en,name)==0) {
            uint32_t in = e->inode;
            free(buf);
            return in;
        }
        off += e->rec_len;
    }
    free(buf);
    return 0;
}

/**
 * Escanea recursivamente bloques indirectos de un inodo como si fuesen bloques de directorio.
 * @param fp       Puntero al fichero de imagen EXT2.
 * @param block    Bloque indirecto a procesar.
 * @param level    1=single, 2=double, 3=triple indirect.
 * @param name     Nombre de la entrada buscada.
 * @return Número de inodo si se encuentra, 0 en caso contrario.
 */
static uint32_t scan_indirect_blocks(FILE *fp, uint32_t block, int level, const char *name)
{
    if (!block||level<1) return 0;
    uint32_t ptrs = block_size/sizeof(uint32_t);
    uint32_t *ib=malloc(block_size);
    if (!ib) return 0;
    if (fseek(fp, block*block_size, SEEK_SET)||
        fread(ib, block_size,1,fp)!=1){
        free(ib);
        return 0;
    }
    for(uint32_t i=0;i<ptrs;i++){
        if (!ib[i]) continue;
        if (level==1){
            uint32_t found = scan_dir_block(fp, ib[i], name);
            if (found){ free(ib); return found; }
        } else {
            uint32_t found = scan_indirect_blocks(fp, ib[i], level-1, name);
            if (found){ free(ib); return found; }
        }
    }
    free(ib);
    return 0;
}

/**
 * Busca en un único inodo de directorio (directos + indirectos) la entrada con nombre dado.
 * @param fp     Puntero al fichero de imagen EXT2.
 * @param inode  Puntero al inodo de directorio.
 * @param name   Nombre de la entrada a buscar.
 * @return Número de inodo encontrado, o 0 si no existe.
 */
static uint32_t find_inode_in_dir(FILE *fp, ext2_inode *inode, const char *name) {
    // direct blocks
    for(int i=0;i<EXT2_NDIR_BLOCKS;i++){
        uint32_t blk = inode->i_block[i];
        if (!blk) continue;
        uint32_t f = scan_dir_block(fp, blk, name);
        if (f) return f;
    }
    // indirect levels
    int idxs[3] = { EXT2_IND_BLOCK,
                    EXT2_DIND_BLOCK,
                    EXT2_TIND_BLOCK };
    for(int lvl=0;lvl<3;lvl++){
        uint32_t ib = inode->i_block[ idxs[lvl] ];
        if (!ib) continue;
        uint32_t f = scan_indirect_blocks(fp, ib, lvl+1, name);
        if (f) return f;
    }
    return 0;
}

/**
 * @brief Resuelve una ruta de la raíz (p.ej., "dir1/dir2/file") a su número de inodo.
 * @param fp     Puntero al fichero de imagen EXT2.
 * @param path   Ruta dentro del sistema de ficheros.
 * @return Número de inodo si existe y es fichero regular, 0 en caso contrario.
 */
static uint32_t find_inode_by_path(FILE *fp, const char *path){
    uint32_t ino = EXT2_ROOT_INO;
    ext2_inode node;
    if (read_inode_ext2(fp, ino, &node)<0) return 0;
    char *p = strdup(path), *tok = strtok(p,"/");
    while(tok){
        uint32_t nxt = find_inode_in_dir(fp, &node, tok);
        if (!nxt){ free(p); return 0; }
        ino = nxt;
        if (read_inode_ext2(fp, ino, &node)<0){ free(p); return 0; }
        tok = strtok(NULL,"/");
    }
    free(p);
    return S_ISREG(node.i_mode) ? ino : 0;
}

/**
 * Recorre un inodo de directorio completo (directos + indirectos + subdirectorios)
 * buscando una entrada target. Marca file_found_flag y file_found_inode si la halla.
 * @param fp     Puntero al fichero de imagen EXT2.
 * @param inode  Puntero al inodo de directorio raíz de la búsqueda.
 * @param target Nombre de fichero a localizar.
 */
static void search_dir_block(FILE *fp, uint32_t block, const char *t){
    uint8_t *buf=malloc(block_size);
    if(!buf) return;
    if(fseek(fp,block*block_size,SEEK_SET)|| fread(buf,block_size,1,fp)!=1){
        free(buf); return;
    }
    uint32_t off=0;
    while(off<block_size && !file_found_flag){
        ext2_dir_entry *e = (ext2_dir_entry*)(buf+off);
        if(e->rec_len==0) break;
        char nm[256]={0};
        int ln = e->name_len<255?e->name_len:255;
        memcpy(nm,e->name,ln); nm[ln]='\0';
        if(e->inode && strcmp(nm,t)==0){
            file_found_flag  = TRUE;
            file_found_inode = e->inode;
            break;
        }
        off += e->rec_len;
    }
    free(buf);
}

/**
 * Escanea bloques indirectos buscando en cada bloque de directorio el fichero target.
 * @param fp       Puntero al fichero de imagen EXT2.
 * @param block    Bloque indirecto a procesar.
 * @param level    Nivel de indirección (1, 2 o 3).
 * @param target   Nombre de fichero a localizar.
 */
static void search_indirect(FILE *fp, uint32_t block, int lvl, const char *t){
    if(!block||lvl<1) return;
    uint32_t ptrs = block_size/sizeof(uint32_t);
    uint32_t *ib=malloc(block_size);
    if(!ib) return;
    if(fseek(fp,block*block_size,SEEK_SET)|| fread(ib,block_size,1,fp)!=1){
        free(ib); return;
    }
    for(uint32_t i=0;i<ptrs&&!file_found_flag;i++){
        if(!ib[i]) continue;
        if(lvl==1) search_dir_block(fp, ib[i], t);
        else      search_indirect(fp, ib[i], lvl-1, t);
    }
    free(ib);
}

/**
 * Recorre un inodo de directorio completo (directos + indirectos + subdirectorios)
 * buscando una entrada target. Marca file_found_flag y file_found_inode si la halla.
 * @param fp     Puntero al fichero de imagen EXT2.
 * @param inode  Puntero al inodo de directorio raíz de la búsqueda.
 * @param target Nombre de fichero a localizar.
 */
static void search_dir(FILE *fp, ext2_inode *node, const char *t){
    // direct
    for(int i=0;i<EXT2_NDIR_BLOCKS&&!file_found_flag;i++){
        if(node->i_block[i]) search_dir_block(fp, node->i_block[i], t);
    }
    // indirect
    int idxs[3]={EXT2_IND_BLOCK,EXT2_DIND_BLOCK,EXT2_TIND_BLOCK};
    for(int l=0;l<3&&!file_found_flag;l++){
        if(node->i_block[idxs[l]])
            search_indirect(fp,node->i_block[idxs[l]],l+1,t);
    }
    // recurse subdirs (solo direct para simplicidad)
    for(int i=0;i<EXT2_NDIR_BLOCKS&&!file_found_flag;i++){
        uint32_t blk=node->i_block[i];
        if(!blk) continue;
        uint8_t *buf=malloc(block_size);
        if(fseek(fp,blk*block_size,SEEK_SET)|| fread(buf,block_size,1,fp)!=1){
            free(buf); continue;
        }
        uint32_t off=0;
        while(off<block_size&&!file_found_flag){
            ext2_dir_entry*e=(ext2_dir_entry*)(buf+off);
            if(e->rec_len==0) break;
            if(e->inode==0 || e->file_type!=EXT2_FT_DIR){
                off+=e->rec_len; continue;
            }
            char nm2[256]={0};
            int ln2=e->name_len<255?e->name_len:255;
            memcpy(nm2,e->name,ln2); nm2[ln2]='\0';
            if(strcmp(nm2,".")&&strcmp(nm2,"..")){
                ext2_inode sub;
                if(read_inode_ext2(fp,e->inode,&sub)==0)
                    search_dir(fp,&sub,t);
            }
            off += e->rec_len;
        }
        free(buf);
    }
}

/**
 * Implementa “cat” en EXT2: busca el inodo por nombre o ruta y vuelca sus bloques.
 * @param filename Ruta a la imagen EXT2.
 * @param target   Nombre (o ruta) de fichero a imprimir.
 */
void cat_ext2(const char *filename, const char *target) {
    if (!read_ext2_superblock(filename, &sb)) return;
    block_size = 1024 << sb.s_log_block_size;

    FILE *fp = fopen(filename, "rb");
    if (!fp) { perror("fopen"); return; }

    uint32_t ino = 0;
    if (strchr(target, '/')) {
        ino = find_inode_by_path(fp, target);
    } else {
        file_found_flag = FALSE;
        file_found_inode = 0;
        ext2_inode root;
        if (read_inode_ext2(fp, EXT2_ROOT_INO, &root) == 0)
            search_dir(fp, &root, target);
        if (file_found_flag) ino = file_found_inode;
    }

    if (!ino) {
        fprintf(stderr, "EXT2: file '%s' not found\n", target);
        fclose(fp);
        return;
    }

    ext2_inode inode;
    if (read_inode_ext2(fp, ino, &inode) < 0) {
        fprintf(stderr, "EXT2: error reading inode %u\n", ino);
        fclose(fp);
        return;
    }

    uint32_t rem = inode.i_size;
    for (int i = 0; i < EXT2_NDIR_BLOCKS && rem > 0; i++) {
        uint32_t blk = inode.i_block[i];
        if (!blk) break;
        uint32_t toread = rem < block_size ? rem : block_size;
        uint8_t *buf = malloc(toread);
        if (!buf) break;
        fseek(fp, blk * block_size, SEEK_SET);
        fread(buf, 1, toread, fp);
        fwrite(buf, 1, toread, stdout);
        free(buf);
        rem -= toread;
    }

    fclose(fp);
}