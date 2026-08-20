-- RushEntity.luau
-- Handles Rush monster spawn, light flickering sequence, room sweeping movement, and player killing mechanics

local Workspace = game:GetService("Workspace")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local TweenService = game:GetService("TweenService")
local Task = task

local GameConfig = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("GameConfig"))
local SoundManager = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("SoundManager"))
local NetworkEvents = require(ReplicatedStorage:WaitForChild("Network"):WaitForChild("NetworkEvents"))

local RushEntity = {}

function RushEntity.spawn(currentRoomNumber: number)
	Task.spawn(function()
		local RoomManager = require(script.Parent.Parent:WaitForChild("Services"):WaitForChild("RoomManager"))

		-- Phase 1: Light Flickering Warning in current and previous rooms
		local lightEvent = NetworkEvents.getEvent("FlickerLights")
		lightEvent:FireAllClients(currentRoomNumber)

		-- Flicker lights server-side
		for i = math.max(1, currentRoomNumber - 2), currentRoomNumber do
			local room = RoomManager.getRoom(i)
			if room then
				local lights = room:FindFirstChild("Lights")
				if lights then
					Task.spawn(function()
						for _ = 1, 6 do
							for _, lightPart in ipairs(lights:GetChildren()) do
								local light = lightPart:FindFirstChildOfClass("PointLight")
								if light then
									light.Enabled = not light.Enabled
								end
							end
							Task.wait(math.random(1, 3) * 0.1)
						end
					end)
				end
			end
		end

		Task.wait(GameConfig.RUSH.FlickerDuration)

		-- Phase 2: Create Rush Model
		local rushModel = Instance.new("Model")
		rushModel.Name = "Rush"

		local part = Instance.new("Part")
		part.Name = "RushHead"
		part.Size = Vector3.new(5, 5, 5)
		part.Color = Color3.fromRGB(10, 10, 10)
		part.Material = Enum.Material.Neon
		part.CanCollide = false
		part.Anchored = true
		part.Parent = rushModel
		rushModel.PrimaryPart = part

		-- Red Glow Face
		local faceGui = Instance.new("SurfaceGui")
		faceGui.Face = Enum.NormalId.Front
		faceGui.Parent = part

		local label = Instance.new("TextLabel")
		label.Size = UDim2.new(1, 0, 1, 0)
		label.BackgroundTransparency = 1
		label.Text = "☠️"
		label.TextScaled = true
		label.TextColor3 = Color3.fromRGB(255, 0, 0)
		label.Parent = faceGui

		local sound = Instance.new("Sound")
		sound.SoundId = "rbxassetid://9114228000"
		sound.Volume = 2
		sound.Looped = true
		sound.Parent = part
		sound:Play()

		rushModel.Parent = Workspace

		-- Phase 3: Sweep through rooms from start room to current room
		local startRoomNum = math.max(1, currentRoomNumber - 4)
		local startRoom = RoomManager.getRoom(startRoomNum)
		local startPos = startRoom and startRoom.PrimaryPart.Position or Vector3.new(0, 5, 0)

		part.Position = startPos + Vector3.new(0, 5, -20)

		-- Check kill loop during movement
		local isMoving = true
		Task.spawn(function()
			while isMoving do
				for _, player in ipairs(Players:GetPlayers()) do
					local character = player.Character
					if character and character:FindFirstChild("HumanoidRootPart") and character:FindFirstChild("Humanoid") then
						local hrp = character.HumanoidRootPart
						local isHidden = player:GetAttribute("IsHidden") == true
						
						-- If player is not hidden and near Rush, kill player with jumpscare
						if not isHidden then
							local dist = (part.Position - hrp.Position).Magnitude
							if dist <= GameConfig.RUSH.KillRadius then
								character.Humanoid.Health = 0
								local jumpscareEvent = NetworkEvents.getEvent("Jumpscare")
								jumpscareEvent:FireClient(player, "Rush")
							end
						end
					end
				end
				Task.wait(0.1)
			end
		end)

		-- Move through each room door by door
		for r = startRoomNum, currentRoomNumber + 1 do
			local room = RoomManager.getRoom(r)
			if room then
				local targetPos = room.PrimaryPart.Position + Vector3.new(0, 5, 0)
				local distance = (part.Position - targetPos).Magnitude
				local travelTime = distance / GameConfig.RUSH.Speed

				local tween = TweenService:Create(part, TweenInfo.new(travelTime, Enum.EasingStyle.Linear), {
					Position = targetPos
				})
				tween:Play()
				tween.Completed:Wait()

				-- Shatter/Break lights in room as Rush passes
				local lights = room:FindFirstChild("Lights")
				if lights then
					for _, lightPart in ipairs(lights:GetChildren()) do
						lightPart.Color = Color3.fromRGB(20, 20, 20)
						local light = lightPart:FindFirstChildOfClass("PointLight")
						if light then
							light.Enabled = false
						end
					end
				end
			end
		end

		isMoving = false
		sound:Stop()
		rushModel:Destroy()
	end)
end

return RushEntity
