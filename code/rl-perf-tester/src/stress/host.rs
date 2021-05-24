use super::protocol::*;

use crate::{HostConfig, FIRMWARE_DIR};
use std::{
	io::ErrorKind as IoErrorKind,
	net::TcpListener,
	sync::{
		atomic::{AtomicBool, Ordering},
		Arc,
	},
};
use tungstenite::error::Error as WsError;

pub fn host_stress_server(port: u16, config: HostConfig) {
	let listener = TcpListener::bind(("127.0.0.1", port)).unwrap();
	let busy = Arc::new(AtomicBool::new(false));

	let cfg = Arc::new(config);

	for stream in listener.incoming() {
		if let Ok(stream) = stream {
			let l_busy = busy.clone();
			stream.set_nonblocking(true).unwrap();

			let cfg = Arc::clone(&cfg);

			std::thread::spawn(move || {
				let mut websocket = tungstenite::server::accept(stream).unwrap();

				let was_busy = l_busy.fetch_or(true, Ordering::Relaxed);

				if was_busy {
					// Send Message.
					let _ = websocket.write_message(for_ws(&HostToClient::Fail(FailReason::Busy)));
					// Close Conn.
					let _ = websocket.close(None);
					return;
				}

				let (tx, rx) = flume::unbounded();

				loop {
					match rx.try_recv() {
						Ok(None) => {
							let _ = websocket.write_message(for_ws(&HostToClient::Success));
						},
						Ok(Some(mistake)) => {
							let _ = websocket.write_message(for_ws(&HostToClient::Fail(mistake)));
						},
						_ => {},
					}

					match websocket.read_message() {
						Ok(msg) =>
							if let Ok(text) = msg.to_text() {
								match serde_json::from_str::<ClientToHost>(text) {
									Ok(ClientToHost::Fw(f)) => {
										let my_tx = tx.clone();
										let my_cfg = Arc::clone(&cfg);
										std::thread::spawn(move || {
											install_fw(my_tx, f, &my_cfg);
										});
									},
									_ => {
										let _ = websocket
											.write_message(for_ws(&HostToClient::IllegalRequest));
										let _ = websocket.close(None);

										break;
									},
								}
							},
						Err(WsError::Io(e)) if e.kind() == IoErrorKind::WouldBlock => {
							// falls out to do write queue etc.
						},
						Err(_) => {
							let _ = websocket.write_message(for_ws(&HostToClient::IllegalRequest));
							let _ = websocket.close(None);

							break;
						},
					}

					if websocket.write_pending().is_err() {
						break;
					}
					std::thread::sleep(std::time::Duration::from_millis(20));
				}

				l_busy.store(false, Ordering::Relaxed);
			});
		}
	}
}

pub fn install_fw(tx: flume::Sender<Option<FailReason>>, f: FwInstall, config: &HostConfig) {
	let in_file = format!("{}/stress/{}.nffw", FIRMWARE_DIR, f.name);
	let json_file = format!("{}/stress/{}.json", FIRMWARE_DIR, f.name);

	let to_send = match crate::install_fw(&config.rtecli_path, &in_file, &json_file) {
		Ok(_) => None,
		Err(e) => Some(FailReason::Generic(format!("{:?}", e))),
	};

	let _ = tx.send(to_send);
}
