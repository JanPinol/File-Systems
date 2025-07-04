#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/util.h"

#define ERR_READING_BOOT_SECTOR "Error al leer el sector de arranque\n"
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20
#define ATTR_VOLUME_ID 0x08


/**
 * Estructura del sector de arranque de FAT16
 */
typedef struct __attribute__((packed)) {
    uint8_t  jmp[3];                    // Instrucción de salto al código de arranque
    char     oem[8];                    // Nombre del sistema que formateó el volumen

    uint16_t bytes_per_sector;          // Tamaño de cada sector en bytes
    uint8_t  sectors_per_cluster;       // Sectores por clúster
    uint16_t reserved_sectors;          // Sectores reservados antes de la FAT
    uint8_t  number_of_fats;            // Número de tablas FAT
    uint16_t root_dir_entries;          // Entradas máximas en el directorio raíz
    uint16_t total_sectors_small;       // Sectores totales si < 65536, si no 0
    uint8_t  media_descriptor;          // Descriptor de tipo de medio
    uint16_t sectors_per_fat;           // Sectores ocupados por cada FAT
    uint16_t sectors_per_track;         // Sectores por pista (no usado)
    uint16_t number_of_heads;           // Cabezas de lectura (no usado)
    uint32_t hidden_sectors;            // Sectores ocultos (no usado)
    uint32_t total_sectors_long;        // Sectores totales si total_sectors_small = 0

    uint8_t  drive_number;              // Número de unidad (no usado)
    uint8_t  current_head;              // Cabeza actual (no usado)
    uint8_t  boot_signature;            // Firma de arranque (no usado)
    uint32_t volume_id;                 // ID del volumen (no usado)
    char     volume_label[11];          // Etiqueta del volumen
    char     fs_type[8];                // Tipo de sistema de archivos (no usado)
} fat16_boot_sector;

/**
 * Entrada de directorio en FAT16
 */
typedef struct __attribute__((packed)) {
    uint8_t filename[11];               // Nombre y extensión (8.3)
    uint8_t attributes;                 // Atributos de archivo/directorio
    uint8_t reserved;                   // Reservado
    uint8_t creation_time_tenths;       // Decenas de segundo de creación
    uint16_t creation_time;             // Hora de creación
    uint16_t creation_date;             // Fecha de creación
    uint16_t last_access_date;          // Fecha de último acceso
    uint16_t first_cluster_high;        // Parte alta del número de clúster (no usado en FAT16)
    uint16_t last_write_time;           // Hora de última escritura
    uint16_t last_write_date;           // Fecha de última escritura
    uint16_t first_cluster_low;         // Parte baja del número de clúster inicial
    uint32_t file_size;                 // Tamaño del archivo en bytes
} fat16_dir_entry;

/**
 * Verifica si un archivo es un sistema de archivos FAT16
 * @param filename Ruta de la imagen o dispositivo
 * @return TRUE si es FAT16, FALSE en caso contrario
 */
int is_fat16(const char *filename);

/**
 * Muestra metadatos de un sistema FAT16
 * @param filename Ruta de la imagen o dispositivo
 */
void metadata_fat16(const char *filename);

/**
 * Muestra el contenido de un sistema FAT16 en formato de árbol
 * @param file_system Ruta de la imagen o dispositivo
 * @param find_file TRUE para buscar un archivo específico, FALSE para listar todo
 * @param file_name Nombre del archivo a buscar (solo si find_file = TRUE)
 */
void tree_fat16(const char *file_system, int find_file, const char *file_name);


/**
 * Muestra el contenido de un archivo en un sistema FAT16
 * @param file_system Ruta de la imagen o dispositivo
 * @param file_name Nombre del archivo a mostrar
 */
void cat_fat16(const char *file_system, const char *file_name);

#endif // FAT16_H