#pragma once
static const char kCollectScript[]=R"COLLECT_SCRIPT(-- Third-person sample. This runs in Studio Play and in the packed Game.exe.
local player = find_object('Player')
assert(player >= 0, 'Player is missing')
follow_camera(player, 7, 24)
game_score(0)
hud_text('WASD move | Space jump | R restart | Collect all 3 gems')
local elapsed = 0
function on_update(dt)
    elapsed = elapsed + dt
    if input_key('R') then restart_game(); return end
    local px, py, pz = get_position(player)
    for gem = 1, 3 do
        local index = find_object('Gem ' .. gem)
        if index >= 0 and not get_property(index, 'Collected') then
            local x, y, z = get_position(index)
            set_rotation(index, 0, elapsed * 70, 0)
            if (px-x)^2 + (py-y+0.7)^2 + (pz-z)^2 < 1.25^2 then
                set_property(index, 'Collected', true)
                set_visible(index, false)
                set_position(index, x, -100, z) -- remove the pickup collider
                game_score(game_score()+1)
                play_sound(0)
            end
        end
    end
    if py < -10 then restart_game() end
    if game_score() == 3 then hud_text('Level complete! Press R to play again.') end
end
)COLLECT_SCRIPT";
