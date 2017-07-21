###############################################################################
# Makefile for groovesizer stochastic
# Requires either
# * AVR compiler (avr-gcc et al) in path, or
# * AVR_TOOLKIT_ROOT set to installation directory for example:
#     AVR_TOOLKIT_ROOT = "E:\codez\arduino-1.8.2\hardware\tools\avr"
#   Note that on windows, you need the quotes and the windows style path even in mingw
#   This path is the root of which the ./bin directory contains avr-gcc
#
# if VERBOSE is defined, produce more output

ifdef VERBOSE
ECHO := @true
AT :=
LDFLAGS += -v
else
# prefix a command with @ and it will not echo the output
AT :=@
ECHO := @echo
endif

ifneq ("$(strip $(AVR_TOOLKIT_ROOT))","")
BINPATH=$(AVR_TOOLKIT_ROOT)/bin/
DUDECONF=$(AVR_TOOLKIT_ROOT)/etc/avrdude.conf
else
BINPATH=""
DUDECONF=$(realpath $(dir $(shell which avrdude))/.. )/etc/avrdude.conf
endif

CC  =$(addprefix $(BINPATH),avr-gcc)
CXX =$(addprefix $(BINPATH),avr-c++)
LD  =$(addprefix $(BINPATH),avr-ld)
CP  =$(addprefix $(BINPATH),avr-objcopy)
OD  =$(addprefix $(BINPATH),avr-objdump)
AS  =$(addprefix $(BINPATH),avr-as)
AR  =$(addprefix $(BINPATH),avr-ar)
DUDE=$(addprefix $(BINPATH),avrdude)
SIZ =$(addprefix $(BINPATH),avr-size)

###############################################################################
# SOURCE FILES
SRCDIR=./
CSRCFILES    = $(shell find $(SRCDIR) -type f -name "*.c"  )
CPPSRCFILES  = $(shell find $(SRCDIR) -type f -name "*.cpp"  )
ASSRCFILES   = $(shell find $(SRCDIR) -type f -name "*.S"  )

# add locations to search for files
vpath %.cpp ./core
vpath %.c ./core
vpath %.cpp ./wire
vpath %.c ./wire/utility
vpath %.c ./wire
vpath %.cpp ./eeprom
vpath %.cpp ./midi
vpath %.cpp ./timerone
vpath %.cpp ./src

# SETUP
OPTIMIZE=s
OBJDIR=./build/
# target binary that can be uploaded to box
BINARY = $(OBJDIR)stocha.hex
MAP    = $(OBJDIR)stocha.map
ELF    =$(OBJDIR)stocha.elf

# Build object files from source...
COBJFILES   = $(addprefix $(OBJDIR),$(notdir $(CSRCFILES:.c=.o)))
CPPOBJFILES = $(addprefix $(OBJDIR),$(notdir $(CPPSRCFILES:.cpp=.o)))
ASOBJFILES  = $(addprefix $(OBJDIR),$(notdir $(ASSRCFILES:.S=.o)))

OBJFILES    = $(COBJFILES) $(CPPOBJFILES) $(ASOBJFILES)

# Project defines
DEFINES = -DF_CPU=16000000L -DARDUINO=10802 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR

# Include directories
INCLUDES += -I"./core"
INCLUDES += -I"./eeprom"
INCLUDES += -I"./midi"
INCLUDES += -I"./pins"
INCLUDES += -I"./timerone"
INCLUDES += -I"./wire"
INCLUDES += -I"./src"

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
	@echo " printfiles : print list of files that would be compiled"

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

.PHONY: printfiles
printfiles:
	@echo "** C FILES **"
	@echo "$(CSRCFILES)"
	@echo ""
	
	@echo "** CPP FILES **"
	@echo "$(CPPSRCFILES)"

	@echo "** S FILES **"
	@echo "$(ASSRCFILES)"	
.PHONY: fw
fw: $(BINARY)

$(ELF): $(OBJFILES)
	$(ECHO) "Linking $@..."
	$(AT)$(CC) $(LDFLAGS) $^ -o $@

$(BINARY): $(ELF)
	$(ECHO) "Creating hex $@..."
	$(AT)$(CP) -O ihex -R .eeprom $^ $@
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
#CFLAGS += -MMD
#CPPFLAGS += -MMD
#ASMFLAGS += -MMD
#-include $(OBJFILES:.o=.d)