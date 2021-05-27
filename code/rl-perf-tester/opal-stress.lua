local p = os.getenv("PKTGEN_HOME") .. "/"
package.path = package.path ..";" .. p .. "?.lua;" .. p .. "test/?.lua;" .. p .. "app/?.lua;"

local results_dir = "../../results/stress/"

require "Pktgen"
local inspect = require('inspect');

local pktSizes = { 64, 128, 256, 512, 1024, 1280, 1518 };

local stress_rate = tonumber(os.getenv("RL_TEST_STRESS_K"))
local iters_to_do = tonumber(os.getenv("RL_TEST_STRESS_ITERS"))

function run_tput(pkt_sz, iter_no)
	pktgen.set(1, "rate", 100);

	-- tput
	-- can't do spoolup, because we need to enumerate *every* packet on the wire here.
	-- 30s monitoring
	pktgen.start(1);
	pktgen.delay(30000);
	pktgen.stop(1);

	pktgen.delay(5000);

	local tput_file_name = results_dir .. string.format("t-%dB-%dk-%d.dat", pkt_sz, stress_rate, iter_no)
	local tput_file = io.open(tput_file_name, "w")
	tput_file:write(inspect(pktgen.portStats(1, "port")))
	tput_file:close()

	printf("Wrote packet/byte volume data to: %s\n", tput_file_name)
end

function run_expt(pkt_sz, iter_no)
	pktgen.clr()
	pktgen.set(1, "size", pkt_sz);
	run_tput(pkt_sz, iter_no)
	pktgen.delay(5000);
end

function main()
	pktgen.screen("off");
	-- run_expt(128, 0)
	for iter=0,iters_to_do-1 do
		for _,pkt_sz in pairs(pktSizes) do
			printf("Starting %dB, iter %d\n", pkt_sz, iter)
			run_expt(pkt_sz, iter)
		end
	end
	pktgen.quit();
end

main()
