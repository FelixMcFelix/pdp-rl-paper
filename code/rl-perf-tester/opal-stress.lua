local p = os.getenv("PKTGEN_HOME") .. "/"
package.path = package.path ..";" .. p .. "?.lua;" .. p .. "test/?.lua;" .. p .. "app/?.lua;"

local results_dir = "../../results/stress/"

require "Pktgen"
local inspect = require('inspect');

local pktSizes = { 64, 128, 256, 512, 1024, 1280, 1518 };

local stress_rate = tonumber(os.getenv("RL_TEST_STRESS_K"))
local iters_to_do = tonumber(os.getenv("RL_TEST_STRESS_ITERS"))

function run_expt(pkt_sz, iter_no)
	pktgen.clr()
	pktgen.set(1, "size", pkt_sz);
	pktgen.set(1, "rate", 100);

	-- latency
	-- 5s spoolup, 10s monitoring
	pktgen.start(1);
	pktgen.delay(5000);
	pktgen.clr()
	pktgen.delay(10000);
	pktgen.stop(1);


	pktgen.delay(5000);

	-- printf("Pktgen stats1   : %s\n", inspect(pktgen.pktStats(1)));
	-- printf("Pktgen stats2   : %s\n", inspect(pktgen.portStats(1, "port")));
	-- printf("Pktgen stats3   : %s\n", inspect(pktgen.portStats(1, "rate")));

	local tput_file_name = results_dir .. string.format("t-%dB-%dk-%d.dat", pkt_sz, stress_rate, iter_no)
	local tput_file = io.open(tput_file_name, "w")
	tput_file:write(inspect(pktgen.portStats(1, "port")))
	tput_file:close()

	printf("Wrote packet/byte volume data to: %s\n", tput_file_name)

	-- latency
	pktgen.clr()
	pktgen.set(1, "rate", 10);
	pktgen.latsampler_params(1, "simple", 10000, 1000, results_dir .. string.format("l-%dB-%dk-%d.dat", pkt_sz, stress_rate, iter_no))
	pktgen.latsampler("1", "on")
	pktgen.start(1)
	pktgen.delay(10000);
	pktgen.latsampler("1", "off")
	pktgen.stop(1)
	pktgen.clr()
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
