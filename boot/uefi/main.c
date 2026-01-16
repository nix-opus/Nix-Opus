/*
 * Nix-Opus UEFI Bootloader
 * Loads the 64-bit kernel and passes control to it
 */

#include "uefi.h"
#include "bootinfo.h"

/* Global system table */
static EFI_SYSTEM_TABLE *gST;
static EFI_BOOT_SERVICES *gBS;
static EFI_HANDLE gImageHandle;

/* Boot info to pass to kernel - allocated in safe memory */
static boot_info_t *boot_info;
static memory_map_entry_t *memory_entries;

/* Kernel entry point type */
typedef void (*kernel_entry_t)(boot_info_t *info);

/* Print a string to console */
static void print(CHAR16 *str) {
    gST->ConOut->OutputString(gST->ConOut, str);
}

/* Print a hex number */
static void print_hex(UINT64 val) {
    CHAR16 buf[17];
    CHAR16 *hex = u"0123456789ABCDEF";
    int i;

    for (i = 15; i >= 0; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    buf[16] = 0;
    print(buf);
}

/* Compare GUIDs */
static int guid_eq(EFI_GUID *a, EFI_GUID *b) {
    UINT64 *pa = (UINT64*)a;
    UINT64 *pb = (UINT64*)b;
    return pa[0] == pb[0] && pa[1] == pb[1];
}

/* Memory copy */
static void memcpy(void *dst, const void *src, UINTN size) {
    UINT8 *d = dst;
    const UINT8 *s = src;
    while (size--) *d++ = *s++;
}

/* Memory set */
static void memset(void *dst, UINT8 val, UINTN size) {
    UINT8 *d = dst;
    while (size--) *d++ = val;
}

/*
 * Initialize Graphics Output Protocol (GOP)
 */
static EFI_STATUS init_gop(void) {
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_STATUS status;

    print(u"Initializing graphics...\r\n");

    status = gBS->LocateProtocol(&gop_guid, NULL, (void**)&gop);
    if (status != EFI_SUCCESS) {
        print(u"ERROR: Could not locate GOP\r\n");
        return status;
    }

    /* Find best mode (highest resolution) */
    UINT32 best_mode = gop->Mode->Mode;
    UINT32 best_width = 0;
    UINT32 best_height = 0;

    for (UINT32 i = 0; i < gop->Mode->MaxMode; i++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        UINTN size;

        status = gop->QueryMode(gop, i, &size, &info);
        if (status != EFI_SUCCESS) continue;

        /* Prefer modes with direct color (not palette) */
        if (info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor ||
            info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {

            if (info->HorizontalResolution >= best_width &&
                info->VerticalResolution >= best_height) {

                /* Cap at 1920x1080 for reasonable performance */
                if (info->HorizontalResolution <= 1920 &&
                    info->VerticalResolution <= 1080) {
                    best_mode = i;
                    best_width = info->HorizontalResolution;
                    best_height = info->VerticalResolution;
                }
            }
        }
    }

    /* Set the mode */
    if (best_mode != gop->Mode->Mode) {
        status = gop->SetMode(gop, best_mode);
        if (status != EFI_SUCCESS) {
            print(u"WARNING: Could not set graphics mode\r\n");
        }
    }

    /* Fill in framebuffer info */
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info = gop->Mode->Info;

    boot_info->framebuffer.base = gop->Mode->FrameBufferBase;
    boot_info->framebuffer.size = gop->Mode->FrameBufferSize;
    boot_info->framebuffer.width = mode_info->HorizontalResolution;
    boot_info->framebuffer.height = mode_info->VerticalResolution;
    boot_info->framebuffer.pitch = mode_info->PixelsPerScanLine * 4;
    boot_info->framebuffer.bpp = 32;

    if (mode_info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
        /* BGRA */
        boot_info->framebuffer.blue_mask_shift = 0;
        boot_info->framebuffer.blue_mask_size = 8;
        boot_info->framebuffer.green_mask_shift = 8;
        boot_info->framebuffer.green_mask_size = 8;
        boot_info->framebuffer.red_mask_shift = 16;
        boot_info->framebuffer.red_mask_size = 8;
    } else {
        /* RGBA */
        boot_info->framebuffer.red_mask_shift = 0;
        boot_info->framebuffer.red_mask_size = 8;
        boot_info->framebuffer.green_mask_shift = 8;
        boot_info->framebuffer.green_mask_size = 8;
        boot_info->framebuffer.blue_mask_shift = 16;
        boot_info->framebuffer.blue_mask_size = 8;
    }

    print(u"Graphics: ");
    print_hex(boot_info->framebuffer.width);
    print(u"x");
    print_hex(boot_info->framebuffer.height);
    print(u" @ 0x");
    print_hex(boot_info->framebuffer.base);
    print(u"\r\n");

    return EFI_SUCCESS;
}

/*
 * Load kernel from disk
 */
static EFI_STATUS load_kernel(EFI_PHYSICAL_ADDRESS *kernel_addr, UINTN *kernel_size) {
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_GUID file_info_guid = EFI_FILE_INFO_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_FILE_PROTOCOL *root, *kernel_file;
    EFI_STATUS status;

    print(u"Loading kernel...\r\n");

    /* Get filesystem protocol */
    status = gBS->LocateProtocol(&fs_guid, NULL, (void**)&fs);
    if (status != EFI_SUCCESS) {
        print(u"ERROR: Could not locate filesystem\r\n");
        return status;
    }

    /* Open root directory */
    status = fs->OpenVolume(fs, &root);
    if (status != EFI_SUCCESS) {
        print(u"ERROR: Could not open volume\r\n");
        return status;
    }

    /* Open kernel file */
    status = root->Open(root, &kernel_file, u"\\EFI\\NIXOPUS\\KERNEL.BIN",
                        EFI_FILE_MODE_READ, 0);
    if (status != EFI_SUCCESS) {
        /* Try alternate path */
        status = root->Open(root, &kernel_file, u"\\KERNEL.BIN",
                           EFI_FILE_MODE_READ, 0);
        if (status != EFI_SUCCESS) {
            print(u"ERROR: Could not open kernel file\r\n");
            return status;
        }
    }

    /* Get file size */
    UINT8 info_buf[256];
    UINTN info_size = sizeof(info_buf);
    status = kernel_file->GetInfo(kernel_file, &file_info_guid, &info_size, info_buf);
    if (status != EFI_SUCCESS) {
        print(u"ERROR: Could not get kernel file info\r\n");
        return status;
    }

    EFI_FILE_INFO *file_info = (EFI_FILE_INFO*)info_buf;
    *kernel_size = file_info->FileSize;

    print(u"Kernel size: 0x");
    print_hex(*kernel_size);
    print(u" bytes\r\n");

    /* Allocate memory for kernel at 2MB */
    UINTN pages = (*kernel_size + 4095) / 4096;
    *kernel_addr = 0x200000;  /* 2MB */

    status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, pages, kernel_addr);
    if (status != EFI_SUCCESS) {
        /* Try allocating anywhere */
        status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages, kernel_addr);
        if (status != EFI_SUCCESS) {
            print(u"ERROR: Could not allocate memory for kernel\r\n");
            return status;
        }
    }

    print(u"Kernel loaded at: 0x");
    print_hex(*kernel_addr);
    print(u"\r\n");

    /* Read kernel into memory */
    UINTN read_size = *kernel_size;
    status = kernel_file->Read(kernel_file, &read_size, (void*)*kernel_addr);
    if (status != EFI_SUCCESS) {
        print(u"ERROR: Could not read kernel\r\n");
        return status;
    }

    kernel_file->Close(kernel_file);
    root->Close(root);

    return EFI_SUCCESS;
}

/*
 * Get memory map and convert to boot info format
 */
static EFI_STATUS get_memory_map(UINTN *map_key) {
    EFI_MEMORY_DESCRIPTOR *mmap = NULL;
    UINTN mmap_size = 0;
    UINTN desc_size;
    UINT32 desc_version;
    EFI_STATUS status;

    /* Get required size */
    status = gBS->GetMemoryMap(&mmap_size, mmap, map_key, &desc_size, &desc_version);
    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }

    /* Allocate buffer (add extra space for the allocation itself) */
    mmap_size += 2 * desc_size;
    status = gBS->AllocatePool(EfiLoaderData, mmap_size, (void**)&mmap);
    if (status != EFI_SUCCESS) {
        return status;
    }

    /* Get actual memory map */
    status = gBS->GetMemoryMap(&mmap_size, mmap, map_key, &desc_size, &desc_version);
    if (status != EFI_SUCCESS) {
        gBS->FreePool(mmap);
        return status;
    }

    /* Convert to boot info format */
    boot_info->memory_map_entries = 0;
    boot_info->memory_map = memory_entries;

    UINT8 *ptr = (UINT8*)mmap;
    UINT8 *end = ptr + mmap_size;

    while (ptr < end && boot_info->memory_map_entries < 256) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR*)ptr;
        memory_map_entry_t *entry = &memory_entries[boot_info->memory_map_entries];

        entry->base = desc->PhysicalStart;
        entry->length = desc->NumberOfPages * 4096;

        /* Convert EFI memory type to our type */
        switch (desc->Type) {
            case EfiConventionalMemory:
                entry->type = MEMORY_USABLE;
                break;
            case EfiLoaderCode:
            case EfiLoaderData:
                entry->type = MEMORY_BOOTLOADER;
                break;
            case EfiACPIReclaimMemory:
                entry->type = MEMORY_ACPI_RECLAIMABLE;
                break;
            case EfiACPIMemoryNVS:
                entry->type = MEMORY_ACPI_NVS;
                break;
            default:
                entry->type = MEMORY_RESERVED;
                break;
        }

        boot_info->memory_map_entries++;
        ptr += desc_size;
    }

    return EFI_SUCCESS;
}

/*
 * UEFI Entry Point
 */
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;
    EFI_PHYSICAL_ADDRESS kernel_addr;
    UINTN kernel_size;
    UINTN map_key;

    /* Store globals */
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gImageHandle = ImageHandle;

    /* Disable watchdog timer */
    gBS->SetWatchdogTimer(0, 0, 0, NULL);

    /* Clear screen */
    gST->ConOut->ClearScreen(gST->ConOut);

    print(u"========================================\r\n");
    print(u"       Nix-Opus UEFI Bootloader        \r\n");
    print(u"========================================\r\n\r\n");

    /* Allocate memory for boot info structure at a fixed address below kernel */
    /* Place it at 1MB which is well below the kernel at 2MB */
    EFI_PHYSICAL_ADDRESS bootinfo_addr = 0x100000;
    status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, 1, &bootinfo_addr);
    if (status != EFI_SUCCESS) {
        print(u"ERROR: Could not allocate boot info memory\r\n");
        while(1);
    }
    boot_info = (boot_info_t*)bootinfo_addr;
    memory_entries = (memory_map_entry_t*)(bootinfo_addr + sizeof(boot_info_t));

    /* Initialize boot info */
    memset(boot_info, 0, sizeof(boot_info_t));
    boot_info->magic = BOOTINFO_MAGIC;
    boot_info->version = 1;

    print(u"Boot info at: 0x");
    print_hex(bootinfo_addr);
    print(u"\r\n");

    /* Initialize graphics */
    status = init_gop();
    if (status != EFI_SUCCESS) {
        print(u"Failed to initialize graphics!\r\n");
        while(1);
    }

    /* Load kernel */
    status = load_kernel(&kernel_addr, &kernel_size);
    if (status != EFI_SUCCESS) {
        print(u"Failed to load kernel!\r\n");
        while(1);
    }

    boot_info->kernel_physical = kernel_addr;
    boot_info->kernel_size = kernel_size;

    /* Get memory map */
    status = get_memory_map(&map_key);
    if (status != EFI_SUCCESS) {
        print(u"Failed to get memory map!\r\n");
        while(1);
    }

    print(u"\r\nExiting boot services...\r\n");

    /* Exit boot services - this is the point of no return */
    status = gBS->ExitBootServices(gImageHandle, map_key);
    if (status != EFI_SUCCESS) {
        /* Memory map may have changed, try again */
        get_memory_map(&map_key);
        status = gBS->ExitBootServices(gImageHandle, map_key);
        if (status != EFI_SUCCESS) {
            /* Can't print anymore after failed ExitBootServices attempt */
            while(1);
        }
    }

    /* Jump to kernel - use inline assembly to pass argument in RDI (System V ABI)
     * The bootloader uses Microsoft ABI (RCX) but kernel expects System V (RDI) */
    __asm__ volatile(
        "mov %0, %%rdi\n\t"     /* boot_info pointer to RDI */
        "jmp *%1\n\t"           /* Jump to kernel entry point */
        :
        : "r"((UINT64)boot_info), "r"((UINT64)kernel_addr)
        : "rdi"
    );

    /* Should never reach here */
    while(1);

    return EFI_SUCCESS;
}
