-- HaltEntity.luau
-- Spawns Halt monster in narrow dark hallways. Forces player to turn around when screen flashes cyan.

local Workspace = game:GetService("Workspace")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Task = task

local NetworkEvents = require(ReplicatedStorage:WaitForChild("Network"):WaitForChild("NetworkEvents"))

local HaltEntity = {}

function HaltEntity.spawn(roomNumber: number)
	Task.spawn(function()
		local showNotif = NetworkEvents.getEvent("ShowNotification")
		for _, player in ipairs(Players:GetPlayers()) do
			showNotif:FireClient(player, "TURN AROUND! (HALT APPROACHING)")
		end

		local haltModel = Instance.new("Model")
		haltModel.Name = "Halt"

		local part = Instance.new("Part")
		part.Name = "HaltHead"
		part.Size = Vector3.new(4, 7, 4)
		part.Color = Color3.fromRGB(0, 255, 255)
		part.Material = Enum.Material.Neon
		part.CanCollide = false
		part.Anchored = true
		part.Parent = haltModel
		haltModel.PrimaryPart = part

		local RoomManager = require(script.Parent.Parent:WaitForChild("Services"):WaitForChild("RoomManager"))
		local room = RoomManager.getRoom(roomNumber)
		if room then
			part.Position = room.PrimaryPart.Position + Vector3.new(0, 5, -20)
		end
		haltModel.Parent = Workspace

		Task.wait(8.0)
		haltModel:Destroy()
	end)
end

return HaltEntity
