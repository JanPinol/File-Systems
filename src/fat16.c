#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "../include/fat16.h"

int file_found_flag = FALSE;
fat16_dir_entry file_found;

// Comprueba si la entrada actual es la última en el directorio.
static int _is_last_entry(FILE *fp, uint32_t sector, int idx, const fat16_boot_sector *bs);

// Calcula la posición del sector correspondiente a un clúster.
static uint32_t _calculate_sector(const fat16_dir_entry *entry, uint32_t root_dirs, const fat16_boot_sector *bs);

/**
 * Read the FAT16 boot sector from the filesystem image.
 *
 * @param filename Path to the FAT16 image file.
 * @param bs       Pointer to a fat16_boot_sector struct to populate.
 * @return TRUE (1) on success, FALSE (0) on failure.
 */
int read_fat16_boot_sector(const char *filename, fat16_boot_sector *bs) {   
    FILE *fp = fopen(filename, "rb");
    if (!fp) return FALSE;
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return FALSE; }
    if (fread(bs, sizeof(*bs), 1, fp) != 1) {
        printf(ERR_READING_BOOT_SECTOR);
        fclose(fp);
        return FALSE;
    }
    fclose(fp);
    return TRUE;
}

/**
 * Determine whether a given file contains a FAT16 filesystem.
 *
 * @param filename Path to the image file.
 * @return TRUE if the image is FAT16, FALSE otherwise.
 */
int is_fat16(const char *filename) {
    fat16_boot_sector bs;
    if (!read_fat16_boot_sector(filename, &bs)) return FALSE;
    if (bs.bytes_per_sector == 0) return FALSE;
    uint32_t root_dirs = ((bs.root_dir_entries * 32) + bs.bytes_per_sector - 1) / bs.bytes_per_sector;
    uint32_t fatsz = bs.sectors_per_fat ? bs.sectors_per_fat
                    : *((uint32_t *)(((uint8_t *)&bs) + 36));
    uint32_t totsec = bs.total_sectors_small ? bs.total_sectors_small : bs.total_sectors_long;
    uint32_t data_sec = totsec - (bs.reserved_sectors + bs.number_of_fats * fatsz + root_dirs);
    uint32_t count = data_sec / bs.sectors_per_cluster;
    return (count >= 4085 && count < 65525);
}

/**
 * Print the metadata of a FAT16 filesystem.
 *
 * @param filename Path to the FAT16 image file.
 */
void metadata_fat16(const char *filename) {
    fat16_boot_sector bs;
    if (!read_fat16_boot_sector(filename, &bs)) return;
    printf("\n------ Información del sistema FAT16 ------\n");
    printf("Sistema: FAT16\n");
    printf("Tamaño de sector: %u bytes\n", bs.bytes_per_sector);
    printf("Sectores por clúster: %u\n", bs.sectors_per_cluster);
    printf("Sectores reservados: %u\n", bs.reserved_sectors);
    printf("Número de FATs: %u\n", bs.number_of_fats);
    printf("Entradas raíz máximas: %u\n", bs.root_dir_entries);
    printf("Sectores por FAT: %u\n", bs.sectors_per_fat);
    printf("Etiqueta del volumen: %.11s\n\n", bs.volume_label);
}

/**
 * Recursively list the contents of a FAT16 directory cluster, printing
 * an ASCII-art tree. Can operate in listing or search mode.
 *
 * @param fp         Open FILE* of the FAT16 image.
 * @param bs         Pointer to the FAT16 boot sector.
 * @param sector     Sector number of the directory to scan.
 * @param root_dirs  Number of root directory sectors.
 * @param prefix     ASCII prefix to use for tree formatting.
 * @param find_file  If TRUE, search for 'target'; if FALSE, list all entries.
 * @param target     Filename to search for (when find_file is TRUE).
 */
static void tree_fat16_subdir(FILE *fp, const fat16_boot_sector *bs, uint32_t sector, uint32_t root_dirs, const char *prefix, int find_file, const char *target) {
    uint32_t entries = bs->bytes_per_sector / sizeof(fat16_dir_entry);

    for (uint32_t idx = 0; idx < entries; idx++) {
        fat16_dir_entry e;
        fseek(fp, sector * bs->bytes_per_sector + idx * sizeof(e), SEEK_SET);
        fread(&e, sizeof(e), 1, fp);

        // entrada vacía o borrada
        if (e.filename[0] == 0x00 || e.filename[0] == 0xE5) continue;
        // ignorar LFN (long file name) y etiquetas de volumen
        if ((e.attributes & 0x0F) == 0x0F || (e.attributes & ATTR_VOLUME_ID)) continue;
        // ignorar “.” y “..”
        if (e.filename[0] == '.') continue;


        // normalizar nombre 8.3 a string
        char name[13] = {0};
        int p = 0;
        for (int i = 0; i < 8 && e.filename[i] != ' '; i++) {
            name[p++] = tolower((unsigned char)e.filename[i]);
        }
        if (e.filename[8] != ' ') {
            name[p++] = '.';
            for (int i = 8; i < 11 && e.filename[i] != ' '; i++) {
                name[p++] = tolower((unsigned char)e.filename[i]);
            }
        }

        name[p] = '\0';

        // ¿es el último en este nivel?
        int last = _is_last_entry(fp, sector, idx, bs);

        if (find_file) { // ----- modo búsqueda -----
            // solo comparamos ficheros, no directorios
            if (!(e.attributes & ATTR_DIRECTORY) && strcmp(name, target) == 0) {
                file_found_flag  = TRUE;
                file_found       = e;
                return;  // ¡encontrado! salimos
            }
        } else { // ----- modo listado -----
            printf("%s%s%s\n",
            prefix,
            last ? "└── " : "├── ",
            name);
        }

        // recursar en subdirectorios (solo si no hemos encontrado el archivo)
        if ((e.attributes & ATTR_DIRECTORY) && !file_found_flag) {
            // construimos el nuevo prefix
            size_t L = strlen(prefix) + 4 + 1;
            char *new_prefix = malloc(L);
            strcpy(new_prefix, prefix);
            strcat(new_prefix, last ? "    " : "│   ");

            // sector hijo
            uint32_t child = _calculate_sector(&e, root_dirs, bs);
            tree_fat16_subdir(fp, bs, child, root_dirs, new_prefix, find_file, target);

            free(new_prefix);
        }

        // si estamos en búsqueda y ya encontramos, salimos del bucle
        if (find_file && file_found_flag) return;
    }
}

/**
 * Print the directory tree of a FAT16 filesystem, starting from root.
 *
 * @param file_system Path to the FAT16 image file.
 * @param find_file   If TRUE, search for 'file_name'; otherwise list everything.
 * @param file_name   Filename to search for (used when find_file is TRUE).
 */
void tree_fat16(const char *file_system, int find_file, const char *file_name) {
    fat16_boot_sector bs;
    if (!read_fat16_boot_sector(file_system, &bs)) return;

    FILE *fp = fopen(file_system, "rb");
    if (!fp) {
        fprintf(stderr, "Error opening '%s'\n", file_system);
        return;
    }

    uint32_t root_dirs = (bs.root_dir_entries * 32 + bs.bytes_per_sector - 1) / bs.bytes_per_sector;
    uint32_t first_root = bs.reserved_sectors + bs.number_of_fats * bs.sectors_per_fat;

    if (!find_file) printf(".\n");

    const char *empty = "";
    
    for (uint32_t i = 0; i < root_dirs; i++) {
        tree_fat16_subdir(fp, &bs, first_root + i, root_dirs, empty, find_file, file_name);
        if (find_file && file_found_flag) break;
    }

    fclose(fp);
}

/**
 * Comprueba si la entrada en la posición `idx` de un directorio es la última,
 * mirando a partir de `idx + 1` hasta el final del sector.
 *
 * @param fp        FILE* abierto de la imagen FAT16.
 * @param sector    Número de sector donde está el directorio.
 * @param idx       Índice de la entrada actual dentro del sector.
 * @param bs        Puntero al boot sector FAT16 para obtener bytes por sector.
 * @return          TRUE si no hay más entradas válidas tras `idx`, FALSE en caso contrario.
 */
static int _is_last_entry(FILE *fp, uint32_t sector, int idx, const fat16_boot_sector *bs) {
    uint32_t entries = bs->bytes_per_sector / sizeof(fat16_dir_entry);
    for (uint32_t k = idx + 1; k < entries; k++) {
        fat16_dir_entry e;
        fseek(fp, sector * bs->bytes_per_sector + k * sizeof(e), SEEK_SET);
        fread(&e, sizeof(e), 1, fp);
        if (e.filename[0] != 0x00 && e.filename[0] != 0xE5) return FALSE;
    }

    return TRUE;
}

/**
 * Calcula el número de sector de datos donde comienza el clúster
 * especificado en una entrada de directorio FAT16.
 *
 * @param entry     Puntero a la entrada de directorio con el primer clúster.
 * @param root_dirs Número de sectores reservados para el directorio raíz.
 * @param bs        Puntero al boot sector FAT16 para obtener parámetros.
 * @return          Sector de inicio de los datos del clúster.
 */
static uint32_t _calculate_sector(const fat16_dir_entry *e, uint32_t root_dirs, const fat16_boot_sector *bs) {
    uint32_t cluster = e->first_cluster_low;
    uint32_t data_start = bs->reserved_sectors + bs->number_of_fats * bs->sectors_per_fat
                          + root_dirs;
    return data_start + (cluster - 2) * bs->sectors_per_cluster;
}

/**
 * Imprime el contenido de un archivo almacenado en un sistema FAT16.
 * Lee clúster a clúster hasta que se haya mostrado todo el archivo.
 *
 * @param file_system Ruta al archivo de imagen FAT16.
 * @param file_name   Nombre del archivo dentro del sistema FAT16.
 */
void cat_fat16(const char *file_system, const char *file_name) {
    // Inicialitza l'estat de cerca
    file_found_flag = FALSE;
    tree_fat16(file_system, TRUE, file_name);

    // Comprovem si s'ha trobat el fitxer
    if (!file_found_flag) {
        fprintf(stderr, "Fitxer '%s' no trobat.\n", file_name);
        exit(EXIT_FAILURE);
    }

    // Obrim la imatge FAT16
    FILE *fp = fopen(file_system, "rb");
    if (!fp) {
        perror("No s'ha pogut obrir el sistema de fitxers");
        exit(EXIT_FAILURE);
    }

    // Carreguem el boot sector
    fat16_boot_sector bs;
    fread(&bs, sizeof(bs), 1, fp);

    // Calculem el sector inicial de dades
    uint32_t root_sectors = (bs.root_dir_entries * 32 + bs.bytes_per_sector - 1) / bs.bytes_per_sector;
    uint32_t data_base = bs.reserved_sectors + bs.number_of_fats * bs.sectors_per_fat + root_sectors;

    // Obtenim el primer clúster del fitxer
    uint32_t cluster = ((uint32_t)file_found.first_cluster_high << 16) | file_found.first_cluster_low;
    uint32_t first_sector = data_base + (cluster - 2) * bs.sectors_per_cluster;

    uint32_t remaining = file_found.file_size;

    // Bucle per llegir i imprimir el contingut
    while (remaining > 0) {
        uint32_t chunk = bs.bytes_per_sector;
        if (chunk > remaining) chunk = remaining;

        uint8_t block[chunk];
        fseek(fp, first_sector * bs.bytes_per_sector, SEEK_SET);
        fread(block, 1, chunk, fp);
        fwrite(block, 1, chunk, stdout);

        remaining -= chunk;
        first_sector++;
    }

    fclose(fp);
}