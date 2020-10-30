use super::*;

pub struct GlobalConfig {
	pub iface: NetworkInterface,
	pub channel: Channel,
}

impl GlobalConfig {
	pub fn new(iface_name: Option<&str>) -> Self {
		let iface = datalink::interfaces()
			.into_iter()
			.find(move |iface: &NetworkInterface| {
				if let Some(name) = iface_name {
					iface.name == name
				} else {
					true
				}
			})
			.unwrap_or_else(|| panic!("Couldn't bind interface \"{}\"", iface_name.unwrap_or("")));

		let channel = datalink::channel(&iface, Default::default())
			.map_err(|e| eprintln!("{:?}", e))
			.unwrap_or_else(|_| {
				panic!("Failed to open channel on \"{}\"", iface_name.unwrap_or(""))
			});

		Self { iface, channel }
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

pub struct Config<'a> {
	pub global: &'a mut GlobalConfig,
	pub transport: TransportConfig,
	pub iterations: u64,
}

impl<'a> Config<'a> {
	pub fn new(global: &'a mut GlobalConfig, iterations: u64) -> Self {
		Self {
			global,
			transport: Default::default(),
			iterations,
		}
	}
}
