use super::protocol::*;

use crate::{ProtoSetup, StressConfig, DUMB_HACK_FILE, DUMB_HACK_PATH};
use control::{GlobalConfig, Setup, SetupConfig, TilingsConfig};
use std::{
	error::Error,
	fs::File,
	io::{Error as IoError, ErrorKind as IoErrorKind, Result as IoResult, Write},
	process::Command,
	thread,
	time::Duration,
};
use tungstenite::error::Error as WsError;

type AnyRes<A> = Result<A, Box<dyn Error>>;

pub fn run_stress_test(config: &StressConfig, if_name: &str) -> AnyRes<()> {
	// Unfortunately, ip set up -> dpdk is broken.
	// the fix is to do the setup, then reset the machine.
	// this requires a script at boot time to start this program,
	// check for a file in the right place...

	// config.do_hack
	// config.do_trial_with_hack

	let true_min_rate = if let Some(start) = config.do_trial_with_hack {
		start
	} else {
		config.min_rate
	};

	let rate_to_reset_on = if config.do_trial_with_hack.is_some() {
		// this is a resume; reset after the *next* setup.
		Some(true_min_rate + 1)
	} else if config.do_hack {
		// this is the first instance with the hack.
		// reset right here.
		Some(true_min_rate)
	} else {
		None
	};

	for rate in true_min_rate..=config.max_rate {
		// Run this if
		// * no progress
		// * progress and not "true_min_rate"
		if !(rate == true_min_rate && config.do_trial_with_hack.is_some()) {
			eprintln!("Rate set to {}k pkts/sec!", rate);

			unbind_and_reset_nfp(config, if_name)?;

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
		}

		// THIS IS THE RESET POINT.
		if Some(rate) == rate_to_reset_on {
			return save_progress_and_reset(rate).map_err(Into::into);
		}

		bind_nfp(config)?;

		run_dpdk_expt_set(config, rate)?;
	}

	let _ = unbind_and_reset_nfp(config, if_name);

	Ok(())
}

fn save_progress_and_reset(rate: u32) -> IoResult<()> {
	std::fs::create_dir_all(DUMB_HACK_PATH)?;
	{
		let mut out_file = File::create(DUMB_HACK_FILE)?;
		out_file.write_all(&rate.to_string().as_bytes())?;
	}

	rpo(Command::new("reboot").output()?, "just reboot, y'all")
}

fn run_dpdk_expt_set(config: &StressConfig, rate: u32) -> IoResult<()> {
	// sudo -E $PKTGEN_HOME/build/app/pktgen -n 6 -l 0-15 -- -m "{1}.0,{2:3}.1" -f opal-stress.lua

	rpo(
		Command::new(&format!("{}/build/app/pktgen", config.dpdk_pktgen_home))
			.args(&[
				"-n",
				"6",
				"-l",
				"0-15",
				"--",
				"-m",
				"{1}.0,{1:1-7}.1",
				"-f",
				"opal-stress.lua",
			])
			.env("PKTGEN_HOME", config.dpdk_pktgen_home)
			.env("RL_TEST_STRESS_K", rate.to_string())
			.env("RL_TEST_STRESS_ITERS", config.num_trials.to_string())
			.output()?,
		"run full DPDK experiment set",
	)
}

fn bind_nfp(config: &StressConfig) -> IoResult<()> {
	rpo(
		Command::new("dpdk-devbind.py")
			.args(&[&format!("--bind={}", config.bind_driver), config.pci_addr])
			.output()?,
		"DPDK bind",
	)
}

fn unbind_and_reset_nfp(config: &StressConfig, if_name: &str) -> IoResult<()> {
	rpo(
		Command::new("dpdk-devbind.py")
			.args(&["-u", config.pci_addr])
			.output()?,
		"DPDK unbind",
	)?;

	rpo(
		Command::new("rmmod").args(&["nfp"]).output()?,
		"Remove NFP module",
	)?;

	rpo(
		Command::new("modprobe")
			.args(&[
				"nfp",
				"nfp_dev_cpp=1",
				"nfp_pf_netdev=1",
				"fw_load_required=1",
			])
			.output()?,
		"Install NFP module",
	)?;

	thread::sleep(Duration::from_secs(1));

	rpo(
		Command::new("ip")
			.args(&["link", "set", if_name, "up"])
			.output()?,
		"bring up interface",
	)
}

// resultify process output
fn rpo(input: std::process::Output, name: &str) -> IoResult<()> {
	if !input.status.success() {
		return Err(IoError::new(
			IoErrorKind::InvalidInput,
			format!(
				"Failed to perform {} (code {:?}): {:?} {:?}",
				name,
				input.status.code(),
				std::str::from_utf8(&input.stdout[..]),
				std::str::from_utf8(&input.stderr[..]),
			),
		));
	} else {
		Ok(())
	}
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
