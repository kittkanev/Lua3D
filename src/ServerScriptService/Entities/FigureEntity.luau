-- FigureEntity.luau
-- Blind entity spawning in Library (Room 50). Hunts players based on sound/walking when not crouching!

local Workspace = game:GetService("Workspace")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local TweenService = game:GetService("TweenService")
local Task = task

local GameConfig = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("GameConfig"))
local NetworkEvents = require(ReplicatedStorage:WaitForChild("Network"):WaitForChild("NetworkEvents"))

local FigureEntity = {}

function FigureEntity.spawnInLibrary(libraryRoom: Model)
	Task.spawn(function()
		local figureModel = Instance.new("Model")
		figureModel.Name = "Figure"

		local figureHead = Instance.new("Part")
		figureHead.Name = "Head"
		figureHead.Size = Vector3.new(4, 9, 4)
		figureHead.Color = Color3.fromRGB(110, 30, 20)
		figureHead.Material = Enum.Material.Meat
		figureHead.CanCollide = false
		figureHead.Anchored = true
		figureHead.Position = libraryRoom.PrimaryPart.Position + Vector3.new(0, 5, 0)
		figureHead.Parent = figureModel
		figureModel.PrimaryPart = figureHead

		-- Teeth & Face details
		local faceGui = Instance.new("SurfaceGui")
		faceGui.Face = Enum.NormalId.Front
		faceGui.Parent = figureHead

		local teethLabel = Instance.new("TextLabel")
		teethLabel.Size = UDim2.new(1, 0, 1, 0)
		teethLabel.BackgroundTransparency = 1
		teethLabel.Text = "👹"
		teethLabel.TextScaled = true
		teethLabel.Parent = faceGui

		figureModel.Parent = Workspace

		local isHunting = true
		local targetPos = figureHead.Position

		Task.spawn(function()
			while isHunting do
				local detectedTarget = nil
				local minDistance = 9999

				-- Hear players walking/running (not crouching)
				for _, player in ipairs(Players:GetPlayers()) do
					local character = player.Character
					if character and character:FindFirstChild("HumanoidRootPart") and character:FindFirstChild("Humanoid") then
						local hrp = character.HumanoidRootPart
						local humanoid = character.Humanoid

						local isCrouching = character:GetAttribute("IsCrouching") == true or player:GetAttribute("IsCrouching") == true
						local isMoving = hrp.AssemblyLinearVelocity.Magnitude > 2

						-- Figure hears non-crouched moving players within 70 studs
						local dist = (figureHead.Position - hrp.Position).Magnitude
						if dist < 70 and isMoving and not isCrouching then
							if dist < minDistance then
								minDistance = dist
								detectedTarget = hrp.Position
							end
						end

						-- If player gets within 10 studs, Figure hears heartbeat regardless
						if dist < 10 then
							detectedTarget = hrp.Position
						end

						-- Kill condition
						if dist <= 6 then
							humanoid.Health = 0
							local jumpscareEvent = NetworkEvents.getEvent("Jumpscare")
							jumpscareEvent:FireClient(player, "Figure")
						end
					end
				end

				-- Pathfinding/Patrolling logic
				if detectedTarget then
					targetPos = detectedTarget
				else
					-- Random patrol point inside library
					if (figureHead.Position - targetPos).Magnitude < 4 then
						local roomSize = GameConfig.ROOM_SIZE
						local rx = math.random(-roomSize.X/2 + 5, roomSize.X/2 - 5)
						local rz = math.random(-roomSize.Z/2 + 5, roomSize.Z/2 - 5)
						targetPos = libraryRoom.PrimaryPart.Position + Vector3.new(rx, 5, rz)
					end
				end

				-- Smooth move towards target
				local moveDirection = (targetPos - figureHead.Position).Unit
				local speed = detectedTarget and 14 or 7
				local nextPos = figureHead.Position + (moveDirection * speed * 0.2)
				
				local tween = TweenService:Create(figureHead, TweenInfo.new(0.2, Enum.EasingStyle.Linear), { Position = nextPos })
				tween:Play()

				Task.wait(0.2)
			end
		end)
	end)
end

return FigureEntity
