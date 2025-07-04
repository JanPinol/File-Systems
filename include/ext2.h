#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../include/util.h"


#define EXT2_SUPER_MAGIC    0xEF53      // Número mágico para ext2
#define BASE_OFFSET         1024        // Desplazamiento hasta el superbloque

#define ERR_READ_SUPERBLOCK "Error leyendo el superbloque"

// Define missing EXT2 constants
#ifndef EXT2_NDIR_BLOCKS
#define EXT2_NDIR_BLOCKS    12
#endif
#define EXT2_IND_BLOCK      12
#define EXT2_DIND_BLOCK     13
#define EXT2_TIND_BLOCK     14
#define EXT2_ROOT_INO       2

// File type constants
#define EXT2_FT_UNKNOWN     0
#define EXT2_FT_REG_FILE    1
#define EXT2_FT_DIR         2

/*
 * Estructura del sistema de archivos EXT2
 */
typedef struct __attribute__((packed)) {
    uint32_t s_inodes_count;          // Cantidad total de inodos
    uint32_t s_blocks_count;          // Cantidad total de bloques
    uint32_t s_r_blocks_count;        // Cantidad de bloques reservados
    uint32_t s_free_blocks_count;     // Cantidad de bloques libres
    uint32_t s_free_inodes_count;     // Cantidad de inodos libres
    uint32_t s_first_data_block;      // Primer bloque de datos
    uint32_t s_log_block_size;        // Tamaño de bloque (log2)
    uint32_t s_log_frag_size;         // Tamaño de fragmento (no usado)
    uint32_t s_blocks_per_group;      // Bloques por grupo
    uint32_t s_frags_per_group;       // Fragmentos por grupo (no usado)
    uint32_t s_inodes_per_group;      // Inodos por grupo
    uint32_t s_mtime;                 // Última vez montado
    uint32_t s_wtime;                 // Última vez escrito
    uint16_t s_mnt_count;             // Contador de montajes (no usado)
    uint16_t s_max_mnt_count;         // Montajes máximos (no usado)
    uint16_t s_magic;                 // Firma mágica
    uint16_t s_state;                 // Estado (no usado)
    uint16_t s_errors;                // Comportamiento ante errores (no usado)
    uint16_t s_minor_rev_level;       // Nivel de revisión menor (no usado)
    uint32_t s_lastcheck;             // Fecha de última comprobación
    uint32_t s_checkinterval;         // Intervalo entre comprobaciones (no usado)
    uint32_t s_creator_os;            // SO que creó (no usado)
    uint32_t s_rev_level;             // Nivel de revisión (no usado)
    uint16_t s_def_resuid;            // UID por defecto (no usado)
    uint16_t s_def_resgid;            // GID por defecto (no usado)

    uint32_t s_first_ino;             // Primer inodo no reservado
    uint16_t s_inode_size;            // Tamaño de la estructura inode
    uint16_t s_block_group_nr;        // Número de grupo de bloques (no usado)
    uint32_t s_feature_compat;        // Flags de características compatibles
    uint32_t s_feature_incompat;      // Flags de características incompatibles (no usado)
    uint32_t s_feature_ro_compat;     // Flags de solo lectura compatibles (no usado)
    uint8_t  s_uuid[16];              // UUID del volumen (no usado)
    char     s_volume_name[16];       // Nombre del volumen
    char     s_last_mounted[64];      // Punto de montaje anterior (no usado)
    uint32_t s_algo_bitmap;           // Mapa de algoritmos (no usado)

    uint8_t  s_prealloc_blocks;       // Bloques prefijados (no usado)
    uint8_t  s_prealloc_dir_blocks;   // Bloques de directorio prefijados (no usado)
    uint16_t s_padding1;              // Relleno (no usado)

    uint8_t  s_journal_uuid[16];      // UUID del diario (no usado)
    uint32_t s_journal_inum;          // Inodo del diario (no usado)
    uint32_t s_journal_dev;           // Dispositivo del diario (no usado)
    uint32_t s_last_orphan;           // Lista de huérfanos (no usado)

    uint32_t s_hash_seed[4];          // Semilla para hash (no usado)
    uint8_t  s_def_hash_version;      // Versión de hash (no usado)
    uint8_t  s_reserved_char_pad;     // Relleno (no usado)
    uint16_t s_reserved_word_pad;     // Relleno (no usado)
    uint32_t s_default_mount_options; // Opciones por defecto de montaje (no usado)
    uint32_t s_first_meta_bg;         // Primer grupo meta (no usado)
    uint32_t s_reserved[190];         // Relleno restante (no usado)
} ext2_superblock;

/*
 * Estructura de descriptor de grupo
 */
typedef struct __attribute__((packed)) {
    uint32_t unused[2];
    uint32_t bg_inode_table;       // Bloque de la tabla de inodos
} ext2_group_desc;

/*
 * Estructura de inodo
 */
typedef struct __attribute__((packed)) {
    uint16_t i_mode;        // Modo de archivo
    uint16_t i_uid;         // UID bajo (16 bits)
    uint32_t i_size;        // Tamaño en bytes
    uint32_t i_atime;       // Fecha de último acceso
    uint32_t i_ctime;       // Fecha de creación
    uint32_t i_mtime;       // Fecha de modificación
    uint32_t i_dtime;       // Fecha de eliminación
    uint16_t i_gid;         // GID bajo (16 bits)
    uint16_t i_links_count; // Número de enlaces duros
    uint32_t i_blocks;      // Número de bloques en sectores de disco
    uint32_t i_flags;       // Flags de archivo
    union {
        struct { uint32_t l_i_reserved1; } linux1;
        struct { uint32_t h_i_translator; } hurd1;
        struct { uint32_t m_i_reserved1; } masix1;
    } osd1;
    uint32_t i_block[15];   // Punteros a bloques
} ext2_inode;

/*
 * Entrada de directorio
 */
typedef struct __attribute__((packed)) {
    uint32_t inode;           // Número de inodo
    uint16_t rec_len;         // Longitud de la entrada
    uint8_t name_len;         // Longitud del nombre
    uint8_t file_type;        // Tipo de archivo
    char name[255];           // Nombre del archivo
} ext2_dir_entry;

/**
 * Función que verifica si un archivo es un sistema ext2
 * @param filename: ruta del archivo
 * @return 1 si es ext2, 0 en caso contrario
 */
int is_ext2(const char *filename);

/**
 * Función que muestra los metadatos de un sistema ext2
 * @param filename: ruta del archivo
 */
void metadata_ext2(const char *filename);

/**
 * Función que muestra en forma de árbol el contenido de un sistema ext2
 * @param filename: ruta del archivo
 */
void tree_ext2(const char *filename);

/**
 * Funcion para buscar el inodo por nombre o ruta y volcar sus bloques.
 * @param filename Ruta a la imagen EXT2.
 * @param target   Nombre (o ruta) de fichero a imprimir.
 */
void cat_ext2(const char *filename, const char *target);

#endif