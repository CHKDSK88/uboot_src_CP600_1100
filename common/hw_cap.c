#include <common.h>
#include <hw_cap.h>

int get_hw_capabilities(unsigned int *hw_cap) {
	char *hw_cap_str = getenv("hardware_capabilities");

	if (hw_cap_str == NULL) {
		printf("Could not read HW capablities!!!\n");
		return -1;
	}
	(*hw_cap) = (unsigned int)simple_strtoul(hw_cap_str, NULL, 16);
}

int is_hw_cap_bit_on(unsigned int bit) {
	unsigned int hw_cap;

	if (get_hw_capabilities(&hw_cap) == -1) {
		return 0;
	}

	return (hw_cap & bit);	
}

int is_wireless_supported(void) {
	return is_hw_cap_bit_on(IS_WIRELESS_MASK);
}

