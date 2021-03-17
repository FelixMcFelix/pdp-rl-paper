import shutil
import subprocess
import os

fw_path_base = "fws/"
wc_header_file_to_write = "worker_config_marker.h"
tile_header_file_to_write = "tile_marker.h"
build_bat = "rl-test_rebuild.bat"
fw_to_copy = "rl-test.nffw"

cwd = os.getcwd()

# nffw_ld_cmd = r"""C:\NFP_SDK_6.1.0-preview\bin\nfld.exe"""
nffw_ld_cmd = [
	r"C:\NFP_SDK_6.1.0-preview\bin\nfld.exe",
	"-chip", "AMDA0078-0012:0",
	"-u", "i4.me0",
	"-l", cwd + r"\out\nfd_pcie0_pci_out_me0.list\nfd_pcie0_pci_out_me0.list",
	"-u", "i4.me1",
	"-l", cwd + r"\out\nfd_pcie0_pci_in_gather.list\nfd_pcie0_pci_in_gather.list",
	"-u", "i4.me2",
	"-l", cwd + r"\out\nfd_pcie0_pci_in_issue0.list\nfd_pcie0_pci_in_issue0.list ",
	"-u", "i4.me3",
	"-l", cwd + r"\out\nfd_pcie0_pci_in_issue1.list\nfd_pcie0_pci_in_issue1.list",
	"-u", "i5.me0",
	"-l", cwd + r"\out\rl_app.list\rl_app.list",
	"-u", "i5.me1",
	"-l", cwd + r"\out\rl_worker.list\rl_worker.list",
	"-u", "i5.me2",
	"-l", cwd + r"\out\rl_worker_alt.list\rl_worker_alt.list",
	"-u", "i5.me3",
	"-l", cwd + r"\out\rl_worker_cap.list\rl_worker_cap.list",
	"-u", "i32.me0",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i32.me1",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i32.me2",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i32.me3",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i32.me4",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i32.me5",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i32.me6",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i32.me7",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i32.me8",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i32.me9",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i32.me10",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i32.me11",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i33.me0",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i33.me1",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i33.me2",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i33.me3",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i33.me4",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i33.me5",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i33.me6",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i33.me7",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i33.me8",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i33.me9",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i33.me10",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i33.me11",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i34.me0",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i34.me1",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i34.me2",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i34.me3",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i34.me4",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i34.me5",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i34.me6",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i34.me7",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i34.me8",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i34.me9",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i34.me10",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i34.me11",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i35.me0",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i35.me1",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i35.me2",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i35.me3",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i35.me4",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i35.me5",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i35.me6",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i35.me7",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i35.me9",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i35.me10",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i35.me11",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i36.me0",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i36.me1",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i36.me2",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i36.me3",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i36.me4",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i36.me5",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i36.me6",
	"-l", cwd + r"\out\nfd_pcie0_notify.list\nfd_pcie0_notify.list",
	"-u", "i36.me7",
	"-l", cwd + r"\out\nfd_pcie0_pd1.list\nfd_pcie0_pd1.list",
	"-u", "i36.me8",
	"-l", cwd + r"\out\nfd_pcie0_sb.list\nfd_pcie0_sb.list",
	"-u", "i36.me9",
	"-l", cwd + r"\out\nfd_pcie0_pd0.list\nfd_pcie0_pd0.list",
	"-u", "i36.me11",
	"-l", cwd + r"\out\app_master.list\app_master.list",
	"-u", "i37.me0",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i37.me1",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i37.me2",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i37.me3",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i37.me4",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i37.me5",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i37.me6",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i37.me7",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i37.me8",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i37.me9",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i37.me10",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i37.me11",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i38.me0",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i38.me1",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i38.me2",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i38.me3",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i38.me4",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i38.me5",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i38.me6",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i38.me7",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i38.me8",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i38.me9",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i38.me10",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i38.me11",
	"-l", cwd + r"\out\pif_app_nfd.list\pif_app_nfd.list",
	"-u", "i48.me0",
	"-l", cwd + r"\out\gro0.list\gro0.list",
	"-u", "i48.me1",
	"-l", cwd + r"\out\gro1.list\gro1.list",
	"-u", "i48.me2",
	"-l", cwd + r"\out\nfd_svc.list\nfd_svc.list",
	"-u", "i48.me3",
	"-l", cwd + r"\out\blm0.list\blm0.list",
	"-u", "i49.me0",
	"-l", cwd + r"\out\flowcache_timeout_emu0.list\flowcache_timeout_emu0.list",
	"-u", "i49.me1",
	"-l", cwd + r"\out\flowcache_timeout_emu1.list\flowcache_timeout_emu1.list",
	"-u", "i49.me3",
	"-l", cwd + r"\out\blm1.list\blm1.list",
	"-L", cwd + r"\out\nbi_init_csr.list\nbi_init_csr.list",
	"-g",
	"-map", cwd + r"\rl-test_.map",
	"-user_note_f", "build_info_json", cwd + r"\out\build_info.json",
	"-user_note_f", "pif_debug_json", cwd + r"\out\pif_debug.json",
	"-user_note_f", "pif_design_json", cwd + r"\out\pif_design.json",
	"-i", "i8",
	"-e", r"C:\NFP_SDK_6.1.0-preview\components\standardlibrary\picocode\nfp6000\catamaran\catamaran.npfw",
	"-i", "i9",
	"-e", r"C:\NFP_SDK_6.1.0-preview\components\standardlibrary\picocode\nfp6000\catamaran\catamaran.npfw",
	"-o", cwd + r"\rl-test.nffw",
	"-mip",
	"-rtsyms",
	"-emu_cache_as_ddr.i26"
]

map_from_labels = {
	"singlethread": "#define _RL_CONFIG_SOLO_INDEP_RL_CORES",
	"multithread": "#define _RL_CONFIG_SOLO_REMOTE_WORK",

	"chunk": "#define WORK_ALLOC_STRAT ALLOC_CHUNK",
	"chunk_rand": "#define WORK_ALLOC_RANDOMISE\n#define WORK_ALLOC_STRAT ALLOC_CHUNK",
	"stride": "#define WORK_ALLOC_STRAT ALLOC_STRIDE",
	"stride_off": "#define WORK_ALLOC_STRAT ALLOC_STRIDE_OFFSET",
}

bit_depths = [8, 16, 32]
thread_strats = [("single", False), ("multi", True)]
work_alloc_schemes = ["chunk", "chunk_rand", "stride", "stride_off"]

def gen_tag_sets():
	out = []
	for bd in bit_depths:
		for threading, use_work_alloc in thread_strats:
			bitname = "{}bit".format(bd)
			tags = [bitname, "{}thread".format(threading)]
			if use_work_alloc:
				for scheme in work_alloc_schemes:
					name = "{}/{}.{}.nffw".format(bitname, threading, scheme)
					new_tags = tags + [scheme]
					out.append((name, set(new_tags)))
			else:
				name = "{}/{}.nffw".format(bitname, threading)
				out.append((name, set(tags)))
	return out

def write_wc_h_file(tags):
	with open(wc_header_file_to_write, "w+") as of:
		of.write("#ifndef _CONFIG_MARKER_H\n#define _CONFIG_MARKER_H\n")
		for k in tags:
			if k in map_from_labels:
				of.write(map_from_labels[k])
				of.write("\n")
		of.write("#endif /* !_CONFIG_MARKER_H_ */")

def write_tile_h_file(tags):
	with open(tile_header_file_to_write, "w+") as of:
		of.write("#ifndef _TILE_MARKER_H\n#define _TILE_MARKER_H\n")
		contents = "#define "
		if "8bit" in tags:
			contents += "_8_BIT_TILES\n"
		elif "16bit" in tags:
			contents += "_16_BIT_TILES\n"
		else:
			contents += "_32_BIT_TILES\n"
		of.write(contents)
		of.write("#endif /* !_TILE_MARKER_H_ */")

def main():
	if not os.path.exists(build_bat):
		print("Could not find main build bat file: {}".format(build_bat))
		print("If not present, in the NFP IDE use 'Build > Export Build Script...'")
		return

	os.makedirs(fw_path_base, exist_ok=True)

	work_sets = gen_tag_sets()
	for name, tags in work_sets:
		write_wc_h_file(tags)
		write_tile_h_file(tags)

		# run the bat
		subprocess.run([build_bat])
		subprocess.run(nffw_ld_cmd)

		final_out_path = fw_path_base + name
		(head, tail) = os.path.split(final_out_path)
		os.makedirs(head, exist_ok=True)
		shutil.copy(fw_to_copy, final_out_path)

		pass

if __name__ == "__main__":
	main()