SRC_DIR = ./src
BUILD_DIR = .

SRC_LIST = $(wildcard $(SRC_DIR)/**/*.gnuplot)

source_data := $(wildcard ../data/**/*.csv)
targets := $(patsubst $(SRC_DIR),$(BUILD_DIR)/,$(patsubst %.gnuplot,%.png,$(SRC_LIST)))

marker: $(targets)
	touch marker

$(BUILD_DIR)/%.png: $(SRC_DIR)/%.gnuplot $(source_data) config/preamble.cfg
	mkdir -p $(BUILD_DIR)
	gnuplot -e "load 'config/preamble.cfg'" $<

clean:
	rm *.png
