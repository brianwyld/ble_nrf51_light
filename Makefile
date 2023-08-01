########################################################################
# Start of user section
#

# Default var
SPECIFIC_REV   = 
# if not passed on command line
VER_MAJ?=2
VER_MIN?=0
# card types : 1-5 = revA-E, 10=rect. base, 11=circ. base Default is fille revE
CARD_TYPE?=10

ROOT_DIR = .
SDKROOT = $(ROOT_DIR)/nrfsdk

# Name of output file
OUTPUT_DIR     = $(ROOT_DIR)/outputs
OUTPUT_FILE    = ble_modem
OUTPUT_DIRFILE = $(OUTPUT_DIR)/$(OUTPUT_FILE)

# Directories where find config files for tools
CONFIGDIR = tools
OPENOCD = "c:/soft/openocd-0.10.0"
OPENOCD_SCRIPT = "$(OPENOCD)/share/openocd/scripts"
MKDIR = "mkdir.exe"

# GNU parameters - if gcc is on your path then leave GNU_INSTALL_ROOT empty
#C:\Program Files (x86)\GNU Tools ARM Embedded\8 2019-q3-update
#GNU_INSTALL_ROOT := $(ROOT_DIR)/GNU_Tools_Arm_Embedded/7_2018-q2-update/bin/
GNU_INSTALL_ROOT := C:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2020-q4-major\bin
GNU_INSTALL_ROOT := 
GNU_PREFIX := arm-none-eabi


# Toolchain
#TARGET  = arm-none-eabi-
#CC      = $(TARGET)gcc
#OBJCOPY = $(TARGET)objcopy
#AS      = $(TARGET)as
#SIZE    = $(TARGET)size
#OBJDUMP = $(TARGET)objdump
#GDB 	= $(TARGET)gdb

# Toolchain commands
CC      := "$(GNU_INSTALL_ROOT)$(GNU_PREFIX)-gcc"
CXX     := "$(GNU_INSTALL_ROOT)$(GNU_PREFIX)-c++"
AS      := "$(GNU_INSTALL_ROOT)$(GNU_PREFIX)-as"
AR      := "$(GNU_INSTALL_ROOT)$(GNU_PREFIX)-ar" -r
LD      := "$(GNU_INSTALL_ROOT)$(GNU_PREFIX)-ld"
NM      := "$(GNU_INSTALL_ROOT)$(GNU_PREFIX)-nm"
OBJDUMP := "$(GNU_INSTALL_ROOT)$(GNU_PREFIX)-objdump"
OBJCOPY := "$(GNU_INSTALL_ROOT)$(GNU_PREFIX)-objcopy"
SIZE    := "$(GNU_INSTALL_ROOT)$(GNU_PREFIX)-size"
$(if $(shell $(CC) --version),,$(info Cannot find: $(CC).) \
  $(info Please set values in: "$(abspath $(TOOLCHAIN_CONFIG_FILE))") \
  $(info according to the actual configuration of your system.) \
  $(error Cannot continue))


# The CPU used
MCU = cortex-m0
# Add -mthumb for use THUMB code
THUMB = -mthumb

#################################
# Start define flags
#

# User flags for ASM 
# UASFLAGS =

# Default flags for C 
UCFLAGS  += -Wall -std=c99
#UCFLAGS  += -fmessage-length=0 -fdata-sections -ffunction-sections
# Default flags for linkage. Note use of -nostdlib to avoid the standard GCC stdlibc (we are using baselibc)
ULDFLAGS = -Wl,-Map=$(OUTPUT_DIRFILE).map,-lm,-nostdlib
# ULDFLAGS = -lm -Wl,-Map=$(OUTPUT_DIRFILE).map,--gc-sections
#ULDFLAGS += -Wl,-Map=$(OUTPUT_DIRFILE).map,-static


# Release flags additional for ASM 
# RASFLAGS =
# Release flags additional for C 
RCFLAGS  = -Os -DRELEASE_BUILD
# RCFLAGS  += -fshort-enums
# RCFLAGS  += -fpack-struct >>> build failed
# RCFLAGS  = -O1 -g
# RCFLAGS  += -fno-threadsafe-statics
# RCFLAGS  += -ffunction-sections
# RCFLAGS  += -fdata-sections
# Release flags additional for linkage 
RLDFLAGS = 

# Debug flags additional for ASM 
DASFLAGS =
# Debug flags additional for C 
DCFLAGS  = -O0 -g3 -DNDEBUG
# Debug flags additional for linkage 
DLDFLAGS = 

#
# End define flags
#################################

# Linker script file
LDSCRIPT = $(ROOT_DIR)/linker/nrf51_xxac.ld

# List ASM source files
ASRC = $(SDKROOT)/components/toolchain/gcc/gcc_startup_nrf51.s

# List C source files

CSRC = $(wildcard $(ROOT_DIR)/baselibc/src/*.c)
CSRC += $(SDKROOT)/components/boards/boards.c
CSRC += $(SDKROOT)/components/toolchain/system_nrf51.c
CSRC += $(SDKROOT)/components/ble/common/ble_advdata.c
CSRC += $(SDKROOT)/components/ble/common/ble_conn_params.c
CSRC += $(SDKROOT)/components/ble/common/ble_srv_common.c
CSRC += $(SDKROOT)/components/ble/ble_db_discovery/ble_db_discovery.c
CSRC += $(SDKROOT)/components/ble/ble_advertising/ble_advertising.c
CSRC += $(SDKROOT)/components/ble/ble_services/ble_nus/ble_nus.c
CSRC += $(SDKROOT)/components/ble/ble_services/ble_nus_c/ble_nus_c.c
CSRC += $(SDKROOT)/components/drivers_nrf/clock/nrf_drv_clock.c
CSRC += $(SDKROOT)/components/drivers_nrf/common/nrf_drv_common.c
CSRC += $(SDKROOT)/components/drivers_nrf/gpiote/nrf_drv_gpiote.c
CSRC += $(SDKROOT)/components/drivers_nrf/uart/nrf_drv_uart.c
CSRC += $(SDKROOT)/components/libraries/button/app_button.c
CSRC += $(SDKROOT)/components/libraries/util/app_error.c
CSRC += $(SDKROOT)/components/libraries/util/app_error_weak.c
CSRC += $(SDKROOT)/components/libraries/fifo/app_fifo.c
CSRC += $(SDKROOT)/components/libraries/timer/app_timer.c
CSRC += $(SDKROOT)/components/libraries/uart/app_uart_fifo.c
CSRC += $(SDKROOT)/components/libraries/util/app_util_platform.c
CSRC += $(SDKROOT)/components/libraries/hardfault/hardfault_implementation.c
CSRC += $(SDKROOT)/components/libraries/util/nrf_assert.c
CSRC += $(SDKROOT)/components/libraries/uart/retarget.c
CSRC += $(SDKROOT)/components/libraries/util/sdk_errors.c
CSRC += $(SDKROOT)/components/libraries/fstorage/fstorage.c
CSRC += $(SDKROOT)/components/libraries/log/src/nrf_log_backend_serial.c
CSRC += $(SDKROOT)/components/libraries/log/src/nrf_log_frontend.c
# CSRC += $(SDKROOT)/external/segger_rtt/RTT_Syscalls_KEIL.c
# CSRC += $(SDKROOT)/external/segger_rtt/SEGGER_RTT.c
# CSRC += $(SDKROOT)/external/segger_rtt/SEGGER_RTT_printf.c
CSRC += $(SDKROOT)/components/softdevice/common/softdevice_handler/softdevice_handler.c
CSRC += $(SDKROOT)/components/libraries/bsp/bsp.c
CSRC += $(SDKROOT)/components/libraries/bsp/bsp_btn_ble.c
CSRC += $(wildcard $(ROOT_DIR)/src/*.c)

# List of directories to include
UINCDIR = $(ROOT_DIR)/src/includes
UINCDIR += $(ROOT_DIR)/baselibc/include
UINCDIR += $(ROOT_DIR)/baselibc/src/templates
UINCDIR += $(SDKROOT)/config
UINCDIR += $(SDKROOT)/components
UINCDIR += $(SDKROOT)/components/ble/ble_advertising
UINCDIR += $(SDKROOT)/components/ble/ble_db_discovery
UINCDIR += $(SDKROOT)/components/ble/ble_dtm
UINCDIR += $(SDKROOT)/components/ble/ble_racp
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_ancs_c
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_ans_c
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_bas
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_bas_c
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_cscs
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_cts_c
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_dfu
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_dis
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_gls
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_hids
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_hrs
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_hrs_c
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_hts
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_ias
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_ias_c
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_lbs
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_lbs_c
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_lls
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_nus
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_nus_c
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_rscs
UINCDIR += $(SDKROOT)/components/ble/ble_services/ble_rscs_c
UINCDIR += $(SDKROOT)/components/ble/common
UINCDIR += $(SDKROOT)/components/ble/nrf_ble_qwr
UINCDIR += $(SDKROOT)/components/ble/peer_manager
UINCDIR += $(SDKROOT)/components/boards
UINCDIR += $(SDKROOT)/components/device
UINCDIR += $(SDKROOT)/components/drivers_nrf/adc
UINCDIR += $(SDKROOT)/components/drivers_nrf/clock
UINCDIR += $(SDKROOT)/components/drivers_nrf/common
UINCDIR += $(SDKROOT)/components/drivers_nrf/comp
UINCDIR += $(SDKROOT)/components/drivers_nrf/delay
UINCDIR += $(SDKROOT)/components/drivers_nrf/gpiote
UINCDIR += $(SDKROOT)/components/drivers_nrf/hal
UINCDIR += $(SDKROOT)/components/drivers_nrf/i2s
UINCDIR += $(SDKROOT)/components/drivers_nrf/lpcomp
UINCDIR += $(SDKROOT)/components/drivers_nrf/pdm
UINCDIR += $(SDKROOT)/components/drivers_nrf/powerppi
UINCDIR += $(SDKROOT)/components/drivers_nrf/pwm
UINCDIR += $(SDKROOT)/components/drivers_nrf/qdec
UINCDIR += $(SDKROOT)/components/drivers_nrf/rng
UINCDIR += $(SDKROOT)/components/drivers_nrf/rtc
UINCDIR += $(SDKROOT)/components/drivers_nrf/saadc
UINCDIR += $(SDKROOT)/components/drivers_nrf/spi_master
UINCDIR += $(SDKROOT)/components/drivers_nrf/spi_slave
UINCDIR += $(SDKROOT)/components/drivers_nrf/swi
UINCDIR += $(SDKROOT)/components/drivers_nrf/timer
UINCDIR += $(SDKROOT)/components/drivers_nrf/twi_master
UINCDIR += $(SDKROOT)/components/drivers_nrf/twis_slave
UINCDIR += $(SDKROOT)/components/drivers_nrf/uart
UINCDIR += $(SDKROOT)/components/drivers_nrf/usbd
UINCDIR += $(SDKROOT)/components/drivers_nrf/wdt
UINCDIR += $(SDKROOT)/components/libraries/bsp
UINCDIR += $(SDKROOT)/components/libraries/button
UINCDIR += $(SDKROOT)/components/libraries/crc16
UINCDIR += $(SDKROOT)/components/libraries/crc32
UINCDIR += $(SDKROOT)/components/libraries/csense
UINCDIR += $(SDKROOT)/components/libraries/csense_drv
UINCDIR += $(SDKROOT)/components/libraries/experimental_section_vars
UINCDIR += $(SDKROOT)/components/libraries/fds
UINCDIR += $(SDKROOT)/components/libraries/fifo
UINCDIR += $(SDKROOT)/components/libraries/fstorage
UINCDIR += $(SDKROOT)/components/libraries/gpiote
UINCDIR += $(SDKROOT)/components/libraries/hardfault
UINCDIR += $(SDKROOT)/components/libraries/hci
UINCDIR += $(SDKROOT)/components/libraries/led_softblink
UINCDIR += $(SDKROOT)/components/libraries/log
UINCDIR += $(SDKROOT)/components/libraries/log/src
UINCDIR += $(SDKROOT)/components/libraries/low_power_pwm
UINCDIR += $(SDKROOT)/components/libraries/mem_manager
UINCDIR += $(SDKROOT)/components/libraries/pwm
UINCDIR += $(SDKROOT)/components/libraries/queue
UINCDIR += $(SDKROOT)/components/libraries/scheduler
UINCDIR += $(SDKROOT)/components/libraries/slip
UINCDIR += $(SDKROOT)/components/libraries/timer
UINCDIR += $(SDKROOT)/components/libraries/twi
UINCDIR += $(SDKROOT)/components/libraries/uart
UINCDIR += $(SDKROOT)/components/libraries/usbd
UINCDIR += $(SDKROOT)/components/libraries/usbd/class/audio
UINCDIR += $(SDKROOT)/components/libraries/usbd/class/cdc
UINCDIR += $(SDKROOT)/components/libraries/usbd/class/cdc/acm
UINCDIR += $(SDKROOT)/components/libraries/usbd/class/hid
UINCDIR += $(SDKROOT)/components/libraries/usbd/class/hid/generic
UINCDIR += $(SDKROOT)/components/libraries/usbd/class/hid/kbd
UINCDIR += $(SDKROOT)/components/libraries/usbd/class/hid/mouse
UINCDIR += $(SDKROOT)/components/libraries/usbd/class/msc
UINCDIR += $(SDKROOT)/components/libraries/usbd/config
UINCDIR += $(SDKROOT)/components/libraries/util
UINCDIR += $(SDKROOT)/components/softdevice/common/softdevice_handler
UINCDIR += $(SDKROOT)/components/softdevice/s130/headers
UINCDIR += $(SDKROOT)/components/softdevice/s130/headers/nrf51
UINCDIR += $(SDKROOT)/components/toolchain
UINCDIR += $(SDKROOT)/components/toolchain/arm
UINCDIR += $(SDKROOT)/components/toolchain/gcc
UINCDIR += $(SDKROOT)/components/toolchain/CMSIS/Include
UINCDIR += $(SDKROOT)/components/toolchain/gcc
UINCDIR += $(SDKROOT)/external/segger_rtt

# List of user define
UDEFS += NRF51
UDEFS += BLE_STACK_SUPPORT_REQD
UDEFS += __HEAP_SIZE=0
UDEFS += NRF51822
UDEFS += BOARD_CUSTOM
UDEFS += BOARD_W_BLE_LIGHT_MINEW_MS50SFA2
UDEFS += S130
UDEFS += NRF_SD_BLE_API_VERSION=2
UDEFS += BSP_UART_SUPPORT
UDEFS += SOFTDEVICE_PRESENT
UDEFS += SWI_DISABLE0

# UASDEFS += BOARD_PCA10028
# UASDEFS += SOFTDEVICE_PRESENT
# UASDEFS += NRF51
# UASDEFS += S130
# UASDEFS += BLE_STACK_SUPPORT_REQD
# UASDEFS += SWI_DISABLE0
# UASDEFS += BSP_DEFINES_ONLY
# UASDEFS += NRF51822
# UASDEFs += NRF_SD_BLE_API_VERSION=2

UASDEFs += __HEAP_SIZE=16

# define for specific revision and set fw major/minor values
UDEFS += $(SPECIFIC_REV) 
UDEFS += FW_MAJOR=$(VER_MAJ)
UDEFS += FW_MINOR=$(VER_MIN)
UDEFS += CARD_TYPE=$(CARD_TYPE)

# List of release define in more
RDEFS = 

# List of debug define in more
DDEFS =

# List of libraries directory
ULIBDIR = $(ROOT_DIR)/linker

# List of libraries
ULIBS = 

#
# End of user defines
########################################################################


########################################################################
# Start build define
#

# Binary objects directory
OBJS = .obj
# Binary ASM objects directory
DASOBJS = $(OBJS)/asm
# Binary C objects directory
DCOBJS = $(OBJS)/c

# ASM list of binary objects
ASOBJS=$(patsubst %.s,$(DASOBJS)/%.o, $(ASRC))
# C list of binary objects
COBJS=$(patsubst %.c,$(DCOBJS)/%.o, $(CSRC))

# List of include directory
INCDIR = $(patsubst %,-I%, $(UINCDIR))
# List of include library
LIBDIR = $(patsubst %,-L%, $(ULIBDIR))

# List of library
LIBS = $(patsubst %,-l%, $(ULIBS))

# List of define
_UDEFS = $(patsubst %,-D%, $(UDEFS))
_RDEFS = $(patsubst %,-D%, $(RDEFS))
_DDEFS = $(patsubst %,-D%, $(DDEFS))
_UASDEFS = $(patsubst %,-defsym %, $(UASDEFS))

#
# End build define
########################################################################

########################################################################
# Start rules section
#

all:release

# Build define for release
#.PHONY: release
# release:ASFLAGS = -mcpu=$(MCU) $(THUMB) $(RASFLAGS) $(UASFLAGS) $(_UASDEFS)
release:ASFLAGS = -mcpu=$(MCU) $(THUMB)
release:CFLAGS  = -mcpu=$(MCU) $(THUMB) $(RCFLAGS) $(UCFLAGS) $(_UDEFS) $(_RDEFS) $(INCDIR)
release:LDFLAGS = -mcpu=$(MCU) $(THUMB) $(RLDFLAGS) $(ULDFLAGS) -T$(LDSCRIPT) $(LIBDIR)
release:$(OUTPUT_DIRFILE).elf

# Build define for debug
#.PHONY: debug
debug:ASFLAGS = -mcpu=$(MCU) $(THUMB) $(DASFLAGS) $(UASFLAGS) $(_UASDEFS)
debug:CFLAGS  = -mcpu=$(MCU) $(THUMB) $(DCFLAGS) $(UCFLAGS) $(_UDEFS) $(_DDEFS) $(INCDIR)
debug:LDFLAGS = -mcpu=$(MCU) $(THUMB) $(DLDFLAGS) $(ULDFLAGS) -T$(LDSCRIPT) $(LIBDIR)
debug:$(OUTPUT_DIRFILE).elf

# Build sources to generate elf file
%.elf: cobjs aobjs #$(COBJS) $(ASOBJS) 
	@$(MKDIR) -p $(patsubst /%,%, $(@D))
	$(CC) -o $@ $(ASOBJS) $(COBJS) $(LDFLAGS) $(LIBS)

cobjs: $(COBJS)
	$(info compiled C files)
	
aobjs: $(ASOBJS)
	$(info assembled S files)
	
# Build ASM sources
.PRECIOUS: $(DASOBJS)/%.o
$(DASOBJS)/%.o: %.s
	@$(MKDIR) -p $(patsubst /%,%, $(@D))
	$(AS) $(ASFLAGS) $< -o $@

# Build C sources
.PRECIOUS: $(DCOBJS)/%.o
$(DCOBJS)/%.o: %.c
	@$(MKDIR) -p $(patsubst /%,%, $(@D))
	$(CC) $(CFLAGS) $< -c -o $@

hex:
	$(OBJCOPY) -O ihex $(OUTPUT_DIRFILE).elf $(OUTPUT_DIRFILE).hex

bin:
	$(OBJCOPY) -O binary $(OUTPUT_DIRFILE).elf $(OUTPUT_DIRFILE).bin

size:
	$(SIZE) $(OUTPUT_DIRFILE).elf
	$(SIZE) $(OUTPUT_DIRFILE).hex
# $(SIZE) $(OUTPUT_DIRFILE).bin

binsize: bin
	@du -bhs $(OUTPUT_DIRFILE).bin
	
disassemble:
	$(OBJDUMP) -hd $(OUTPUT_DIRFILE).elf > $(OUTPUT_DIRFILE).lss

itall: clean release hex bin disassemble size

itall_debug: clean debug hex bin disassemble size

# incremental dev, just rebuild changed source files and reflash to board
dev: debug hex bin disassemble size flash

# NOTE flash/debug rules here for info, untested in specific project
# Reset target
reset:
	@echo Reset board...
	nrfjprog -f nrf51 --reset
	
flash: flash_program
flashall: flash_erase flash_softdevice flash_program reset

flash_program:
	@echo Flashing: $(OUTPUT_DIRFILE).hex
	nrfjprog -f nrf51 --program $(OUTPUT_DIRFILE).hex --sectorerase --verify
	nrfjprog -f nrf51 --reset

flash_boot:
	# THIS DOESNT WORK - NO RUN AFTER
	@echo Flashing: hexs\bootloader.hex
	nrfjprog -f nrf51 --program $(ROOT_DIR)\hexs\bootloader.hex --sectorerase --verify

flash_softdevice:
	@echo Flashing: s130_nrf51_2.0.1_softdevice.hex from sdk
	nrfjprog -f nrf51 --program $(ROOT_DIR)\hexs\s130_nrf51_2.0.1_softdevice.hex --sectorerase --verify

# Erase flash
flash_erase:
	@echo Erasing flash...
	nrfjprog -f nrf51 --eraseall
	
## Flash target
#st_flash_program:bin
#	st-flash --reset write $(OUTPUT_DIRFILE).bin 0x8000000
	
## dfu target
#dfu_flash_program:bin
#	dfu-util -a 0 -s 0x08000000 -D $(OUTPUT_DIRFILE).bin 0x8000000

## Erase flash
#st_flash_erase:
#	@st-flash erase
	
# Run gdb/openocd for load and debug.
gdb:debug
	$(GDB) --command=$(CONFIGDIR)/gdb/init.cfg $(OUTPUT_DIRFILE).elf
	
# Run cgdb/openocd for load and debug with color.
cgdb:debug
	cgdb -d $(GDB) --command=$(CONFIGDIR)/gdb/init.cfg $(OUTPUT_DIRFILE).elf

# Clean projet
clean:cleanOutputs
	rm -fr $(OBJS)

cleanOutputs:
ifneq ($(wildcard $(OUTPUT_DIR)*.elf),$(wildcard))
	rm -f $(OUTPUT_DIR)*.elf
endif
ifneq ($(wildcard $(OUTPUT_DIR)*.map),$(wildcard))
	rm -f $(OUTPUT_DIR)*.map
endif
ifneq ($(wildcard $(OUTPUT_DIR)*.bin),$(wildcard))
	rm -f $(OUTPUT_DIR)*.bin
endif
ifneq ($(wildcard $(OUTPUT_DIR)*.hex),$(wildcard))
	rm -f $(OUTPUT_DIR)*.hex
endif
ifneq ($(wildcard $(OUTPUT_DIR)*.lss),$(wildcard))
	rm -f $(OUTPUT_DIR)*.lss
endif
ifneq ($(wildcard $(OUTPUT_DIR)*.log),$(wildcard))
	rm -f $(OUTPUT_DIR)*.log
endif
	rm -fr $(OUTPUT_DIR)

#
# End rules section
########################################################################
