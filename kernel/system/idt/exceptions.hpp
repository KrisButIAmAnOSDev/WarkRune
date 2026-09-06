#pragma once
#include <stdint.h>

extern "C" {
    void exception_divide_zero();
    void exception_invalid_op();
    void exception_no_fpu();
    void exception_double_fault();
    void exception_stack_fault();
    void exception_gpf();
    void exception_page_fault();
    void exception_overflow();
    void _page_fault_handler(uint32_t cr2);
}