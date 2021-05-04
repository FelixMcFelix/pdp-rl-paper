mod config;
mod constants;
mod packet;
mod policy;
mod setup;
mod tiles;

pub use self::{config::*, constants::*, packet::*, policy::*, setup::*, tiles::*};

use byteorder::{BigEndian, WriteBytesExt};
use enum_primitive::*;
use fraction::{Integer, Ratio};
use pnet::{
	datalink::{self, Channel, NetworkInterface},
	packet::{
		ethernet::*,
		ip::IpNextHeaderProtocols,
		ipv4::{self, *},
		udp::{self, *},
		PacketSize,
	},
	util::MacAddr,
};
use rand::Rng;
use serde::{Deserialize, Serialize};
use std::{
	cmp::Ordering,
	io::Result as IoResult,
	net::{Ipv4Addr, SocketAddrV4},
};

pub fn list() {
	println!("{:#?}", datalink::interfaces());
}

pub fn setup<T: Tile>(cfg: &mut SetupConfig<T>) {
	let mut buffer = [0u8; MAX_PKT_SIZE];

	cfg.setup.validate();

	let sz = build_setup_packet(cfg, &mut buffer[..]).expect("Packet building failed...");

	if let Channel::Ethernet(ref mut tx, _rx) = &mut cfg.global.channel {
		tx.send_to(&buffer[..sz], None);
	} else {
		eprintln!("Failed to send packet: no channel found");
	}
}

pub fn tilings(cfg: &mut TilingsConfig) {
	let mut buffer = [0u8; MAX_PKT_SIZE];

	cfg.tiling.validate();

	let sz = build_tilings_packet(cfg, &mut buffer[..]).expect("Packet building failed...");

	if let Channel::Ethernet(ref mut tx, _rx) = &mut cfg.global.channel {
		tx.send_to(&buffer[..sz], None);
	} else {
		eprintln!("Failed to send packet: no channel found");
	}
}

pub fn insert_policy<T: Tile>(cfg: &mut PolicyConfig<T>) {
	let mut buffer = [0u8; MAX_PKT_SIZE];

	// cfg.policy.validate();

	println!("{:?}", PolicyBoundaries::compute(&cfg.setup, &cfg.tiling));

	let (base, offsets) = build_policy_packet_base(&cfg, &mut buffer[..]).unwrap();

	match cfg.policy.take().unwrap() {
		Policy::Float { data, quantiser } => match data {
			PolicyFormat::Sparse(s) => {
				let ps = s.iter().map(|entry| SparsePolicyEntry {
					offset: entry.offset,
					data: entry
						.data
						.iter()
						.map(|val| T::from_float(quantiser * val))
						.collect(),
				});
				write_and_send_sparse_policy(cfg, &mut buffer, base, ps, &offsets);
			},
			PolicyFormat::Dense(d) => {
				let ps = d.iter().map(|val| T::from_float(quantiser * val));
				write_and_send_policy(cfg, &mut buffer, base, ps, &offsets);
			},
		},
		Policy::Quantised {
			data: PolicyFormat::Sparse(s),
		} => {
			write_and_send_sparse_policy(cfg, &mut buffer, base, s.into_iter(), &offsets);
		},
		Policy::Quantised {
			data: PolicyFormat::Dense(d),
		} => {
			write_and_send_policy(cfg, &mut buffer, base, d.into_iter(), &offsets);
		},
	}
}

pub fn send_state<T: Tile>(cfg: &mut SendStateConfig, state: Vec<T>) {
	let mut buffer = [0u8; MAX_PKT_SIZE];

	let sz = build_state_packet(cfg, &mut buffer[..], state).expect("Packet building failed...");

	if let Channel::Ethernet(ref mut tx, _rx) = &mut cfg.global.channel {
		tx.send_to(&buffer[..sz], None);
	} else {
		eprintln!("Failed to send packet: no channel found");
	}
	// write_and_send_state
}

pub fn generate_policy<T: Tile + Serialize>(cfg: &FakePolicyGeneratorConfig<T>) {
	let mut tiles: usize = 0_usize;

	let a = cfg.setup.n_actions as usize;
	let s = cfg.setup.tilings_per_set as usize;

	let n = cfg.setup.tiles_per_dim as usize;

	let mut bias = false;

	for tiling in cfg.tiling.tilings.iter() {
		if tiling.location.is_none() {
			bias = true;
		} else {
			tiles += n.pow(tiling.dims.len() as u32);
			// println!("{:?}", tiles);
		}
	}

	let cap = (a * s * tiles) + if bias { a } else { 0 };

	// let mut policy_data = vec![0; cap];

	let policy_data: Vec<T> = (0..cap)
		.map(|i| T::from_int(((i % 20) as i32) - 10))
		.collect();

	let policy = Policy::Quantised {
		data: PolicyFormat::Dense(policy_data),
	};

	println!("{}", serde_json::to_string_pretty(&policy).unwrap());
}

pub fn generate_state<T: Tile + Serialize>(cfg: &FakeStateGeneratorConfig<T>) {
	let mut out = Vec::with_capacity(cfg.n_dims as usize);

	let mut r = rand::thread_rng();

	for (min, max) in cfg.mins.iter().zip(cfg.maxes.iter()) {
		let w_min = min.wideint();
		let w_max = max.wideint();
		// You never know...
		let true_min = w_min.min(w_max);
		let true_max = w_max.max(w_min);
		out.push(T::from_int(r.gen_range(true_min, true_max)));
	}

	println!("{}", serde_json::to_string_pretty(&out).unwrap());
}

pub fn send_reward<T: Tile>(cfg: &mut SendRewardConfig, value_loc: i32, reward: f32, shift: usize) {
	let mut buffer = [0u8; MAX_PKT_SIZE];

	let quant_reward = T::from_float(reward * ((1 << shift) as f32));

	let sz = build_reward_packet(cfg, &mut buffer[..], quant_reward, value_loc)
		.expect("Packet building failed...");

	if let Channel::Ethernet(ref mut tx, _rx) = &mut cfg.global.channel {
		tx.send_to(&buffer[..sz], None);
	} else {
		eprintln!("Failed to send packet: no channel found");
	}
}
