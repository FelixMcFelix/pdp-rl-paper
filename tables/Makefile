SRC_DIR = src
BUILD_DIR = build

SRC_LIST = $(wildcard $(SRC_DIR)/**/*.py)

source_data := $(wildcard ../results/**/*.csv)
tables := $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(patsubst %.py,%.tex,$(SRC_LIST)))

.SECONDARY:

.PHONY: plots

tables: $(targets)

$(BUILD_DIR)/%.tex: $(SRC_DIR)/%.py
	mkdir -p $(dir $@)
	python $< > $@

cleanall:
	rm -rf $(BUILD_DIR)
