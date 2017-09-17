###############################################################################
# Makefile for groovesizer stochastic
# Requires either
# * AVR compiler (avr-gcc et al) in path, or
# * AVR_TOOLKIT_ROOT set to installation directory for example:
#     export AVR_TOOLKIT_ROOT="/e/codez/arduino-1.8.2/hardware/tools/avr"
#     export AVR_TOOLKIT_ROOT="/c/data/dev/arduino-1.8.2/hardware/tools/avr"
# path needed at home /c/data/dev/GnuWin32/bin
#   NOTE that on windows, you MAY need the quotes and the windows style path (not sure when)
#   This path is the root of which the ./bin directory contains avr-gcc
#
# if VERBOSE is defined, produce more output
#   example: make fw VERBOSE=1

ifdef VERBOSE
ECHO := @true
AT :=
#LDFLAGS += -v
else
# prefix a command with @ and it will not echo the output
AT :=@
ECHO := @echo
endif

ifeq ("$(strip $(AVR_TOOLKIT_ROOT))","")
 checkforgcc := $(shell which avr-gcc 1>/dev/null 2>/dev/null && echo "yes" || echo "no" ) #1> /dev/null 2>&1; echo $$?)
 ifneq ("$(strip $(checkforgcc))","yes")
  $(error AVR path is not set! Add the avr bin directory to the path or set AVR_TOOLKIT_ROOT )
 endif
 BINPATH=""
 DUDECONF=$(realpath $(dir $(shell which avrdude))/.. )/etc/avrdude.conf 
else
 BINPATH=$(AVR_TOOLKIT_ROOT)/bin/
 DUDECONF=$(AVR_TOOLKIT_ROOT)/etc/avrdude.conf
endif

CC  =$(addprefix $(BINPATH),avr-gcc)
CXX =$(addprefix $(BINPATH),avr-c++)
LD  =$(addprefix $(BINPATH),avr-ld)
CP  =$(addprefix $(BINPATH),avr-objcopy)
OD  =$(addprefix $(BINPATH),avr-objdump)
AS  =$(addprefix $(BINPATH),avr-as)
AR  =$(addprefix $(BINPATH),avr-gcc-ar)
DUDE=$(addprefix $(BINPATH),avrdude)
SIZ =$(addprefix $(BINPATH),avr-size)

SRCDIR=./src
CSRCFILES    = $(shell find $(SRCDIR) -type f -name "*.c"  )
CPPSRCFILES  = $(shell find $(SRCDIR) -type f -name "*.cpp"  )
ASSRCFILES   = $(shell find $(SRCDIR) -type f -name "*.S"  )
COBJFILES    = $(addprefix $(OBJDIR),$(notdir $(CSRCFILES:.c=.o)))
CPPOBJFILES  = $(addprefix $(OBJDIR),$(notdir $(CPPSRCFILES:.cpp=.o)))
ASOBJFILES   = $(addprefix $(OBJDIR),$(notdir $(ASSRCFILES:.S=.o)))
SRCFILES     = $(CSRCFILES) $(CPPSRCFILES) $(ASSRCFILES)
OBJFILES     = $(COBJFILES) $(CPPOBJFILES) $(ASOBJFILES)

COREDIR=./core
CCOREFILES    = $(shell find $(COREDIR) -type f -name "*.c"  )
CPPCOREFILES  = $(shell find $(COREDIR) -type f -name "*.cpp"  )
ASCOREFILES   = $(shell find $(COREDIR) -type f -name "*.S"  )
CCOREOBJFILES    = $(addprefix $(OBJDIR),$(notdir $(CCOREFILES:.c=.o)))
CPPCOREOBJFILES  = $(addprefix $(OBJDIR),$(notdir $(CPPCOREFILES:.cpp=.o)))
ASCOREOBJFILES   = $(addprefix $(OBJDIR),$(notdir $(ASCOREFILES:.S=.o)))
CORESRCFILES     = $(CCOREFILES) $(CPPCOREFILES) $(ASCOREFILES)
COREOBJFILES     = $(CCOREOBJFILES) $(CPPCOREOBJFILES) $(ASCOREOBJFILES)


# add locations to search for files
vpath %.cpp ./core
vpath %.cpp ./core/wire
vpath %.cpp ./core/eeprom
vpath %.cpp ./core/midi
vpath %.cpp ./core/timerone
vpath %.cpp ./src
vpath %.c ./core
vpath %.c ./core/wire/utility
vpath %.c ./core/wire


# SETUP
OPTIMIZE=s
OBJDIR=./build/
# target binary that can be uploaded to box
BINARY = $(OBJDIR)stocha.hex
MAP    = $(OBJDIR)stocha.map
ELF    =$(OBJDIR)stocha.elf
CORE   =$(OBJDIR)core.a

# Project defines
DEFINES = -DF_CPU=16000000L -DARDUINO=10802 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR

# Include directories
INCLUDES += -I"./core"
INCLUDES += -I"./core/eeprom"
INCLUDES += -I"./core/midi"
INCLUDES += -I"./core/pins"
INCLUDES += -I"./core/timerone"
INCLUDES += -I"./core/wire"
INCLUDES += -I"./core/src"

# options to pass compiler for the different targets
OPTS    = -c -g -flto -mmcu=atmega328p
ASMOPTS = -x assembler-with-cpp 
COPTS   = -Os -std=gnu11 -ffunction-sections -fdata-sections -fno-fat-lto-objects
CPPOPTS = -Os -std=gnu++11 -fpermissive -fno-exceptions -ffunction-sections -fdata-sections -fno-threadsafe-statics 

CFLAGS = $(OPTS) $(COPTS) $(DEFINES) $(INCLUDES) -O$(OPTIMIZE)
CPPFLAGS = $(OPTS) $(CPPOPTS) $(DEFINES) $(INCLUDES) -O$(OPTIMIZE)
ASMFLAGS = $(OPTS) $(ASMOPTS) $(DEFINES) $(INCLUDES) -O$(OPTIMIZE)
LDFLAGS += -Os -g -flto -fuse-linker-plugin -Wl,--gc-sections -mmcu=atmega328p -lm 
LDFLAGS += -Wl,-Map=$(MAP) 

###############################################################################
# TARGETS
print-%  : ; @echo $* = $($*)

# Require explicit target
.PHONY: all
all:
	@echo "Valid targets are"
	@echo " fw : build firmware"
	@echo " upload : upload firmware to device"
	@echo " clean : clean build directory"
	@echo " showfiles : print list of files that would be compiled"
	@echo " showsize : display memory usage of final firmware image"
	
.PHONY: clean
clean:
	@$(RM) $(BINARY)
	@$(RM) $(ELF)
	@$(RM) $(MAP)	
	@$(RM) $(OBJDIR)/*.o
	@$(RM) $(OBJDIR)/*.d
	
.PHONY: upload
upload:
	$(ECHO) "Uploading..."
	$(AT)$(DUDE) -C$(DUDECONF) -v -patmega328p -cstk500v2 -Pusb -Uflash:w:$(BINARY):i
	
	@echo "AVR_TOOLKIT_ROOT='$(AVR_TOOLKIT_ROOT)'"
	@echo "CC  = $(CC)"
	@echo "CXX = $(CXX)"
	@echo "AS  = $(AS)"

.PHONY: showfiles
showfiles:
	@echo "Files that will be built:"
	@echo "$(SRCFILES)"
	@echo "Core files that will be built:"
	@echo "$(CORESRCFILES)"

.PHONY: fw
fw: $(BINARY)

$(ELF): $(OBJFILES) $(CORE)
	$(ECHO) "Linking $@..."
	$(AT)$(CC) $(LDFLAGS) $^ -o $@

$(BINARY): $(ELF)
	$(ECHO) "Creating hex $@..."
	$(AT)$(CP) -O ihex -R .eeprom $^ $@

$(CORE): $(COREOBJFILES)
	$(ECHO) "Conjuring core..."
	$(AT)rm $(CORE) || true
	$(AT)$(AR) rcs $@ $^ 
	
.PHONY: showsize	
showsize:
	$(AT)$(SIZ) -C --mcu=atmega328p $(ELF)


# create objdir if it doesn't exist	
$(OBJFILES) : | $(OBJDIR)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(OBJDIR)%.o: %.c
	$(ECHO) "Compiling $<..."
	$(AT)$(CC) $(CFLAGS) $< -o $@
	
$(OBJDIR)%.o: %.cpp
	$(ECHO) "Compiling $<..."
	$(AT)$(CXX) $(CPPFLAGS) $< -o $@	

$(OBJDIR)%.o: %.S
	$(ECHO) "Assembling $<..."
	$(AT)$(AS) $(ASMFLAGS) $< -o $@ > $@.lst

# Automatic dependency generation
CFLAGS += -MMD
CPPFLAGS += -MMD
ASMFLAGS += -MMD
-include $(OBJFILES:.o=.d)