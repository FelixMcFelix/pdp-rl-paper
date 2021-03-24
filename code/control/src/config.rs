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

#[derive(Copy, Clone)]
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

pub struct SetupConfig<'a, T: Tile> {
	pub global: &'a mut GlobalConfig,
	pub setup: Setup<T>,
	pub transport: TransportConfig,
}

impl<'a, T: Tile> SetupConfig<'a, T> {
	pub fn new(global: &'a mut GlobalConfig) -> Self {
		Self {
			global,
			setup: Default::default(),
			transport: Default::default(),
		}
	}
}

pub struct SendStateConfig<'a> {
	pub global: &'a mut GlobalConfig,
	pub transport: TransportConfig,
}

impl<'a> SendStateConfig<'a> {
	pub fn new(global: &'a mut GlobalConfig) -> Self {
		Self {
			global,
			transport: Default::default(),
		}
	}
}

pub struct SendRewardConfig<'a> {
	pub global: &'a mut GlobalConfig,
	pub transport: TransportConfig,
}

impl<'a> SendRewardConfig<'a> {
	pub fn new(global: &'a mut GlobalConfig) -> Self {
		Self {
			global,
			transport: Default::default(),
		}
	}
}

pub struct TilingsConfig<'a> {
	pub global: &'a mut GlobalConfig,
	pub tiling: TilingSet,
	pub transport: TransportConfig,
}

impl<'a> TilingsConfig<'a> {
	pub fn new(global: &'a mut GlobalConfig) -> Self {
		Self {
			global,
			tiling: Default::default(),
			transport: Default::default(),
		}
	}
}

pub struct PolicyConfig<'a, T: Tile> {
	pub global: &'a mut GlobalConfig,
	pub transport: TransportConfig,

	pub policy: Option<Policy<T>>,
	pub tiling: TilingSet,
	pub setup: Setup<T>,
}

impl<'a, T: Tile + Default> PolicyConfig<'a, T> {
	pub fn new(global: &'a mut GlobalConfig) -> Self {
		Self {
			global,
			policy: Some(Default::default()),
			setup: Default::default(),
			tiling: Default::default(),
			transport: Default::default(),
		}
	}
}

pub struct FakePolicyGeneratorConfig<T: Tile> {
	pub tiling: TilingSet,
	pub setup: Setup<T>,
}

pub type FakeStateGeneratorConfig<T> = Setup<T>;
