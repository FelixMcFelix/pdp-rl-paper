use super::*;

use control::{PolicyConfig, SendRewardConfig};
use rand::Rng;

pub fn verify_experiment(config: &VerifyConfig, if_name: &str) {
	let mut global_base = GlobalConfig::new(Some(if_name));
	let global = &mut global_base;

	let mut proto = ProtoSetup::default();
	proto.epsilon = Some(0.0);

	let mut setup: Setup<i32> = proto.instantiate(true);
	prime_setup_with_timings(&mut setup, &TimeBreakdown::UpdateAll);

	let tiling = crate::generate_tiling(&setup, 28);
	setup.limit_workers = None;

	let transport = config.transport_cfg;

	let mut state_rng = ChaChaRng::seed_from_u64(config.sample_seed);

	let policy = if let Some(policy_seed) = config.policy_seed {
		unimplemented!()
	} else {
		control::Policy::Quantised {
			data: control::PolicyFormat::Dense(vec![0; control::policy_size(&setup, &tiling)]),
		}
	};

	let shift = setup.quantiser_shift;

	let mut setup_cfg = SetupConfig {
		global,
		transport,
		setup: setup.clone(),
	};

	control::setup::<i32>(&mut setup_cfg);

	control::tilings(&mut TilingsConfig {
		global,
		tiling: tiling.clone(),
		transport,
	});

	// TODO: send policy packets
	control::insert_policy::<i32>(&mut PolicyConfig {
		global,
		transport,

		policy: Some(policy),
		tiling: tiling.clone(),
		setup: setup.clone(),
	});

	std::thread::sleep(Duration::from_millis(20));

	let mut host_impl = Parsa::new(setup.clone(), &tiling, 1);

	for i in 0..config.n_samples {
		// take state vector
		let mut v = crate::generate_state(&setup, &mut state_rng);
		v[0] = 15;

		let reward_measure = state_rng.gen_range(-1.0..=1.0);

		if i < config.first_sample {
			continue;
		}

		// reward value handling
		let q_reward = i32::from_float_with_quantiser(reward_measure, shift);
		host_impl.reward(1, q_reward);
		control::send_reward::<i32>(
			&mut SendRewardConfig { global, transport },
			1,
			reward_measure,
			shift.into(),
		);
		std::thread::sleep(Duration::from_millis(2));

		// send to nic.
		control::send_state::<i32>(&mut SendStateConfig { global, transport }, v.clone());

		// do local
		let choice = host_impl.act(&v);

		std::thread::sleep(Duration::from_millis(200));

		// verify all.
		for byte_src in BlockDataSource::all() {
			let bytes = super::get_nfp_mem(config, byte_src);
			if !host_impl.verify(byte_src, &bytes, choice) {
				panic!("ITER {}: Mismatch in {:?}!", i, byte_src);
			}
		    std::thread::sleep(Duration::from_millis(200));
		}
	}

	println!("PASSED!");
}
