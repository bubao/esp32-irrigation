local _ENV = ...

function setup()
    -- gpio.set_mode(pin, gpio.MODE_INPUT_OUTPUT)
end

function loop()
    local val = read_sensor()
    log("Sensor value: " .. val)
    return settimeout(1000)
end

return {
    setup = setup,
    loop = loop
}