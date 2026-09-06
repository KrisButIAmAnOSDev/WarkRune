#pragma once
#include <stdint.h>

void acpi_init();
void acpi_shutdown();
void acpi_reboot();
bool acpi_available();