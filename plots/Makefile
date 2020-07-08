SRC_DIR = src
BUILD_DIR = build

SRC_LIST = $(wildcard $(SRC_DIR)/**/*.gnuplot)

source_data := $(wildcard ../results/**/*.csv)
targets := $(patsubst $(SRC_DIR)/%,$(BUILD_DIR)/%,$(patsubst %.gnuplot,%.pdf,$(SRC_LIST)))

.SECONDARY:

.PHONY: plots clean

plots: $(targets)

$(BUILD_DIR)/%.tex: $(SRC_DIR)/%.gnuplot $(source_data) config/preamble.cfg
	mkdir -p $(dir $@)
	gnuplot -e "load 'config/preamble.cfg'; set output '$@';" $<

$(BUILD_DIR)/%.pdf: $(BUILD_DIR)/%.tex
	#pdflatex -output-directory='$(dir $@)' $<
	lualatex -output-directory='$(dir $@)' $<

clean:
	rm -f $(BUILD_DIR)/**/*.tex
	rm -f $(BUILD_DIR)/**/*.aux
	rm -f $(BUILD_DIR)/**/*.log

cleanall:
	rm -rf $(BUILD_DIR)
