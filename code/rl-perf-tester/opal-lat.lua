local p = os.getenv("PKTGEN_HOME") .. "/"
package.path = package.path ..";" .. p .. "?.lua;" .. p .. "test/?.lua;" .. p .. "app/?.lua;"

local results_dir = "../../results/stress/"

require "Pktgen"
local inspect = require('inspect');

local pktSizes = { 64, 128, 256, 512, 1024, 1280, 1518 };

local stress_rate = tonumber(os.getenv("RL_TEST_STRESS_K"))
local iters_to_do = tonumber(os.getenv("RL_TEST_STRESS_ITERS"))

function run_latency(pkt_sz, iter_no)
	-- latency
	pktgen.set(1, "rate", 10);

	pktgen.latsampler_params(1, "simple", 20000, 2000, results_dir .. string.format("l-%dB-%dk-%d.dat", pkt_sz, stress_rate, iter_no))
	pktgen.latsampler("1", "on")
	pktgen.start(1)
	pktgen.delay(11000);
	pktgen.latsampler("1", "off")
	pktgen.stop(1)
	pktgen.clr()
end

function run_expt(pkt_sz, iter_no)
	pktgen.clr()
	pktgen.set(1, "size", pkt_sz);
	run_latency(pkt_sz, iter_no)
	pktgen.delay(10000);
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
