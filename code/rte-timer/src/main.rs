use byteorder::{BigEndian, WriteBytesExt};
use itertools::Itertools;
use serde_json::json;
use std::{
	fs::self,
	process::Command,
	time::Instant,
};

const P4CFG_FILE: &str = "temp.p4cfg";
const RESULT_DIR: &str = "../../results/rte-timer";

const ITERS_PER_RUN: u64 = 1_000;
const INCLUSIVE_MAX_RULES: u64 = 65536;

fn main() {
	let mut i = 1;
    while i <= INCLUSIVE_MAX_RULES {
    	let mut rules = vec![];
    	for rule_no in 0..i {
    		let mut byte_space = [0u8; 6];

			(&mut byte_space[..]).write_u48::<BigEndian>(rule_no).unwrap();

    		let mac_str = format!("{:02x}", (&byte_space[..]).iter().format(":"));

    		rules.push(json!({
    			"action": {
    				"data": {"port": {"value": "p0"}},
    				"type": "ingress::set_egress_spec"
    			},
    			"name": format!("{}", rule_no),
    			"match": {"ethernet.eth_dst": {"value": mac_str}}
    		}));
    	}

    	let out = serde_json::to_vec(&json!({
    		"tables": {"ingress::forward": {"rules": rules}}
    	})).unwrap();

    	fs::write(P4CFG_FILE, out).unwrap();

    	let mut times = vec![];
    	for _iter in 0..ITERS_PER_RUN {
    		let mut c = Command::new("rtecli");
    		c.args(&["config-reload", "-c", P4CFG_FILE]);

    		let t = Instant::now();
    		c.spawn().unwrap().wait();
    		let t = t.elapsed();

    		times.push(t.as_nanos());
    	}

    	let out = format!("{}", (&times[..]).iter().format("\n"));

    	fs::write(format!("{}/{}.csv", RESULT_DIR, i), out).unwrap();

    	i <<= 1;
    }

    fs::remove_file(P4CFG_FILE).unwrap();
}
