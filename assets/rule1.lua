
pin = 15
threshold = 50

function setup()
    -- 创建一个共享上下文table
    gpio.set_mode(pin, gpio.MODE_INPUT_OUTPUT)
end

function loop()
    local val = read_sensor()
    if val > threshold then
        gpio.set_level(pin, 1)
    else
        gpio.set_level(pin, 0)
    end
	coroutine.yield()

	settimeout(1000)  -- 每秒检查一次
end
