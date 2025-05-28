-- main/rules/rule1.lua
-- 示例规则：当传感器读数超过阈值时，控制LED灯
local sensor_threshold = 50

co_rule1 = coroutine.create(function()
    while true do
        local sensor_value = read_sensor()
        if sensor_value > sensor_threshold then
            print("Rule1: Sensor value exceeded threshold: " .. sensor_value)
            gpio.set_level(15, 1)  -- 打开LED
        else
            print("Rule1: Sensor value within threshold: " .. sensor_value)
            gpio.set_level(15, 0)  -- 关闭LED
        end
        coroutine.yield()  -- 暂停协程
    end
end)