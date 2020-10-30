use super::*;

pub struct TransportOffsets {
	ip: usize,
	udp: usize,
	data: usize,
}

pub fn build_transport(
	cfg: &GlobalConfig,
	t_cfg: &TransportConfig,
	buf: &mut [u8],
) -> (usize, TransportOffsets) {
	let mut out = TransportOffsets {
		ip: 0,
		udp: 0,
		data: 0,
	};

	let mut cursor = 0;
	{
		let mut eth_pkt =
			MutableEthernetPacket::new(&mut buf[cursor..]).expect("Plenty of room...");
		eth_pkt.set_destination(MacAddr::broadcast());
		eth_pkt.set_ethertype(EtherTypes::Ipv4);
		eth_pkt.set_source(cfg.iface.mac_address());

		cursor += eth_pkt.packet_size();
	}

	{
		out.ip = cursor;

		let mut ipv4_pkt = MutableIpv4Packet::new(&mut buf[cursor..]).expect("Plenty of room...");
		ipv4_pkt.set_version(4);
		ipv4_pkt.set_header_length(5);
		ipv4_pkt.set_ttl(64);
		ipv4_pkt.set_next_level_protocol(IpNextHeaderProtocols::Udp);
		ipv4_pkt.set_destination(*t_cfg.dst_addr.ip());
		ipv4_pkt.set_source(*t_cfg.src_addr.ip());

		// ipv4_pkt.set_dscp(RL_DSCP_TRAP);

		cursor += ipv4_pkt.packet_size();
	}

	{
		out.udp = cursor;

		let mut udp_pkt = MutableUdpPacket::new(&mut buf[cursor..]).expect("Plenty of room...");
		udp_pkt.set_source(t_cfg.src_addr.port());
		udp_pkt.set_destination(t_cfg.dst_addr.port());
		// checksum is optional in ipv4
		udp_pkt.set_checksum(0);

		cursor += udp_pkt.packet_size();
	}

	out.data = cursor;

	(cursor, out)
}

pub fn finalize(buf: &mut [u8], off: &TransportOffsets, t_cfg: &TransportConfig) {
	{
		let region = &mut buf[off.ip..];
		let len = region.len();
		let mut ipv4_pkt = MutableIpv4Packet::new(region).expect("Plenty of room...");
		ipv4_pkt.set_total_length(len as u16);

		ipv4_pkt.set_checksum(ipv4::checksum(&ipv4_pkt.to_immutable()));
	}

	{
		let (region, payload) = (&mut buf[off.udp..]).split_at_mut(off.data - off.udp);
		let len = region.len() + payload.len();
		let mut udp_pkt = MutableUdpPacket::new(region).expect("Plenty of room...");
		udp_pkt.set_length(len as u16);

		udp_pkt.set_checksum(udp::ipv4_checksum_adv(
			&udp_pkt.to_immutable(),
			payload,
			t_cfg.src_addr.ip(),
			t_cfg.dst_addr.ip(),
		));
	}
}
