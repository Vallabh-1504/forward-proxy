-- bench.lua
-- Usage:
--   BENCH_MODE=hit  wrk -t4 -c16 -d15s -s bench.lua http://127.0.0.1:8080
--   BENCH_MODE=miss wrk -t4 -c16 -d15s -s bench.lua http://127.0.0.1:8080

local mode = os.getenv("BENCH_MODE") or "hit"
local id = 0
local counter = 0

function setup(thread)
    thread:set("id", id)
    id = id + 1
end

request = function()
    if mode == "hit" then
        return "GET http://127.0.0.1:8000/ping HTTP/1.1\r\n" ..
               "Host: 127.0.0.1:8000\r\n\r\n"
    else
        counter = counter + 1
        local key = id .. "_" .. counter
        return "GET http://127.0.0.1:8000/ping?n=" .. key .. " HTTP/1.1\r\n" ..
               "Host: 127.0.0.1:8000\r\n\r\n"
    end
end