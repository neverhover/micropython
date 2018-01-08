# Select the board to build for: if not given on the command line,
# then default to PYBV10.
BOARD ?= HelTec
ifeq ($(wildcard boards/$(BOARD)/.),)
$(error Invalid BOARD specified)
endif

#BUILD ?= build-$(BOARD)

include boards/$(BOARD)/mpconfigboard.mk

# APP_INC
APP_INC += -Idrivers/sx127x
APP_INC += -Idrivers/sx127x/$(RADIO)
# APP_SRC
APP_RADIO_SRC += $(addprefix drivers/sx127x/$(RADIO)/, \
	$(RADIO).c \
	)

# APP_OBJ
APP_OBJ += $(addprefix $(BUILD)/, \
	$(APP_RADIO_SRC:.c=.o) \
	)