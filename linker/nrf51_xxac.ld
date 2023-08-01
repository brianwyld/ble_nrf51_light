/* Linker script to configure memory regions. */
SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  /* flash is 0x40000 size (256kb) for nRF51822 */
  /* With softdevice, our app starts at 0x1B000 (see S130 SoftDevice spec v2.0 Memory resource Map and Usage) */
  /* And we run to 0x3ABFF (to leave 3AC00->3FFFF for the bootloader+bootloader settings) which is a total of 127kb */
  /* We leave 1K (0x400) for our config store at the end of the app area, so our app can be up to 126kb */
  FLASH (rx) : ORIGIN = 0x1b000, LENGTH = 0x3AC00-0x1b000-0x400    
  FLASH_CFG (rw!x) : ORIGIN = 0x3AC00-0x400, LENGTH = 0x400
  BOOTLOADER_START (rx) : ORIGIN = 0x3AC00, LENGTH = 0x5000
  BOOTLOADER_SETTINGS (rw!x) : ORIGIN = 0x3FC00, LENGTH = 0x400

  /* RAM starts at 0x20000000 and is size 0x4000 (16kb) */
  /* our RAM area is just after softdevice area , up to the end */
  /* Softdevice S130 requires 0x1288+'configurable resources': min is 0x13C8 if using minimal resources 
  /** see components/softdevice/common/softdevice_handle/app_ram_base.h for values based on configs */
  /* 0 Central + 1 periph + 1 UUID -> 0x20001fe8 */
  /* 1 Central + 1 periph + 1 UUID -> 0x20002418 -> 25E0 ? (in debugger, break in softdevice_enable() to see its calulcation */
  RAM (rwx) :  ORIGIN = 0x200025E0, LENGTH = 0x4000-0x25E0
  /* Note we set the heap to size 0 */
}

SECTIONS
{
  .fs_data :
  {
    PROVIDE(__start_fs_data = .);
    KEEP(*(.fs_data))
    PROVIDE(__stop_fs_data = .);
  } > RAM
  .pwr_mgmt_data :
  {
    PROVIDE(__start_pwr_mgmt_data = .);
    KEEP(*(.pwr_mgmt_data))
    PROVIDE(__stop_pwr_mgmt_data = .);
  } > RAM
} INSERT AFTER .data;

INCLUDE "./linker/nrf5x_common.ld"

/* allow code to know where the space for config is, and its size by exporting as linker variables */
PROVIDE(__FLASH_CONFIG_BASE_ADDR = ORIGIN(FLASH_CFG));
PROVIDE(__FLASH_CONFIG_SZ = LENGTH(FLASH_CFG));
