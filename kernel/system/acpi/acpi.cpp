#include "system/acpi/acpi.hpp"
#include "io/io.hpp"
#include "io/serial/serial.hpp"
#include "memory/paging/paging.hpp"

struct RSDP {
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed));

struct ACPISDTHeader {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed));

struct FADT {
    ACPISDTHeader header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t reserved;
    uint8_t preferred_pm_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t pm1_event_length;
    uint8_t pm1_control_length;
    uint8_t pm2_control_length;
    uint8_t pm_timer_length;
    uint8_t gpe0_length;
    uint8_t gpe1_length;
    uint8_t gpe1_base;
    uint8_t cstate_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alarm;
    uint8_t month_alarm;
    uint8_t century;
    uint16_t boot_arch_flags;
    uint8_t reserved2;
    uint32_t flags;
} __attribute__((packed));

static bool acpi_found = false;
static FADT* fadt = nullptr;
static uint16_t pm1a_control_block = 0;
static uint16_t pm1b_control_block = 0;
static uint16_t slp_typa = 0;
static uint16_t slp_typb = 0;

static bool checksum_valid(void* ptr, uint32_t len) {
    uint8_t* bytes = (uint8_t*)ptr;
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++)
        sum += bytes[i];
    return sum == 0;
}

static RSDP* find_rsdp() {
    for (uint32_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        RSDP* rsdp = (RSDP*)addr;
        if (rsdp->signature[0] == 'R' && rsdp->signature[1] == 'S' &&
            rsdp->signature[2] == 'D' && rsdp->signature[3] == ' ' &&
            rsdp->signature[4] == 'P' && rsdp->signature[5] == 'T' &&
            rsdp->signature[6] == 'R' && rsdp->signature[7] == ' ') {
            if (checksum_valid(rsdp, 20))
                return rsdp;
        }
    }
    return nullptr;
}

static ACPISDTHeader* find_table(ACPISDTHeader* rsdt, const char* signature) {
    if (rsdt->length < sizeof(ACPISDTHeader) + 4) return nullptr;
    uint32_t entries = (rsdt->length - sizeof(ACPISDTHeader)) / 4;
    uint32_t* entry_ptr = (uint32_t*)((uint8_t*)rsdt + sizeof(ACPISDTHeader));

    for (uint32_t i = 0; i < entries; i++) {
        ACPISDTHeader* h = (ACPISDTHeader*)entry_ptr[i];
        if (h->signature[0] == signature[0] &&
            h->signature[1] == signature[1] &&
            h->signature[2] == signature[2] &&
            h->signature[3] == signature[3]) {
            return h;
        }
    }
    return nullptr;
}

void acpi_init() {
    serial_puts("acpi: searching for RSDP...\n");

    RSDP* rsdp = find_rsdp();
    if (!rsdp) {
        serial_puts("acpi: RSDP not found\n");
        acpi_found = false;
        return;
    }

    serial_puts("acpi: RSDP found at 0x");
    serial_puthex((uint32_t)rsdp);
    serial_putc('\n');

    ACPISDTHeader* rsdt = (ACPISDTHeader*)(uint32_t)rsdp->rsdt_address;
    if (!rsdt || (uint32_t)rsdt < 0x100000) {
        serial_puts("acpi: RSDT address invalid\n");
        acpi_found = false;
        return;
    }
    if (!checksum_valid(rsdt, rsdt->length)) {
        serial_puts("acpi: RSDT checksum invalid\n");
        acpi_found = false;
        return;
    }

    fadt = (FADT*)find_table(rsdt, "FACP");
    if (!fadt) {
        serial_puts("acpi: FADT not found\n");
        acpi_found = false;
        return;
    }

    pm1a_control_block = fadt->pm1a_control_block;
    pm1b_control_block = fadt->pm1b_control_block;

    serial_puts("acpi: PM1a control = 0x");
    serial_puthex(pm1a_control_block);
    serial_putc('\n');

    uint8_t* dsdt = (uint8_t*)fadt->dsdt;
    uint32_t dsdt_len = ((ACPISDTHeader*)dsdt)->length;
    if (dsdt_len < sizeof(ACPISDTHeader) || dsdt_len > 1048576) {
        serial_puts("acpi: DSDT invalid size\n");
        acpi_found = true;
        return;
    }

    for (uint32_t i = 0; i + 8 < dsdt_len; i++) {
        if (dsdt[i] == '_' && dsdt[i+1] == 'S' && dsdt[i+2] == '5' && dsdt[i+3] == '_') {
            if (dsdt[i+4] == 0x12) {
                uint32_t offset = i + 5;
                if (offset + 1 >= dsdt_len) break;
                offset += (dsdt[offset] & 0x0F) + 2;
                if (offset + 1 < dsdt_len && dsdt[offset] == 0x0A) slp_typa = dsdt[offset + 1];
                offset += 2;
                if (offset + 1 < dsdt_len && dsdt[offset] == 0x0A) slp_typb = dsdt[offset + 1];
                break;
            }
        }
    }

    serial_puts("acpi: SLP_TYPa = 0x");
    serial_puthex(slp_typa);
    serial_puts(" SLP_TYPb = 0x");
    serial_puthex(slp_typb);
    serial_putc('\n');

    acpi_found = true;
    serial_puts("acpi: initialized\n");
}

bool acpi_available() {
    return acpi_found;
}

void acpi_shutdown() {
    if (!acpi_found) {
        serial_puts("acpi: shutdown not available\n");
        return;
    }

    serial_puts("acpi: shutting down...\n");

    if (pm1a_control_block) {
        outw(pm1a_control_block, (slp_typa << 10) | (1 << 13));
    }
    if (pm1b_control_block) {
        outw(pm1b_control_block, (slp_typb << 10) | (1 << 13));
    }

    while (1) asm volatile("hlt");
}

void acpi_reboot() {
    serial_puts("acpi: rebooting...\n");

    uint8_t good = 0x02;
    int timeout = 100000;
    while ((good & 0x02) && timeout-- > 0)
        good = inb(0x64);
    outb(0x64, 0xFE);

    timeout = 1000000;
    while (timeout-- > 0) asm volatile("nop");
    outb(0x92, 0x03);

    while (1) asm volatile("hlt");
}
