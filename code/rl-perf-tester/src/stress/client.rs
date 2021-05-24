use super::protocol::*;

use crate::{ProtoSetup, StressConfig};
use control::{GlobalConfig, Setup, SetupConfig, TilingsConfig};
use std::{error::Error, io::Result as IoResult, process::Command, thread, time::Duration};
use tungstenite::error::Error as WsError;

type AnyRes<A> = Result<A, Box<dyn Error>>;

pub fn run_stress_test(config: &StressConfig, if_name: &str) -> AnyRes<()> {
	const PKT_SIZES: [usize; 10] = [64, 128, 256, 512, 1024, 1500, 2048, 4096, 8192, 9000];

	for rate in config.min_rate..=config.max_rate {
		eprintln!("Rate set to {}k pkts/sec!", rate);

		unbind_and_reset_nfp(config)?;

		eprintln!("Device {} unbound from DPDK!", config.pci_addr);

		while tell_dut_to_install_fw(config, rate)? {
			// Accidentally in use... so sleep it off.
			eprintln!("DUT busy; retrying in 10s...");
			thread::sleep(Duration::from_secs(10));
		}

		eprintln!("Firmware {}.0k installed!", rate);

		thread::sleep(Duration::from_secs(1));

		setup_dut(config, if_name)?;

		eprintln!("Control packets sent over {}!", if_name);

		thread::sleep(Duration::from_secs(3));

		bind_nfp(config)?;

		for i in 0..config.num_trials {
			for pkt_sz in PKT_SIZES.iter() {
				// TODO: Pktgen stuff.
				eprintln!("Trial {} for {}B pkts would be here!", i, pkt_sz);
			}
		}
	}

	let _ = unbind_and_reset_nfp(config);

	Ok(())
}

fn bind_nfp(config: &StressConfig) -> IoResult<()> {
	Command::new("dpdk-devbind.py")
		.args(&[&format!("--bind={}", config.bind_driver), config.pci_addr])
		.output()
		.map(|_| ())
}

fn unbind_and_reset_nfp(config: &StressConfig) -> IoResult<()> {
	Command::new("dpdk-devbind.py")
		.args(&["-u", config.pci_addr])
		.output()?;

	Command::new("rmmod").args(&["nfp"]).output()?;

	Command::new("modprobe")
		.args(&[
			"nfp",
			"nfp_dev_cpp=1",
			"nfp_pf_netdev=1",
			"fw_load_required=1",
		])
		.output()?;

	Ok(())
}

// returns true if "should retry".
// this is by no means what we'd call a "Sane API".
fn tell_dut_to_install_fw(config: &StressConfig, rate: u32) -> AnyRes<bool> {
	let (mut ws, _resp) =
		tungstenite::connect(format!("ws://{}:{}", config.host_server, config.port))?;

	ws.write_message(for_ws(&ClientToHost::Fw(FwInstall {
		name: format!("{}.0k", rate),
		p4cfg_override: None,
	})))?;

	let mut can_exit = false;
	let mut was_busy = false;
	let mut err_to_throw: Option<Box<dyn Error>> = None;
	while !can_exit {
		let msg = ws.read_message();
		match msg {
			Ok(msg) =>
				if let Ok(text) = msg.to_text() {
					let decoded = serde_json::from_str::<HostToClient>(text);

					match decoded {
						Ok(HostToClient::Fail(r)) => match r {
							FailReason::Busy => {
								was_busy = true;
							},
							FailReason::Generic(e) => {
								err_to_throw = Some(e.into());
							},
						},
						Ok(HostToClient::Success) => {},
						Ok(HostToClient::IllegalRequest) => {
							err_to_throw = Some("Server named this an illegal request!".into());
						},
						Err(e) => {
							err_to_throw = Some(e.into());
						},
					}

					can_exit = true;
				},
			Err(WsError::ConnectionClosed) | Err(WsError::AlreadyClosed) => {
				can_exit = true;
			},
			Err(_) => {},
		}

		if can_exit {
			let _ = ws.close(None);
		}
	}

	err_to_throw.map(Err).unwrap_or_else(|| Ok(was_busy))
}

pub fn setup_dut(config: &StressConfig, if_name: &str) -> IoResult<()> {
	let mut global_base = GlobalConfig::new(Some(if_name));
	let global = &mut global_base;
	let transport = config.transport_cfg;

	let mut setup: Setup<i32> = ProtoSetup::default().instantiate(true);
	let tiling = crate::generate_tiling(&setup, 28);
	setup.limit_workers = None;

	let mut setup_cfg = SetupConfig {
		global,
		transport,
		setup,
	};

	control::setup::<i32>(&mut setup_cfg);

	control::tilings(&mut TilingsConfig {
		global,
		tiling,
		transport,
	});

	Ok(())
}
