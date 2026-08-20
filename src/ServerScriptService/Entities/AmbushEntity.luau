-- AmbushEntity.luau
-- Handles Ambush monster spawn, aggressive green flicker sequence, multi-pass rebound sweeping movement, and player killing mechanics

local Workspace = game:GetService("Workspace")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local TweenService = game:GetService("TweenService")
local Task = task

local GameConfig = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("GameConfig"))
local SoundManager = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("SoundManager"))
local NetworkEvents = require(ReplicatedStorage:WaitForChild("Network"):WaitForChild("NetworkEvents"))

local AmbushEntity = {}

function AmbushEntity.spawn(currentRoomNumber: number)
	Task.spawn(function()
		local RoomManager = require(script.Parent.Parent:WaitForChild("Services"):WaitForChild("RoomManager"))

		-- Phase 1: Heavy Flicker Warning
		local lightEvent = NetworkEvents.getEvent("FlickerLights")
		lightEvent:FireAllClients(currentRoomNumber)

		for i = math.max(1, currentRoomNumber - 2), currentRoomNumber do
			local room = RoomManager.getRoom(i)
			if room then
				local lights = room:FindFirstChild("Lights")
				if lights then
					Task.spawn(function()
						for _ = 1, 10 do
							for _, lightPart in ipairs(lights:GetChildren()) do
								local light = lightPart:FindFirstChildOfClass("PointLight")
								if light then
									light.Enabled = not light.Enabled
									light.Color = Color3.fromRGB(100, 255, 100)
								end
							end
							Task.wait(math.random(1, 2) * 0.1)
						end
					end)
				end
			end
		end

		Task.wait(GameConfig.AMBUSH.FlickerDuration)

		-- Phase 2: Create Ambush Model
		local ambushModel = Instance.new("Model")
		ambushModel.Name = "Ambush"

		local part = Instance.new("Part")
		part.Name = "AmbushHead"
		part.Size = Vector3.new(6, 6, 6)
		part.Color = Color3.fromRGB(0, 255, 100)
		part.Material = Enum.Material.Neon
		part.CanCollide = false
		part.Anchored = true
		part.Parent = ambushModel
		ambushModel.PrimaryPart = part

		local faceGui = Instance.new("SurfaceGui")
		faceGui.Face = Enum.NormalId.Front
		faceGui.Parent = part

		local label = Instance.new("TextLabel")
		label.Size = UDim2.new(1, 0, 1, 0)
		label.BackgroundTransparency = 1
		label.Text = "👹"
		label.TextScaled = true
		label.TextColor3 = Color3.fromRGB(0, 255, 0)
		label.Parent = faceGui

		local sound = Instance.new("Sound")
		sound.SoundId = "rbxassetid://9114228000"
		sound.Volume = 2.5
		sound.Looped = true
		sound.Parent = part
		sound:Play()

		ambushModel.Parent = Workspace

		-- Phase 3: Rebound Sweeps
		local rebounds = math.random(GameConfig.AMBUSH.MinRebounds, GameConfig.AMBUSH.MaxRebounds)
		local startRoomNum = math.max(1, currentRoomNumber - 5)

		local isMoving = true
		Task.spawn(function()
			while isMoving do
				for _, player in ipairs(Players:GetPlayers()) do
					local character = player.Character
					if character and character:FindFirstChild("HumanoidRootPart") and character:FindFirstChild("Humanoid") then
						local hrp = character.HumanoidRootPart
						local isHidden = player:GetAttribute("IsHidden") == true
						
						if not isHidden then
							local dist = (part.Position - hrp.Position).Magnitude
							if dist <= GameConfig.AMBUSH.KillRadius then
								character.Humanoid.Health = 0
								local jumpscareEvent = NetworkEvents.getEvent("Jumpscare")
								jumpscareEvent:FireClient(player, "Ambush")
							end
						end
					end
				end
				Task.wait(0.08)
			end
		end)

		for pass = 1, rebounds do
			local forward = (pass % 2 == 1)
			local fromRoom = forward and startRoomNum or (currentRoomNumber + 1)
			local toRoom = forward and (currentRoomNumber + 1) or startRoomNum
			local step = forward and 1 or -1

			for r = fromRoom, toRoom, step do
				local room = RoomManager.getRoom(r)
				if room then
					local targetPos = room.PrimaryPart.Position + Vector3.new(0, 5, 0)
					local distance = (part.Position - targetPos).Magnitude
					local travelTime = distance / GameConfig.AMBUSH.Speed

					local tween = TweenService:Create(part, TweenInfo.new(travelTime, Enum.EasingStyle.Linear), {
						Position = targetPos
					})
					tween:Play()
					tween.Completed:Wait()
				end
			end
			Task.wait(0.5) -- Pause briefly before rebounding
		end

		isMoving = false
		sound:Stop()
		ambushModel:Destroy()
	end)
end

return AmbushEntity
