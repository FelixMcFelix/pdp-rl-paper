mod config;
mod constants;
mod packet;
mod policy;
mod setup;
mod tiles;

pub use self::{
	config::*,
	constants::*,
	packet::*,
	policy::*,
	setup::*,
	tiles::*,
};

use byteorder::{
	BigEndian,
	WriteBytesExt,
};
use enum_primitive::*;
use fraction::{
	Integer,
	Ratio,
};
use pnet::{
	datalink::{
		self,
		Channel,
		NetworkInterface,
	},
	packet::{
		ethernet::*,
		ip::IpNextHeaderProtocols,
		ipv4::{self, *},
		udp::{self, *},
		PacketSize,
	},
	util::MacAddr,
};
use serde::{Deserialize, Serialize};
use std::{
	cmp::Ordering,
	io::Result as IoResult,
	net::{
		Ipv4Addr,
		SocketAddrV4,
	},
};

pub fn list() {
	println!("{:#?}", datalink::interfaces());
}

pub fn setup(cfg: &mut SetupConfig) {
	let mut buffer = [0u8; MAX_PKT_SIZE];

	cfg.setup.validate();

	let sz = build_setup_packet(cfg, &mut buffer[..])
		.expect("Packet building failed...");

	if let Channel::Ethernet(ref mut tx, _rx) = &mut cfg.global.channel {
		tx.send_to(&buffer[..sz], None);
	} else {
		eprintln!("Failed to send packet: no channel found");
	}
}

pub fn tilings(cfg: &mut TilingsConfig) {
	let mut buffer = [0u8; MAX_PKT_SIZE];

	cfg.tiling.validate();

	let sz = build_tilings_packet(cfg, &mut buffer[..])
		.expect("Packet building failed...");

	if let Channel::Ethernet(ref mut tx, _rx) = &mut cfg.global.channel {
		tx.send_to(&buffer[..sz], None);
	} else {
		eprintln!("Failed to send packet: no channel found");
	}
}

pub fn insert_policy(cfg: &mut PolicyConfig) {
	let mut buffer = [0u8; MAX_PKT_SIZE];

	// cfg.policy.validate();

	let (base, offsets) = build_policy_packet_base(&cfg, &mut buffer[..]).unwrap();

	match cfg.policy.take().unwrap() {
		Policy::Float{data, quantiser} => {
			match data {
				PolicyFormat::Sparse(s) => {
					let ps = s
						.iter()
						.map(|entry| SparsePolicyEntry {
							offset: entry.offset,
							data: entry.data.iter().map(|val| (quantiser * val) as i32).collect()
						});
					write_and_send_sparse_policy(cfg, &mut buffer, base, ps, &offsets);
				},
				PolicyFormat::Dense(d) => {
					let ps = d
						.iter()
						.map(|val| (quantiser * val) as i32);
					write_and_send_policy(cfg, &mut buffer, base, ps, &offsets);	
				},
			}
		},
		Policy::Quantised{data: PolicyFormat::Sparse(s)} => {
			write_and_send_sparse_policy(cfg, &mut buffer, base, s.into_iter(), &offsets);
		},
		Policy::Quantised{data: PolicyFormat::Dense(d)} => {
			write_and_send_policy(cfg, &mut buffer, base, d.into_iter(), &offsets);
		},
	}
}

// #[derive(Debug, Serialize, Deserialize)]
// pub struct Setup {
// 	pub n_dims: u16,

// 	pub tiles_per_dim: u16,

// 	pub tilings_per_set: u16,

// 	pub n_actions: u16,

// 	pub epsilon: RatioDef<Tile>,

// 	pub alpha: RatioDef<Tile>,

// 	pub epsilon_decay_amt: Tile,

// 	pub epsilon_decay_freq: u32,

// 	pub maxes: Vec<Tile>,

// 	pub mins: Vec<Tile>,
// }

// #[derive(Debug, Serialize, Deserialize)]
// pub struct TilingSet {
// 	pub tilings: Vec<Tiling>,
// }

// #[derive(Debug, Serialize, Deserialize)]
// pub struct Tiling {
// 	pub dims: Vec<u16>,
// 	pub location: Option<u8>,
// }

// pub struct FakePolicyGeneratorConfig {
// 	pub tiling: TilingSet,
// 	pub setup: Setup,
// }

pub fn generate_policy(cfg: &FakePolicyGeneratorConfig) {
	let mut tiles: usize = 0 as usize;

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

	let cap = (a * s * tiles) + if bias {
		a
	} else {
		0
	};

	let mut policy_data = vec![0; cap];

	for (i, val) in policy_data[0..].iter_mut().enumerate() {
		*val = ((i as Tile) % 20) - 10;
	}

	let policy = Policy::Quantised{ data: PolicyFormat::Dense(policy_data) };

	println!("{}", serde_json::to_string_pretty(&policy).unwrap());
}
