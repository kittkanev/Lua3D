-- EyesEntity.luau
-- Spawns floating Eyes entity in a room. Deals periodic damage if players look at it.

local Workspace = game:GetService("Workspace")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")

local GameConfig = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("GameConfig"))
local SoundManager = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("SoundManager"))
local NetworkEvents = require(ReplicatedStorage:WaitForChild("Network"):WaitForChild("NetworkEvents"))

local EyesEntity = {}

function EyesEntity.spawnInRoom(room: Model)
	local eyesModel = Instance.new("Model")
	eyesModel.Name = "Eyes"

	local part = Instance.new("Part")
	part.Name = "EyesCore"
	part.Size = Vector3.new(4, 4, 4)
	part.Shape = Enum.PartType.Ball
	part.Color = Color3.fromRGB(180, 0, 255)
	part.Material = Enum.Material.Neon
	part.CanCollide = false
	part.Anchored = true
	part.Position = room.PrimaryPart.Position + Vector3.new(0, 6, 0)
	part.Parent = eyesModel

	local light = Instance.new("PointLight")
	light.Color = Color3.fromRGB(200, 50, 255)
	light.Range = 25
	light.Brightness = 3
	light.Parent = part

	local gui = Instance.new("SurfaceGui")
	gui.Face = Enum.NormalId.Front
	gui.Parent = part

	local label = Instance.new("TextLabel")
	label.Size = UDim2.new(1, 0, 1, 0)
	label.BackgroundTransparency = 1
	label.Text = "👁️"
	label.TextScaled = true
	label.Parent = gui

	eyesModel.Parent = Workspace

	local sound = SoundManager.playSound("EyesWhisper", part)

	-- Request client angle validation and apply damage
	local active = true
	task.spawn(function()
		while active and eyesModel.Parent do
			for _, player in ipairs(Players:GetPlayers()) do
				local character = player.Character
				if character and character:FindFirstChild("Humanoid") and character:FindFirstChild("HumanoidRootPart") then
					-- Check if player is near Eyes
					local dist = (character.HumanoidRootPart.Position - part.Position).Magnitude
					if dist <= 40 then
						-- Trigger client damage check event
						local eyesCheck = NetworkEvents.getEvent("CheckEyesLooking")
						eyesCheck:FireClient(player, part.Position)
					end
				end
			end
			task.wait(0.5)
		end
	end)

	-- Destroy Eyes when room despawns or player advances far
	room.AncestryChanged:Connect(function()
		if not room:IsDescendantOf(Workspace) then
			active = false
			eyesModel:Destroy()
		end
	end)
end

return EyesEntity
