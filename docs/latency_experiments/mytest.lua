local mg     = require "moongen"
local memory = require "memory"
local device = require "device"
local ts     = require "timestamping"
local filter = require "filter"
local hist   = require "histogram"
local stats  = require "stats"
local timer  = require "timer"
local arp    = require "proto.arp"
local log    = require "log"

-- set addresses here
-- local DST_MAC		= nil -- resolved via ARP on GW_IP or DST_IP, can be overriden with a string here
local DST_MAC		= "3c:fd:fe:55:52:fa"
local SRC_IP_BASE	= "192.168.1.2" -- actual address will be SRC_IP_BASE + random(0, flows)
local SRC_IP = "192.168.1.2"
local DST_IP		= "192.168.1.10"
local SRC_PORT		= 1234
local DST_PORT		= 8080
local TS_DST_PORT	= 319

-- answer ARP requests for this IP on the rx port
-- change this if benchmarking something like a NAT device
local RX_IP		= DST_IP
-- used to resolve DST_MAC
local GW_IP		= DST_IP
-- used as source IP to resolve GW_IP to DST_MAC
local ARP_IP	= SRC_IP_BASE

function configure(parser)
	parser:description("Generates UDP traffic and measure latencies. Edit the source to modify constants like IPs.")
	parser:argument("dev", "Device to receive/transmit from."):convert(tonumber)
	parser:option("-r --rate", "Transmit rate in Mbit/s."):default(1000):convert(tonumber)
	parser:option("-f --flows", "Number of flows (randomized source IP)."):default(1):convert(tonumber)
	parser:option("-s --size", "Packet size."):default(84):convert(tonumber)
end

function master(args)
	local cntqueue = 2
	local data_q = 0
	local ts_q = 1
	dev = device.config{port = args.dev, rxQueues = cntqueue, txQueues = cntqueue}
	device.waitForLinks()
	-- max 1kpps timestamping traffic timestamping
	-- rate will be somewhat off for high-latency links at low rates
	if args.rate > 0 then
		dev:getTxQueue(data_q):setRate(args.rate - (args.size + 4) * 8 / 1000)
	end

	mg.startTask("loadSlave", dev:getTxQueue(data_q), dev:getRxQueue(data_q), args.size, args.flows)
	mg.startTask("timerSlave", dev:getTxQueue(ts_q), dev:getRxQueue(ts_q), args.size, args.flows)
	mg.waitForTasks()
end

local function setTimestampingFDir(rxQueue)
	-- set rule for placing timestamp packets on rxQueue 1
	-- Note: return `true` from modify function (in timerSlave) so that
	-- the rule is not overwritten
	local mempool = memory.createMemPool()
	local bufs = mempool:bufArray()
	bufs:alloc(84)
	local flowdir_pkt = bufs[1]:getUdpPacket()
	flowdir_pkt:fill({
		-- ethSrc = DST_MAC,
		-- ethDst = dev:getTxQueue(ts_q),
		ip4Src = SRC_IP,
		ip4Dst = DST_IP,
		udpSrc = SRC_PORT,
		udpDst = TS_DST_PORT,
		pktLength = 84,
	})
	rxQueue.dev:reconfigureUdpTimestampFilter(rxQueue, flowdir_pkt)
	bufs:freeAll()
end

local function fillUdpPacket(queue, buf, len)
	buf:getUdpPacket():fill{
		ethSrc = queue,
		ethDst = DST_MAC,
		ip4Src = SRC_IP,
		ip4Dst = DST_IP,
		udpSrc = SRC_PORT,
		udpDst = DST_PORT,
		pktLength = len
	}
end

local function doArp()
	if not DST_MAC then
		log:info("Performing ARP lookup on %s", GW_IP)
		DST_MAC = arp.blockingLookup(GW_IP, 5)
		if not DST_MAC then
			log:info("ARP lookup failed, using default destination mac address")
			return
		end
	end
	log:info("Destination mac: %s", DST_MAC)
end

function loadSlave(txQueue, rxQueue, size, flows)
	doArp()
	local mempool = memory.createMemPool(function(buf)
		fillUdpPacket(txQueue, buf, size)
	end)
	local bufs = mempool:bufArray()
	local counter = 0
	local txCtr = stats:newDevTxCounter(txQueue, "plain")
	local rxCtr = stats:newDevRxCounter(rxQueue, "plain")
	local baseIP = parseIPAddress(SRC_IP_BASE)

	while mg.running() do
		bufs:alloc(size)
		for i, buf in ipairs(bufs) do
			local pkt = buf:getUdpPacket()
			pkt.ip4.src:set(baseIP + counter)
			counter = incAndWrap(counter, flows)
		end
		-- UDP checksums are optional, so using just IPv4 checksums would be sufficient here
		bufs:offloadUdpChecksums()
		txQueue:send(bufs)
		txCtr:update()
		rxCtr:update()
	end
	txCtr:finalize()
	rxCtr:finalize()
end

function timerSlave(txQueue, rxQueue, size, flows)
	doArp()
	if size < 84 then
		log:warn("Packet size %d is smaller than minimum timestamp size 84. Timestamped packets will be larger than load packets.", size)
		size = 84
	end
	setTimestampingFDir(rxQueue)
	local timestamper = ts:newUdpTimestamper(txQueue, rxQueue, nil)
	local hist = hist:new()
	mg.sleepMillis(1000) -- ensure that the load task is running
	local counter = 0
	local rateLimit = timer:new(0.001)
	local baseIP = parseIPAddress(SRC_IP_BASE)
	local modifyPacket;
	modifyPacket = function(buf)
		fillUdpPacket(txQueue, buf, size)
		local pkt = buf:getUdpPacket()
		pkt.udp:setDstPort(TS_DST_PORT)
		pkt.ip4.src:set(baseIP + counter)
		counter = incAndWrap(counter, flows)

		return false -- reconfigure filters  (i40e)
		-- rxQueue.dev:reconfigureUdpTimestampFilter(rxQueue, pkt)
		-- return true -- do not reconfigure filters  (i40e)
	end
	while mg.running() do
		lat, count = timestamper:measureLatency(size, modifyPacket)
		-- print(lat, count)
		hist:update(lat, count)
		rateLimit:wait()
		rateLimit:reset()
	end
	-- print the latency stats after all the other stuff
	mg.sleepMillis(300)
	hist:print()
	hist:save("histogram.csv")
end

