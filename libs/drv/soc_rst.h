#include <stdint.h>

/* resetting the external system leaves it in the off state
 * vtor_address is where the cpu will start after release
 */
void reset_external_system0(uint32_t vtor_address);
void reset_external_system1(uint32_t vtor_address);

void release_external_system0();
void release_external_system1();
