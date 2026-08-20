-- ScreechEntity.luau
-- Handles Screech spawning in dark rooms and testing if player turns camera towards Screech in time

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Task = task

local GameConfig = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("GameConfig"))
local SoundManager = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("SoundManager"))
local NetworkEvents = require(ReplicatedStorage:WaitForChild("Network"):WaitForChild("NetworkEvents"))

local ScreechEntity = {}

function ScreechEntity.checkAndSpawn(player: Player, room: Model)
	local lights = room:FindFirstChild("Lights")
	local isDark = false

	if lights then
		local lightPart = lights:FindFirstChild("CeilingLight")
		if lightPart then
			local light = lightPart:FindFirstChildOfClass("PointLight")
			if light and not light.Enabled then
				isDark = true
			end
		end
	end

	if isDark and math.random() < GameConfig.SCREECH.SpawnChanceInDark then
		Task.delay(math.random(2, 5), function()
			if not player or not player.Character then return end
			
			-- Fire Screech client event to initiate sound "Psst!" and look check
			local screechEvent = NetworkEvents.getEvent("ScreechSpawn")
			screechEvent:FireClient(player)
		end)
	end
end

return ScreechEntity
