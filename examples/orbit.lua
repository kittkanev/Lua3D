-- Paste into Script Editor and press Run Script.
clear_scene()
local planet = add_object('Planet', 'sphere')
set_color(planet, 0.22, 0.65, 1.0)
set_material(planet, 0.25, 0.4)
local t = 0
function on_update(dt)
    t = t + dt
    set_position(planet, math.cos(t) * 3, 1, math.sin(t) * 3)
    set_rotation(planet, 0, t * 45, 0)
end
