use serde::{Deserialize, Serialize};
use tungstenite::protocol::Message;

#[derive(Clone, Deserialize, Serialize)]
pub enum ClientToHost {
	Fw(FwInstall),
}

#[derive(Clone, Deserialize, Serialize)]
pub enum HostToClient {
	Success,
	Fail(FailReason),
	IllegalRequest,
}

#[derive(Clone, Deserialize, Serialize)]
pub enum FailReason {
	Busy,
	Generic(String),
}

#[derive(Clone, Deserialize, Serialize)]
pub struct FwInstall {
	pub name: String,
	pub p4cfg_override: Option<String>,
}

pub fn for_ws(msg: &impl Serialize) -> Message {
	Message::Text(serde_json::to_string(msg).unwrap())
}
