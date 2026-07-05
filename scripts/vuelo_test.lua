-- Test flight script for the FlightComputer module.
-- Defines on_tick(dt) called each simulation step.


-- Activate full throttle when falling fast at low altitude.
function on_tick(dt)
    local alt = get_altitude()
    local vel = get_velocity()

    if vel < -50.0 and alt < 10000.0 then
        set_throttle(1.0)
        print_log("EMERGENCY BURN STARTED")
    else
        set_throttle(0.0)
    end
end
