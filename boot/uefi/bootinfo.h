/*
 * Nix-Opus Boot Information
 * Passed from UEFI bootloader to kernel
 */

#ifndef _BOOTINFO_H
#define _BOOTINFO_H

/* Basic types - compatible with both bootloader and kernel */
#ifndef _NIXOPUS_TYPES_DEFINED
#define _NIXOPUS_TYPES_DEFINED
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
#endif

#define BOOTINFO_MAGIC 0x4E49584F  /* "NIXO" */

/* Memory map entry (simplified from UEFI) */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;      /* 1 = usable, 2 = reserved, 3 = ACPI, etc. */
    uint32_t reserved;
} memory_map_entry_t;

/* Memory types (simplified) */
#define MEMORY_USABLE           1
#define MEMORY_RESERVED         2
#define MEMORY_ACPI_RECLAIMABLE 3
#define MEMORY_ACPI_NVS         4
#define MEMORY_BAD              5
#define MEMORY_BOOTLOADER       6
#define MEMORY_KERNEL           7
#define MEMORY_FRAMEBUFFER      8

/* Framebuffer info */
typedef struct {
    uint64_t base;              /* Physical address of framebuffer */
    uint64_t size;              /* Size in bytes */
    uint32_t width;             /* Width in pixels */
    uint32_t height;            /* Height in pixels */
    uint32_t pitch;             /* Bytes per scanline */
    uint32_t bpp;               /* Bits per pixel */
    uint8_t  red_mask_size;
    uint8_t  red_mask_shift;
    uint8_t  green_mask_size;
    uint8_t  green_mask_shift;
    uint8_t  blue_mask_size;
    uint8_t  blue_mask_shift;
    uint8_t  reserved[2];
} framebuffer_info_t;

/* Boot information structure */
typedef struct {
    uint32_t magic;             /* BOOTINFO_MAGIC */
    uint32_t version;           /* Structure version */

    /* Framebuffer */
    framebuffer_info_t framebuffer;

    /* Memory map */
    uint32_t memory_map_entries;
    uint32_t reserved1;
    memory_map_entry_t *memory_map;

    /* ACPI tables */
    uint64_t acpi_rsdp;         /* ACPI RSDP address */

    /* Kernel info */
    uint64_t kernel_physical;   /* Where kernel was loaded */
    uint64_t kernel_size;       /* Kernel size in bytes */

} __attribute__((packed)) boot_info_t;

#endif /* _BOOTINFO_H */
