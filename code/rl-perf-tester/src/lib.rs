mod config;
mod constants;
mod experiment;

pub use self::{config::*, constants::*, experiment::*};

pub fn list() {
	match std::fs::read_dir(EXPERIMENT_DIR) {
		Ok(iter) =>
			for entry in iter {
				if let Ok(entry) = entry {
					println!("{}", entry.path().file_stem().unwrap().to_str().unwrap());
				}
			},
		Err(e) => println!("Failed to iterate over experiments: {:?}", e),
	}
}

pub fn run_experiment(config: &mut Config) {
	unimplemented!();
}
