use super::*;

enum_from_primitive! {
pub enum RlType {
	Config = 0,
	Insert,
	State,
	Reward,
}
}

enum_from_primitive! {
pub enum RlConfigType {
	Setup = 0,
	Tiles,
}
}

pub struct TransportOffsets {
	ip: usize,
	udp: usize,
	data: usize,
}

fn build_transport(
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

		ipv4_pkt.set_dscp(RL_DSCP_TRAP);

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

fn finalize(buf: &mut [u8], off: &TransportOffsets, t_cfg: &TransportConfig) {
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

pub fn build_setup_packet(setup: &SetupConfig, buf: &mut [u8]) -> IoResult<usize> {
	let (mut cursor, offsets) = build_transport(setup.global, &setup.transport, buf);
	cursor += build_type(&mut buf[cursor..], RlType::Config);
	cursor += build_config_type(&mut buf[cursor..], RlConfigType::Setup);

	{
		let mut body = &mut buf[cursor..];
		let space_start = body.len();

		body.write_u8(setup.setup.do_updates as u8)?;
		body.write_u8(setup.setup.quantiser_shift)?;

		body.write_u16::<BigEndian>(setup.setup.n_dims)?;
		body.write_u16::<BigEndian>(setup.setup.tiles_per_dim)?;
		body.write_u16::<BigEndian>(setup.setup.tilings_per_set)?;
		body.write_u16::<BigEndian>(setup.setup.n_actions)?;

		//serialise do_updates, quantiser_shift, gamma, reward_key, state_key

		body.write_i32::<BigEndian>(setup.setup.epsilon)?;
		body.write_i32::<BigEndian>(setup.setup.alpha)?;
		body.write_i32::<BigEndian>(setup.setup.gamma)?;

		body.write_i32::<BigEndian>(setup.setup.epsilon_decay_amt)?;
		body.write_u32::<BigEndian>(setup.setup.epsilon_decay_freq)?;

		body.write_u8(setup.setup.state_key.id())?;
		body.write_i32::<BigEndian>(setup.setup.state_key.body())?;

		body.write_u8(setup.setup.reward_key.id())?;
		body.write_i32::<BigEndian>(setup.setup.reward_key.body())?;

		for el in setup.setup.maxes.iter().chain(setup.setup.mins.iter()) {
			body.write_i32::<BigEndian>(*el)?;
		}

		let space_end = body.len();
		cursor += space_start - space_end;
	}

	finalize(&mut buf[..cursor], &offsets, &setup.transport);

	Ok(cursor)
}

pub fn build_tilings_packet(tile_cfg: &TilingsConfig, buf: &mut [u8]) -> IoResult<usize> {
	let (mut cursor, offsets) = build_transport(tile_cfg.global, &tile_cfg.transport, buf);
	cursor += build_type(&mut buf[cursor..], RlType::Config);
	cursor += build_config_type(&mut buf[cursor..], RlConfigType::Tiles);

	{
		let mut body = &mut buf[cursor..];
		let space_start = body.len();

		for el in tile_cfg.tiling.tilings.iter() {
			body.write_u16::<BigEndian>(el.dims.len() as u16)?;

			if let Some(location) = el.location {
				body.write_u8(location)?;
			}

			for dim in el.dims.iter() {
				body.write_u16::<BigEndian>(*dim)?;
			}
		}

		let space_end = body.len();
		cursor += space_start - space_end;
	}

	finalize(&mut buf[..cursor], &offsets, &tile_cfg.transport);

	Ok(cursor)
}

pub fn build_policy_packet_base(
	tile_cfg: &PolicyConfig,
	buf: &mut [u8],
) -> IoResult<(usize, TransportOffsets)> {
	let (mut cursor, offsets) = build_transport(tile_cfg.global, &tile_cfg.transport, buf);
	cursor += build_type(&mut buf[cursor..], RlType::Insert);

	// finalize(&mut buf[..cursor], &offsets, &tile_cfg.transport);

	Ok((cursor, offsets))
}

pub fn write_and_send_policy(
	cfg: &mut PolicyConfig,
	buf: &mut [u8],
	base_offset: usize,
	policy: impl Iterator<Item = i32>,
	offsets: &TransportOffsets,
) {
	let boundaries = PolicyBoundaries::compute(&cfg.setup, &cfg.tiling);
	let mut active_bound = 0;

	let max_send_bytes = buf.len() - base_offset;
	let max_send_tiles =
		(max_send_bytes - std::mem::size_of::<u32>()) / std::mem::size_of::<Tile>();

	let mut packet_offset = None;

	let mut curr_send_tiles = 0;
	let mut cursor = base_offset;

	for (i, tile) in policy.enumerate() {
		if packet_offset.is_none() {
			(&mut buf[cursor..])
				.write_u32::<BigEndian>(i as u32)
				.unwrap();
			cursor += std::mem::size_of::<u32>();
			packet_offset = Some(i);
		}

		// Might be in a 0-tile tiling...
		while i >= boundaries.0[active_bound] {
			active_bound += 1;
		}

		(&mut buf[cursor..]).write_i32::<BigEndian>(tile).unwrap();
		cursor += std::mem::size_of::<i32>();
		curr_send_tiles += 1;

		if curr_send_tiles >= max_send_tiles || boundaries.0[active_bound] == (i + 1) {
			finalize(&mut buf[..cursor], &offsets, &cfg.transport);

			if let Channel::Ethernet(ref mut tx, _rx) = &mut cfg.global.channel {
				tx.send_to(&buf[..cursor], None);
			} else {
				eprintln!("Failed to send packet: no channel found");
			}

			curr_send_tiles = 0;
			packet_offset = None;
			cursor = base_offset;
		}
	}

	if curr_send_tiles != 0 {
		finalize(&mut buf[..cursor], &offsets, &cfg.transport);

		if let Channel::Ethernet(ref mut tx, _rx) = &mut cfg.global.channel {
			tx.send_to(&buf[..cursor], None);
		} else {
			eprintln!("Failed to send packet: no channel found");
		}
	}
}

pub fn write_and_send_sparse_policy(
	cfg: &mut PolicyConfig,
	buf: &mut [u8],
	base_offset: usize,
	policy: impl Iterator<Item = SparsePolicyEntry<i32>>,
	offsets: &TransportOffsets,
) {
	for part in policy {
		let cursor = {
			let mut body = &mut buf[base_offset..];
			let space_start = body.len();

			body.write_u32::<BigEndian>(part.offset);

			for tile in part.data.iter() {
				body.write_i32::<BigEndian>(*tile).unwrap();
			}

			base_offset + space_start - body.len()
		};

		finalize(&mut buf[..cursor], &offsets, &cfg.transport);

		if let Channel::Ethernet(ref mut tx, _rx) = &mut cfg.global.channel {
			tx.send_to(&buf[..cursor], None);
		} else {
			eprintln!("Failed to send packet: no channel found");
		}
	}
}

pub fn build_state_packet(
	tile_cfg: &SendStateConfig,
	buf: &mut [u8],
	state: Vec<Tile>,
) -> IoResult<usize> {
	let (mut cursor, offsets) = build_transport(tile_cfg.global, &tile_cfg.transport, buf);
	cursor += build_type(&mut buf[cursor..], RlType::State);

	{
		let mut body = &mut buf[cursor..];
		let space_start = body.len();

		body.write_u16::<BigEndian>(state.len() as u16)?;

		for el in state.iter() {
			body.write_i32::<BigEndian>(*el)?;
		}

		let space_end = body.len();
		cursor += space_start - space_end;
	}

	finalize(&mut buf[..cursor], &offsets, &tile_cfg.transport);

	Ok(cursor)
}

pub fn build_reward_packet(
	tile_cfg: &SendRewardConfig,
	buf: &mut [u8],
	quant_reward: i32,
	value_loc: i32,
) -> IoResult<usize> {
	let (mut cursor, offsets) = build_transport(tile_cfg.global, &tile_cfg.transport, buf);
	cursor += build_type(&mut buf[cursor..], RlType::Reward);

	{
		let mut body = &mut buf[cursor..];
		let space_start = body.len();

		body.write_i32::<BigEndian>(quant_reward)?;
		body.write_i32::<BigEndian>(value_loc)?;

		let space_end = body.len();
		cursor += space_start - space_end;
	}

	finalize(&mut buf[..cursor], &offsets, &tile_cfg.transport);

	Ok(cursor)
}
