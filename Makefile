DRV_RULES_DIR = ./
include $(DRV_RULES_DIR)/suma_rdk_config 
include $(DRV_RULES_DIR)/cmai_inc
NAME = $(notdir $(CURDIR))
LIB_NAME = lib$(NAME).a

# cmai add
CMAI_DIR := /opt/cmai
CMAI_HEAD = $(CMAI_DIR)/include
CMAI_LIB_DIR = $(CMAI_DIR)/lib/ubuntu
CMAI_INCLUDE = $(CMAI_DIR)/include
CMAI_SUBLIB = cmai ipc pthread_base
ADD_SUBLIB_INC = $(foreach n,$(CMAI_SUBLIB),-I$(CMAI_INCLUDE)/$(n))
ADD_SUBLIB = $(foreach n,$(CMAI_SUBLIB),-l$(n))
CMAI_INC = $(CMAI_HEAD)/cmai
PTHREAD_BASE = $(CMAI_HEAD)/pthread_base


# archiver and its options
ARFLAGS = -r

#CFLAGS 
CROSS_COMPILE = $(COMPILE_PREFIX)
EXE_DIR = .

# CFLAGS
CFLAGS		+= -g
CFLAGS		+= -O2
INCDIRS := -I. -I$(COMPILE_INCLUDE) -I$(HEADER_DIR) -I$(CMAI_INC) -I$(PTHREAD_BASE)
INCDIRS += $(ADD_SUBLIB_INC)
CFLAGS += $(INCDIRS) -Wall #-Werror


# LDFLAGS 
LDFLAGS += -lm -lrt
LDFLAGS += -L$(CMAI_LIB_DIR)
LDFLAGS += -lpthread


objects := $(patsubst %.c,%.o,$(wildcard *.c))
#objects += $(patsubst %.c,$(RING_BUFFER_DIR)/%.o,  $(notdir $(RING_BUFFER_SRC)))
executables := $(patsubst %.c,%,$(wildcard *.c))

all: LIB_NAME
LIB_NAME : $(objects)
	$(AR) $(ARFLAGS) $(LIB_NAME) $(objects)
#LIB_NAME : $(objects) 
#	$(CROSS_COMPILE)gcc $(CFLAGS) $(objects) -o $(LIB_NAME)  $(LDFLAGS)


$(objects): %.o: %.c $(HEADERS)
	$(CROSS_COMPILE)gcc -c $(CFLAGS) $(LDFLAGS) $< -o $@

install:
	cp $(LIB_NAME) $(RELEASE_DIR)

clean:
	rm -f *.o *.a
	@for DIR in $(executables); do \
		rm -f $(EXE_DIR)/$$DIR; \
	done

distclean: clean
	rm -f *.o

lint:
	@echo -----------------------------------------------------------
	splint  $(LINTFLAGS) $(INCDIRS) *.c
	
lintw: 
	@echo -----------------------------------------------------------
	-splint -weak $(LINTFLAGS) $(INCDIRS) *.c
 
lintc: 
	@echo -----------------------------------------------------------
	-splint -checks $(LINTFLAGS) $(INCDIRS) *.c
 
lints: 
	@echo -----------------------------------------------------------
	-splint -strict $(LINTFLAGS) $(INCDIRS) *.c
