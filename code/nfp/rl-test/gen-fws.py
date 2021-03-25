import shutil
import subprocess
import os

fw_path_base = "fws/"
wc_header_file_to_write = "worker_config_marker.h"
tile_header_file_to_write = "tile_marker.h"
build_bat = "rl-test_rebuild.bat"
fw_to_copy = "rl-test.nffw"
design_to_copy = "out/pif_design.json"

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

nfcc_pif_command = [
	r"C:\NFP_SDK_6.1.0-preview\bin\nfcc.exe",
	"-chip", "AMDA0078-0012:0",
	"-Qrevision_min=16",
	"-ng," r"C:\NFP_SDK_6.1.0-preview\components\standardlibrary\microc\src\libc.c",
	r"C:\NFP_SDK_6.1.0-preview\components\standardlibrary\microc\src\rtl.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\apps\common\src\fc.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\hashmap\src\camp_hash.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\apps\common\src\pif_app_nfd.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\apps\common\src\pkt_clone.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\apps\common\src\mcast.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\apps\common\src\pif_app_controller.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\cflow_tstamp\src\cflow_tstamp.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\apps\common\src\dcfl_init.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\apps\common\src\system_init.c",
	r"C:\NFP_SDK_6.1.0-preview\components\flowenv\me\lib\pktio\libpktio.c",
	r"C:\NFP_SDK_6.1.0-preview\components\ng_nfd\me\blocks\vnic\libnfd.c",
	r"C:\NFP_SDK_6.1.0-preview\components\flowenv\me\blocks\blm\libblm.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\macflush\libmacflushuser.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_design.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_parrep.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_actions.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_global.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_ctlflow.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_tables.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_deparse.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_digests.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_meters.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_registers.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_pkt_recurse.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_pkt_clone.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_flcalc.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\dcfl\me\lib\dcfl\libdcfl.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\gro\libgro.c",
	r"C:\NFP_SDK_6.1.0-preview\components\flowenv\me\lib\nfp\libnfp.c",
	r"C:\NFP_SDK_6.1.0-preview\components\flowenv\me\lib\std\libstd.c",
	r"C:\NFP_SDK_6.1.0-preview\components\flowenv\me\lib\pkt\libpkt.c",
	r"C:\NFP_SDK_6.1.0-preview\components\flowenv\me\lib\pktdma\libpktdma.c",
	r"C:\NFP_SDK_6.1.0-preview\components\flowenv\me\lib\net\libnet.c",
	r"C:\NFP_SDK_6.1.0-preview\components\flowenv\me\lib\modscript\libmodscript.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\pif\src\mac_time_user.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\pif\src\pif_action_common.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\pif\src\pif_meter.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\pif\src\pif_lookup.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\pif\src\pif_pkt.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\pif\src\pif_counters.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\pif\src\pif_init.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\pif\src\pif_flcalc_algorithms.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\pif\src\pif_memops.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\flowcache\me\lib\flowcache\_c\flow_cache.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\flowcache\me\lib\flowcache\_c\flow_cache_remove.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\flowcache\me\lib\flowcache\_c\flow_cache_timestamp.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\flowcache\me\lib\flowcache\_c\flow_cache_lock.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\flowcache\me\lib\standardlibrary\_c\mem_atomic_indirect.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\flowcache\me\lib\standardlibrary\_c\mem_cam_add_inc.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\flowcache\me\lib\standardlibrary\_c\cluster_target.c",
	r"C:\NFP_SDK_6.1.0-preview\p4\components\flowcache\me\lib\standardlibrary\_c\cam.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\rl-test.c",
	r"C:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\rl-pkt-store.c",
	"-DPLATFORM_PORT_SELECTION=SF_PLATFORM",
	"-DPIF_APP_CONFIG=PIF_APP_DEFAULT",
	"-DNUM_IMU=2",
	"-DNUM_EMU=2",
	"-DPCIE_ISL_CNT=2",
	"-DSIMULATION=0",
	"-DPIF_APP_REDUCED_THREADS=1",
	"-DPIF_DEBUG=1",
	"-DPIF_APP_MASTER_ME=0x204",
	"-DHASHMAP_CAMPHASH",
	"-O2", "-Ob1", "-W3",
	r"-IC:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\apps\common\include",
	r"-IC:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\shared\include",
	r"-IC:\NFP_SDK_6.1.0-preview\p4\components\flowcache\me\lib\flowcache",
	r"-IC:\NFP_SDK_6.1.0-preview\p4\components\flowcache\me\lib\standardlibrary",
	r"-IC:\NFP_SDK_6.1.0-preview\p4\components\dcfl\me\lib\dcfl",
	r"-IC:\NFP_SDK_6.1.0-preview\p4\components\dcfl\shared\include\dcfl",
	r"-IC:\NFP_SDK_6.1.0-preview\components\flowenv\me\include",
	r"-IC:\NFP_SDK_6.1.0-preview\components\flowenv\me\include\nfp6000",
	r"-IC:\NFP_SDK_6.1.0-preview\components\flowenv\me\lib",
	r"-IC:\NFP_SDK_6.1.0-preview\components\flowenv\me\lib\nfp",
	r"-IC:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out",
	r"-IC:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\callbackapi",
	r"-IC:\NFP_SDK_6.1.0-preview\components\flowenv\me\blocks\blm\_h",
	r"-IC:\NFP_SDK_6.1.0-preview\components\flowenv\me\blocks",
	r"-IC:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\apps\pif_app_nfd\include",
	r"-IC:\NFP_SDK_6.1.0-preview\components\ng_nfd\me\blocks",
	r"-IC:\NFP_SDK_6.1.0-preview\components\ng_nfd\shared",
	r"-IC:\NFP_SDK_6.1.0-preview\components\ng_nfd",
	r"-IC:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\hashmap\include",
	r"-IC:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\cflow_tstamp\include",
	r"-IC:\NFP_SDK_6.1.0-preview\components\ng_nfd\me\lib ",
	r"IC:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib",
	r"-IC:\NFP_SDK_6.1.0-preview\p4\components\gro",
	r"-IC:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\pif\include",
	r"-IC:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\lib\macflush",
	r"-IC:\NFP_SDK_6.1.0-preview\components\standardlibrary\include",
	r"-IC:\NFP_SDK_6.1.0-preview\components\standardlibrary\microc\src",
	r"-IC:\NFP_SDK_6.1.0-preview\components\standardlibrary\microc\include",
	"-FI", r"C:\NFP_SDK_6.1.0-preview\p4\components\nfp_pif\me\apps\pif_app_nfd\include\config.h",
	"-Qnctx=4", "-Qspill=3", "-Qnctx_mode=4", "-Qnn_mode=1", "-Qlm_start=0", "-Qlm_size=1024",
	"-Zi",
	r"-FoC:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_app_nfd.list",
	r"-FeC:\Users\Kyle\gits\pdp-rl-paper\code\nfp\rl-test\out\pif_app_nfd.list\pif_app_nfd.list",
	"-Qno_decl_volatile", "-mIPOPT_nfp=2", "-ng"
]

map_from_labels = {
	"singlethread": "#define _RL_CONFIG_SOLO_INDEP_RL_CORES",
	"multithread": "#define _RL_CONFIG_SOLO_REMOTE_WORK",

	"chunk": "#define WORK_ALLOC_STRAT ALLOC_CHUNK",
	"chunk_rand": "#define WORK_ALLOC_RANDOMISE\n#define WORK_ALLOC_STRAT ALLOC_CHUNK",
	"stride": "#define WORK_ALLOC_STRAT ALLOC_STRIDE",
	"stride_off": "#define WORK_ALLOC_STRAT ALLOC_STRIDE_OFFSET",
}

bit_depths = [
	8,
	16,
	32
]
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
					name = "{}/{}.{}".format(bitname, threading, scheme)
					new_tags = tags + [scheme]
					out.append((name, set(new_tags)))
			else:
				name = "{}/{}".format(bitname, threading)
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
		# subprocess.run(nfcc_pif_command)
		subprocess.run(nffw_ld_cmd)

		final_out_path = fw_path_base + name + ".nffw"
		(head, tail) = os.path.split(final_out_path)
		os.makedirs(head, exist_ok=True)
		shutil.copy(fw_to_copy, final_out_path)

		final_out_path = fw_path_base + name + ".json"
		shutil.copy(design_to_copy, final_out_path)

if __name__ == "__main__":
	main()