# Marl Quantisation testing

Modification of [past work](https://github.com/FelixMcFelix/rln-dc-ddos-paper) to examine the effects of different float->integer quantisation schemes on learning and accuracy, to motivate the PDP design in the rest of the paper.

# Preparation
TODO.
* Make sure all modules are init'd.
* `touch marl/tilecoding/__init__.py`

# Scripts producing experimental data:
* `marl/quant_runner.py`
 * `python quant_runner.py 0 0`
* `marl/qt-tput.sh`
 * `./qt-tput.sh`
 * This is run on 3 machines, named:
  * `beast1`: *Collector* in the paper.
  * `beast2`: *Collector* with a modern kernel.
  * `perlman`: *MidServer* in the paper.
 * This does not require the OVS setup as above.

# Scripts processing experimental data:
TODO.
