// Standalone macOS diagnostic: one 24 MP buffer, no Qt, images or GUI.
// Compile: clang -O2 tests/allocator_probe.c -o /tmp/qingview-allocator-probe
// Run without allocator environment overrides; output JSON to a result file.
#include <mach/mach.h>
#include <malloc/malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static unsigned long long footprint(void)
{
    task_vm_info_data_t info = {0};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&info, &count) != KERN_SUCCESS)
        return 0;
    return info.phys_footprint;
}

static void sample(const char *name, int separator)
{
    printf("%s{\"stage\":\"%s\",\"footprint_bytes\":%llu}",
           separator ? "," : "", name, footprint());
}

int main(void)
{
    const size_t bytes = 6000U * 4000U * 4U;
    uintptr_t prior = 0;
    int sameAddress = 0;
    printf("{\"schema\":1,\"buffer_bytes\":%zu,\"qt_loaded\":false,"
           "\"allocator_environment_changed_by_probe\":false,\"samples\":[", bytes);
    sample("initial", 0);
    for (int round = 0; round < 3; ++round) {
        volatile unsigned char *buffer = malloc(bytes);
        if (!buffer)
            return 2;
        if (round && (uintptr_t)buffer == prior)
            ++sameAddress;
        prior = (uintptr_t)buffer;
        // Touch every page so physical accounting reflects the allocated buffer.
        for (size_t offset = 0; offset < bytes; offset += (size_t)getpagesize())
            buffer[offset] = (unsigned char)round;
        sample("allocated_and_touched", 1);
        free((void *)buffer);
        sample("freed", 1);
    }
    const size_t relieved = malloc_zone_pressure_relief(NULL, 0);
    sample("after_pressure_relief", 1);
    printf("],\"same_address_reuses\":%d,\"pressure_relief_reported_bytes\":%zu}\n",
           sameAddress, relieved);
    return 0;
}
