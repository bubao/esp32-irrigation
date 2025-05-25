#ifndef ESP32_INFO_H
#define ESP32_INFO_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int chip_model;
    int major_rev;
    int minor_rev;
    int cores;
    bool wifi_bgn;
    bool bt;
    bool ble;
    bool ieee802154;
    uint32_t flash_size_mb;
    uint32_t total_heap_size;
    uint32_t free_heap_size;
    uint32_t minimum_free_heap_size;
} esp32_info_t;

void get_esp32_info(esp32_info_t* info);
void print_esp32_info(const esp32_info_t* info);

#endif // ESP32_INFO_H