-- TimothyEntity.luau
-- Rare spider jumpscare in drawers that deals small damage and scares player

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local NetworkEvents = require(ReplicatedStorage:WaitForChild("Network"):WaitForChild("NetworkEvents"))

local TimothyEntity = {}

function TimothyEntity.trigger(player: Player, drawerPart: Part)
	local character = player.Character
	if character and character:FindFirstChild("Humanoid") then
		character.Humanoid:TakeDamage(10)
	end

	local jumpscareEvent = NetworkEvents.getEvent("Jumpscare")
	jumpscareEvent:FireClient(player, "Timothy Spider")
end

return TimothyEntity
