use flume::{Receiver, Sender};
use std::sync::{
	atomic::{AtomicI32, Ordering},
	Arc,
};

pub struct WbSite {
	pub vals: Vec<AtomicI32>,
	// pub ack_target: AtomicU32,
	// pub acks: AtomicU32,
	pub acker: Sender<()>,
	pub rxer: Receiver<()>,
}

impl WbSite {
	pub fn ack(&self) {
		let _ = self.acker.send(());
	}

	pub fn gather(&self, target: usize) {
		let mut count = 0;
		while target > count {
			if self.rxer.recv().is_ok() {
				count += 1;
			} else {
				break;
			}
		}
	}

	pub fn clear_actions(&self) {
		for val in &self.vals {
			val.store(0, Ordering::Relaxed);
		}
	}

	pub fn select_max(&self) -> (i32, usize) {
		let mut max = None;

		for (i, val) in self.vals.iter().enumerate() {
			let val_read = val.load(Ordering::Relaxed);

			// println!("val {}: {}", i, val_read);

			match max {
				None => {
					max = Some((val_read, i));
				},
				Some((l_val, _l_i)) if val_read > l_val => {
					max = Some((val_read, i));
				},
				_ => {},
			}
		}

		max.unwrap()
	}
}

pub type Writeback = Arc<WbSite>;

pub fn new_writeback(num_actions: usize) -> Writeback {
	let (acker, rxer) = flume::unbounded();
	let mut vals = Vec::new();
	vals.resize_with(num_actions, || AtomicI32::new(0));

	Arc::new(WbSite { vals, acker, rxer })
}
