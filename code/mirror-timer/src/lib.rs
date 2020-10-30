mod config;
mod constants;
mod packet;

pub use self::{config::*, constants::*, packet::*};

use byteorder::{BigEndian, WriteBytesExt};
use enum_primitive::*;
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
use std::{
	net::{Ipv4Addr, SocketAddrV4},
	time::Instant,
};

pub fn list() {
	println!("{:#?}", datalink::interfaces());
}

pub fn run(cfg: &mut Config) {
	let mut measurements = vec![];
	let mut buffer = [0u8; MAX_PKT_SIZE];

	let (cursor, offsets) = build_transport(cfg.global, &cfg.transport, &mut buffer);

	for i in 0..cfg.iterations {
		let l_cursor = cursor + {
			let mut body = &mut buffer[cursor..];
			let space_start = body.len();

			body.write_u64::<BigEndian>(i)
				.expect("Eh?");
			let space_end = body.len();

			space_start - space_end
		};

		finalize(&mut buffer[..l_cursor], &offsets, &cfg.transport);

		// Now send.

		if let Channel::Ethernet(ref mut tx, ref mut rx) = &mut cfg.global.channel {
			let t = Instant::now();
			tx.send_to(&buffer[..l_cursor], None);

			let _bytes = rx.next().expect("Argh?!");
			let elapsed = t.elapsed();

			measurements.push(elapsed.as_nanos());
		} else {
			eprintln!("Failed to send packet: no channel found");
		}
	}

	println!("{:?}", measurements);
}
