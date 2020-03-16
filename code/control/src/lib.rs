use byteorder::{
	ByteOrder,
	LittleEndian,
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
		DataLinkSender,
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
use std::net::{
	IpAddr,
	Ipv4Addr,
	SocketAddrV4,
};

pub struct GlobalConfig {
	pub iface: NetworkInterface,
	pub channel: Channel,
}

impl GlobalConfig {
	pub fn new(iface_name: Option<&str>) -> Self {
		let iface = datalink::interfaces()
			.into_iter()
			.filter(move |iface: &NetworkInterface| {
				if let Some(name) = iface_name {
					iface.name == name
				} else {
					true
				}
			})
			.next()
			.expect(&format!("Couldn't bind interface \"{}\"", iface_name.unwrap_or("")));

		let channel = datalink::channel(&iface, Default::default())
			.map_err(|e| eprintln!("{:?}", e))
			.expect(&format!("Failed to open channel on \"{}\"", iface_name.unwrap_or("")));

		Self {
			iface,
			channel,
		}
	}
}

pub struct TransportConfig {
	pub src_addr: SocketAddrV4,
	pub dst_addr: SocketAddrV4,
}

impl Default for TransportConfig {
	fn default() -> Self {
		let a = SocketAddrV4::new(Ipv4Addr::new(0, 0, 0, 0), 0);
		Self {
			src_addr: a,
			dst_addr: a,
		}
	}
}

pub struct SetupConfig<'a> {
	pub global: &'a mut GlobalConfig,
	pub setup: Setup,
	pub transport: TransportConfig,
}

impl<'a> SetupConfig<'a> {
	pub fn new(global: &'a mut GlobalConfig) -> Self {
		Self {
			global,
			setup: Default::default(),
			transport: Default::default(),
		}
	}
}

pub fn list() {
	println!("{:#?}", datalink::interfaces());
}

type Tile = i32;

#[derive(Serialize, Deserialize, Debug, Copy, Clone)]
pub struct RatioDef<T: Integer> {
	numer: T,
	denom: T,
}

impl<T> From<RatioDef<T>> for Ratio<T> 
	where T: Integer + Clone {
	fn from(def: RatioDef<T>) -> Ratio<T> {
		Ratio::new(def.numer, def.denom)
	}
}

impl<T> From<Ratio<T>> for RatioDef<T> 
	where T: Integer + Clone {
	fn from(def: Ratio<T>) -> RatioDef<T> {
		RatioDef {
			numer: def.numer().clone(),
			denom: def.denom().clone(),
		}
	}
}

#[derive(Debug, Serialize, Deserialize)]
pub struct Setup {
	pub n_dims: u16,

	pub tiles_per_dim: u16,

	pub tilings_per_set: u16,

	pub epsilon: RatioDef<Tile>,

	pub alpha: RatioDef<Tile>,

	pub epsilon_decay_amt: Tile,

	pub epsilon_decay_freq: u32,

	pub maxes: Vec<Tile>,

	pub mins: Vec<Tile>,
}

impl Default for Setup {
	fn default() -> Self {
		Self {
			n_dims: 0,
			tiles_per_dim: 1,
			tilings_per_set: 1,
			epsilon: Ratio::new(1, 10).into(),
			alpha: Ratio::new(1, 20).into(),
			epsilon_decay_amt: 1,
			epsilon_decay_freq: 1,
			maxes: vec![],
			mins: vec![],
		}
	}
}

impl Setup {
	pub fn validate(&self) {
		assert_eq!(self.n_dims as usize, self.maxes.len());
		assert_eq!(self.n_dims as usize, self.mins.len());

		let e = self.epsilon.numer as f32 / self.epsilon.denom as f32;

		let a = self.alpha.numer as f32 / self.alpha.denom as f32;

		assert!(e >= 0.0 && e <= 1.0);
		assert!(a >= 0.0 && a <= 1.0);

		assert!(self.tiles_per_dim >= 1);
		assert!(self.tilings_per_set >= 1);
	}
}

enum_from_primitive!{
pub enum RlType {
	Config = 0,
}
}

enum_from_primitive!{
pub enum RlConfigType {
	Setup = 0,
	Tiles,
}
}

struct TransportOffsets {
	ip: usize,
	udp: usize,
	data: usize,
}

const RL_DSCP_TRAP: u8 = 0b000011;

fn build_transport(cfg: &GlobalConfig, t_cfg: &TransportConfig, buf: &mut [u8]) -> (usize, TransportOffsets) {
	// FIXME: read from config.
	let src_ip = Ipv4Addr::new(192, 168, 1, 1);
	let dst_ip = Ipv4Addr::new(192, 168, 1, 141);
	let src_port = 16767;
	let dst_port = 16768;

	let mut out = TransportOffsets { ip: 0, udp: 0, data: 0 };

	let mut cursor = 0;
	{
		let mut eth_pkt = MutableEthernetPacket::new(&mut buf[cursor..])
			.expect("Plenty of room...");
		eth_pkt.set_destination(MacAddr::broadcast());
		eth_pkt.set_ethertype(EtherTypes::Ipv4);
		eth_pkt.set_source(cfg.iface.mac_address());

		cursor += eth_pkt.packet_size();
	}

	{
		out.ip = cursor;

		let mut ipv4_pkt = MutableIpv4Packet::new(&mut buf[cursor..])
			.expect("Plenty of room...");
		ipv4_pkt.set_version(4);
		ipv4_pkt.set_header_length(5);
		ipv4_pkt.set_ttl(64);
		ipv4_pkt.set_next_level_protocol(IpNextHeaderProtocols::Udp);
		ipv4_pkt.set_destination(*t_cfg.dst_addr.ip());
		ipv4_pkt.set_source(*t_cfg.src_addr.ip());

		ipv4_pkt.set_dscp(RL_DSCP_TRAP);

		cursor += ipv4_pkt.packet_size();
	}

	{
		out.udp = cursor;

		let mut udp_pkt = MutableUdpPacket::new(&mut buf[cursor..])
			.expect("Plenty of room...");
		udp_pkt.set_source(t_cfg.src_addr.port());
		udp_pkt.set_destination(t_cfg.dst_addr.port());
		// checksum is optional in ipv4
		udp_pkt.set_checksum(0);

		cursor += udp_pkt.packet_size();
	}

	out.data = cursor;

	(cursor, out)
}

fn build_type(buf: &mut [u8], t: RlType) -> usize {
	let mut cursor = 0;

	buf[cursor] = t as u8;
	cursor += 1;

	cursor
}

fn build_config_type(buf: &mut [u8], t: RlConfigType) -> usize {
	let mut cursor = 0;

	buf[cursor] = t as u8;
	cursor += 1;

	cursor
}

fn finalize(buf: &mut [u8], off: &TransportOffsets) {
	{
		let region = &mut buf[off.ip..];
		let len = region.len();
		let mut ipv4_pkt = MutableIpv4Packet::new(region)
			.expect("Plenty of room...");
		ipv4_pkt.set_total_length(len as u16);

		ipv4_pkt.set_checksum(ipv4::checksum(&ipv4_pkt.to_immutable()));
	}

	// FIXME
	let src_ip = Ipv4Addr::new(192, 168, 1, 1);
	let dst_ip = Ipv4Addr::new(192, 168, 1, 141);
	let src_port = 16767;
	let dst_port = 16768;

	{
		let (region, payload) = (&mut buf[off.udp..]).split_at_mut(off.data - off.udp);
		let len = region.len() + payload.len();
		let mut udp_pkt = MutableUdpPacket::new(region)
			.expect("Plenty of room...");
		udp_pkt.set_length(len as u16);

		udp_pkt.set_checksum(udp::ipv4_checksum_adv(
			&udp_pkt.to_immutable(),
			payload,
			&src_ip,
			&dst_ip,
		));
	}


}

fn build_setup_packet(setup: &SetupConfig, buf: &mut [u8]) -> usize {
	let (mut cursor, offsets) = build_transport(&setup.global, &setup.transport, buf);
	cursor += build_type(buf, RlType::Config);
	cursor += build_config_type(buf, RlConfigType::Setup);

	// TODO: write struct out
	{
		let mut body = &mut buf[cursor..];
		let space_start = body.len();

		body.write_u16::<LittleEndian>(setup.setup.n_dims);
		body.write_u16::<LittleEndian>(setup.setup.tiles_per_dim);
		body.write_u16::<LittleEndian>(setup.setup.tilings_per_set);

		body.write_i32::<LittleEndian>(setup.setup.epsilon.numer);
		body.write_i32::<LittleEndian>(setup.setup.epsilon.denom);

		body.write_i32::<LittleEndian>(setup.setup.alpha.numer);
		body.write_i32::<LittleEndian>(setup.setup.alpha.denom);

		body.write_i32::<LittleEndian>(setup.setup.epsilon_decay_amt);
		body.write_u32::<LittleEndian>(setup.setup.epsilon_decay_freq);

		for el in setup.setup.maxes.iter().chain(setup.setup.mins.iter()) {
			body.write_i32::<LittleEndian>(*el);
		}

		let space_end = body.len();
		cursor += space_start - space_end;
	}

	finalize(&mut buf[..cursor], &offsets);

	cursor
}

const MAX_PKT_SIZE: usize = 1500;

pub fn setup(cfg: &mut SetupConfig) {
	let mut buffer = [0u8; MAX_PKT_SIZE];
	let sz = build_setup_packet(cfg, &mut buffer[..]);

	cfg.setup.validate();

	if let Channel::Ethernet(ref mut tx, rx) = &mut cfg.global.channel {
		tx.send_to(&buffer[..sz], None);
	} else {
		eprintln!("Failed to send packet: no channel found");
	}
}