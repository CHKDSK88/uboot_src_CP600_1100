#ifndef HW_CAP_H
#define HW_CAP_H

#define IS_WIRELESS_MASK (0x2)

int get_hw_capabilities(unsigned int *hw_cap);

int is_hw_cap_bit_on(unsigned int bit);

int is_wireless_supported(void);

#endif /* HW_CAP_H */

