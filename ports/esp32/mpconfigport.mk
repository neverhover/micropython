# Select the board to build for: if not given on the command line,
# then default to PYBV10.
BOARD ?= HelTec
ifeq ($(wildcard boards/$(BOARD)/.),)
$(error Invalid BOARD specified)
endif

#BUILD ?= build-$(BOARD)

include boards/$(BOARD)/mpconfigboard.mk
$(warning  Radio is $(RADIO), LORA is $(LORA))

# APP_INC
ifeq ($(LORA), 1)
APP_INC += -I$(TOP)/lib
#APP_INC += -I$(TOP)/lib/lora/system/crypto
APP_INC += -I$(TOP)/lib/lora/system
APP_INC += -I$(TOP)/lib/lora/mac
APP_INC += -I$(TOP)/drivers/sx127x
APP_INC += -I$(TOP)/drivers/sx127x/$(RADIO)
APP_INC += -I$(TOP)/ports/esp32/lora
endif

APP_INC += -I$(TOP)/ports/esp32/boards/$(BOARD)

# APP_SRC
ifeq ($(LORA), 1)
APP_RADIO_SRC += $(addprefix drivers/sx127x/$(RADIO)/, \
	$(RADIO).c \
	)

APP_LORA_SRC += $(addprefix ./lora/, \
	utilities.c \
	timer-board.c \
	gpio-board.c \
	spi-board.c \
	$(RADIO)-board.c \
	board.c \
	)

APP_LORA_LIB_SRC += $(addprefix lib/lora/, \
	mac/LoRaMac.c \
	mac/LoRaMacCrypto.c \
	system/delay.c \
	system/gpio.c \
	system/timer.c \
	system/lora_crypto/aes.c \
	system/lora_crypto/cmac.c \
	)

APP_LORA_TEST_SRC += $(addprefix ./tests/, \
	lora_ping_pong.c \
	)

APP_LORA_MOD_SRC +=  \
	modlora.c \

endif

# APP_OBJ
ifeq ($(LORA), 1)
APP_OBJ += $(addprefix $(BUILD)/, \
	$(APP_LORA_LIB_SRC:.c=.o) \
	$(APP_RADIO_SRC:.c=.o) \
	$(APP_LORA_SRC:.c=.o) \
	$(APP_LORA_MOD_SRC:.c=.o) \
	$(APP_LORA_TEST_SRC:.c=.o) \
	)
endif