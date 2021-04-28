import numpy as np
import os
import platform
from quantiser import *
from sarsa import SarsaLearner, QLearner
import sys
from spf import *
import time

machine_name = platform.node()
results_dir = "../../../results/host-rl-perf-tput/raw/"
pquantisers = [
	None,
	("i8", np.dtype("i1"), Quantiser.binary(5)),
	("i16", np.dtype("i2"), Quantiser.binary(11)),
	("i32", np.dtype("i4"), Quantiser.binary(24)),
]

warmup_len = 2.0
iters_to_run = 10.0
cooldown_len = 2.0

# Hacked config code taken from marl.py...

def marlParams(
		linkopts = {
			#"bw": 10,
			"delay": 10,
		},
		n_teams = 1,

		# per-team options
		n_inters = 2,
		n_learners = 3,
		host_range = [2, 2],

		calc_max_capacity = None,

		P_good = 0.6,
		good_range = [0, 1],
		evil_range = [2.5, 6],
		good_file = "../../data/pcaps/bigFlows.pcap",
		bad_file = None,

		topol = "tree",
		ecmp_servers = 8,
		ecmp_k = 4,

		explore_episodes = 80000,
		episodes = 1000,#100000
		episode_length = 5000,#1000
		separate_episodes = False,

		max_bw = None,
		pdrop_magnitudes = [0.1*n for n in xrange(10)],

		alpha = 0.05,
		epsilon = 0.3,
		discount = 0,
		break_equal = None,

		algo = "sarsa",
		trace_decay = 0.0,
		trace_threshold = 0.0001,
		use_path_measurements = True,

		model = "tcpreplay",
		submodel = None,
		rescale_opus = False,
		mix_model = None,

		use_controller = False,
		moralise_ips = True,

		dt = 0.001,

		# These should govern tc on the links at various points in the network.
		#
		# Final hop is just what you'd think, old_style governs
		# whether sender rates are mediated by tcpreplay or by tc on the link
		# over from said host.
		old_style = False,
		force_host_tc = False,
		protect_final_hop = True,

		with_ratio = False,
		override_action = None,
		manual_early_limit = None,
		estimate_const_limit = False,

		rf = "ctl",

		rand_seed = 0xcafed00d,
		rand_state = None,
		force_cmd_routes = False,

		rewards = [],
		good_traffic_percents = [],
		total_loads = [],
		store_sarsas = [],
		action_comps = [],

		reward_direction = "in",
		state_direction = "in",
		actions_target_flows = False,

		bw_mon_socketed = False,
		unix_sock = True,
		print_times = False,
		# Can set to "no", "udp", "always"
		prevent_smart_switch_recording = "udp",
		# FIXME: these two flags ae incompatible
		record_times = False,
		record_deltas_in_times = False,

		contributors = [],
		restrict = None,

		single_learner = False,
		single_learner_ep_scale = True,

		spiffy_mode = False,

		randomise = False,
		randomise_count = None,
		randomise_new_ip = False,

		split_codings = False,
		extra_codings = [],
		feature_max = 12,
		combine_with_last_action = [12, 13, 14, 15, 16, 17, 18, 19],
		strip_last_action = True,

		explore_feature_isolation_modifier = 1.0,
		explore_feature_isolation_duration = 5,
		always_include_global = True,
		always_include_bias = True,

		trs_maxtime = None,
		reward_band = 1.0,

		spiffy_but_bad = False,
		spiffy_act_time = 5.0,
		# 24--192 hosts, pick accordingly.
		spiffy_max_experiments = 16,
		spiffy_min_experiments = 1,
		spiffy_pick_prob = 0.2,
		spiffy_drop_rate = 0.15,
		spiffy_traffic_dir = "in",
		spiffy_mbps_cutoff = 0.1,
		spiffy_expansion_factor = 5.0,

		broken_math = False,
		num_drop_groups = 20,

		# New stuff here. wrt. Quantisation.
		do_quant_testing = False,
		quantisers = [],
		quant_results_needed = 10000,
		quant_iter_start = None,

		train_using_quant = None,

		sarsa_pairs_to_export = None,
	):
	if max_bw is None:
		max_bw = 192.0
	aset = pdrop_magnitudes
	AcTrans = MarlMachine
	sarsaParams = {
		"max_bw": max_bw,
		"vec_size": 4,
		"actions": aset,
		"epsilon": epsilon,
		"learn_rate": alpha,
		"discount": discount,
		"break_equal": break_equal,
		# "tile_c": 16,
		# "tilings_c": 3,
		# "default_q": 0,
		"epsilon_falloff": explore_episodes * episode_length,
		"AcTrans": AcTrans,
		"trace_decay": trace_decay,
		"trace_threshold": trace_threshold,
		"broken_math": broken_math,
		"always_include_bias": always_include_bias,
		"quantiser": train_using_quant,
	}

	sarsaParams["extended_mins"] = [
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, -50.0, -50.0,
		0.0, 0.0,
		0.0, 0.0,
		0.0, 0.0,
		-50.0, -50.0,
	][0:feature_max-4]

	sarsaParams["extended_maxes"] = [
		4294967296.0, 1.0, 2000.0, float(10 * (1024 ** 2)), 1.0,
		10000.0, 50.0, 50.0,
		7000.0, 7000.0,
		2000.0, 2000.0,
		1560.0, 1560.0,
		50.0, 50.0,
	][0:feature_max-4]

	#sarsaParams["vec_size"] += len(sarsaParams["extended_maxes"])

	if not split_codings:
		sarsaParams["tc_indices"] = [np.arange(sarsaParams["vec_size"])]
	else:
		sarsaParams["tc_indices"] = [np.arange(4)] + [[i] for i in xrange(4, sarsaParams["vec_size"] + len(sarsaParams["extended_maxes"]))]

		# Okay, index of last_action is 2 after the last global datum in the feature vector.
		# combine_with_last_action, strip_last_action, use_path_measurements
		# Note: its index in the feature vector is 5, its position in the list of tc indices is 2.
		if combine_with_last_action is not None:
			for index in combine_with_last_action:
				# subtract (global_actions - 1) to move to correct position
				f_index = index - 3
				if f_index < len(sarsaParams["tc_indices"]):
					sarsaParams["tc_indices"][f_index] += [5]

		if strip_last_action:
			del sarsaParams["tc_indices"][2]

	sarsaParams["tc_indices"] += extra_codings

	return sarsaParams

args = sys.argv
task_ct = int(args[1])
task_i = int(args[2])
quantiser_i = int(args[3])
act_or_update = int(args[4])
repetition = int(args[5])

possible_quantiser = pquantisers[quantiser_i]

params = {
	#"P_good": 1.0,
	"n_teams": 2,
	"n_inters": 2,
	"n_learners": 3,

	"alpha": 0.05,
	"epsilon": 0.2,
	"discount": 0.0,
	"dt": 0.05,

	"ecmp_servers": 2,

	"explore_episodes": 0.8,
	"episodes": 10,
	"episode_length": 10000,
	"separate_episodes": True,

	"rf": "marl",
	"use_controller": True,
	"actions_target_flows": True,
	"trs_maxtime": 0.001,

	"split_codings": True,
	"feature_max": 20,

	# new shiny quantisation stuff.
	"do_quant_testing": True,
	"quantisers": [possible_quantiser],
	"quant_results_needed": 10000,
	"quant_iter_start": 1000,
}

params["host_range"] = [(16, 16)]
params["dt"] = 0.05

sarsaParams = marlParams(**params)
learner = SarsaLearner(**sarsaParams)

timing_measures = []

state_range = [list(np.array(x)/2.0) for x in learner.state_range]

if possible_quantiser is not None:
	(name, qdt, q) = possible_quantiser
	outname = name
	learner = learner.as_quantised(q, dt=qdt)
else:
	outname = "f32"

states = [np.random.uniform(state_range[0],state_range[1]) for i in xrange(1000)]
q_states = [(np.array([learner.quantiser.into(v) for v in s1]) if learner.quantiser is not None else s1) for s1 in states]

# there will
coded_q_states = {i: (learner.to_state(q_states[i]), int(np.random.uniform(0, len(learner.actions))), None) for i in range(len(q_states))}
r = 1.0

last_pair_storage = {}

start_recording = False
end_recording = False

start = time.time()
t = start
end = t + warmup_len + iters_to_run + cooldown_len

start_rec_time = start + warmup_len
end_rec_time = start_rec_time  + iters_to_run

i = 0

do_not_update = act_or_update == 0

while t < end:
	t_before = time.time()

	s1 = learner.to_state_quanted(q_states[i % len(q_states)])

	last_act = (0, 0, None)

	if not do_not_update:
		last_act = coded_q_states[(i + 1) % len(q_states)]

	# ------------

	(new_ac, vals, new_z) = learner.update(
		s1,
		r,
		subs_last_act=last_act,
		decay=True,
		delta_space=None,
		action_narrowing=None,
		update_narrowing=None,
		do_not_update=do_not_update
	)

	last_pair_storage[i % len(q_states)] = (s1, new_ac, new_z)

	t_after = time.time()
	# ------------

	# TODO: need to add in state-action lookup time somehow

	if start_recording and not end_recording:
		timing_measures.append((t_after - t_before) * 1e6)

	i += 1
	t = time.time()
	start_recording = t >= start_rec_time
	end_recording = t >= end_rec_time

if not os.path.exists(results_dir):
	os.makedirs(results_dir)

# task_ct = int(args[1])
# task_i = int(args[2])
# quantiser_i = int(args[3])
# act_or_update = int(args[4])
# repetition = int(args[5])

if act_or_update == 0:
	name_part = "ComputeAndWriteout"
else:
	name_part = "UpdateAll"

# throughput can just be measured by counting rows (lmao)
outpath = "{}{}.{}.{}.{}({}).{}.dat".format(results_dir, outname, machine_name, name_part, task_ct, task_i, repetition)
with open(outpath, "w") as of:
	for val in timing_measures:
		of.write("{:.2f}\n".format(val))

