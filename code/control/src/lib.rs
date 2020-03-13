use byteorder::{
	ByteOrder,
	LittleEndian,
};
use enum_primitive::*;
use fraction::Ratio;
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
		ipv4::*,
		udp::*,
		PacketSize,
	},
	util::MacAddr,
};
use std::net::{
	IpAddr,
	Ipv4Addr,
};

pub struct GlobalConfig {
	iface: Option<NetworkInterface>,
	channel: Option<Channel>,
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
			.next();

		let channel = iface.as_ref()
			.and_then(|iface| datalink::channel(&iface, Default::default()).ok());

		Self {
			iface,
			channel,
		}
	}
}

pub struct SetupConfig<'a> {
	global: &'a mut GlobalConfig,
}

impl<'a> SetupConfig<'a> {
	pub fn new(global: &'a mut GlobalConfig) -> Self {
		Self {
			global
		}
	}
}

pub fn list() {
	println!("{:#?}", datalink::interfaces());
}

type Tile = i32;

pub struct Setup {
	n_dims: u16,
	tiles_per_dim: u16,
	tilings_per_set: u16,
	epsilon: Ratio<Tile>,
	alpha: Ratio<Tile>,
	epsilon_decay_amt: Tile,
	epsilon_decay_freq: u32,
	maxes: Vec<Tile>,
	mins: Vec<Tile>,
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

fn build_transport(cfg: &GlobalConfig, buf: &mut [u8]) -> usize {
	// FIXME
	let src_ip = Ipv4Addr::new(192, 168, 1, 1);
	let dst_ip = Ipv4Addr::new(192, 168, 1, 141);
	let src_port = 16767;
	let dst_port = 16768;

	let mut cursor = 0;
	{
		let mut eth_pkt = MutableEthernetPacket::new(&mut buf[cursor..])
			.expect("Plenty of room...");
		eth_pkt.set_destination(MacAddr::broadcast());
		// not important to set source, not interested in receiving a reply...
		eth_pkt.set_ethertype(EtherTypes::Ipv4);
		if let Some(iface) = &cfg.iface {
			eth_pkt.set_source(iface.mac_address());
		}

		cursor += eth_pkt.packet_size();
	}

	{
		let mut ipv4_pkt = MutableIpv4Packet::new(&mut buf[cursor..])
			.expect("Plenty of room...");
		ipv4_pkt.set_version(4);
		ipv4_pkt.set_header_length(5);
		ipv4_pkt.set_ttl(64);
		ipv4_pkt.set_next_level_protocol(IpNextHeaderProtocols::Udp);
		ipv4_pkt.set_destination(dst_ip);
		ipv4_pkt.set_source(src_ip);
		cursor += ipv4_pkt.packet_size();

		// FIXME: set ECN bits...
	}

	{
		let mut udp_pkt = MutableUdpPacket::new(&mut buf[cursor..])
			.expect("Plenty of room...");
		udp_pkt.set_source(src_port);
		udp_pkt.set_destination(dst_port);
		// checksum is optional in ipv4
		udp_pkt.set_checksum(0);

		cursor += udp_pkt.packet_size();
	}

	cursor
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

fn build_setup_packet(setup: &SetupConfig, buf: &mut [u8]) -> usize {
	let mut cursor = 0;
	cursor += build_transport(&setup.global, buf);
	cursor += build_type(buf, RlType::Config);
	cursor += build_config_type(buf, RlConfigType::Setup);

	// TODO: write struct out

	cursor
}

const MAX_PKT_SIZE: usize = 1500;

pub fn setup(cfg: &mut SetupConfig) {
	let mut buffer = [0u8; MAX_PKT_SIZE];
	let sz = build_setup_packet(cfg, &mut buffer[..]);

	if let Some(Channel::Ethernet(ref mut tx, rx)) = &mut cfg.global.channel {
		tx.send_to(&buffer[..sz], None);
	}
}