cd target/release && sudo ./control -i vf0_0 setup --src-ip 192.168.0.4 ../../config/example-setup-big.json --dst-ip 192.168.1.5 --dst-port 10602 --src-port 9001
cd target/release && sudo ./control -i vf0_0 tiling --src-ip 192.168.0.4 ../../config/example-tiling-big.json --dst-ip 192.168.1.5 --dst-port 10602 --src-port 9001
# Skip policy: this is only used to estimate timing costs.
cd target/release && sudo ./control -i vf0_0 state --src-ip 192.168.0.4 ../../config/example-state-big.json
