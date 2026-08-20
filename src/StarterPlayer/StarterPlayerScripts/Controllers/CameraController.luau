-- CameraController.luau
-- Handles first-person head bob, camera shake on monster events, dynamic FOV, and Screech angle verification

local Workspace = game:GetService("Workspace")
local RunService = game:GetService("RunService")
local Players = game:GetService("Players")
local TweenService = game:GetService("TweenService")

local CameraController = {}
local camera = Workspace.CurrentCamera
local player = Players.LocalPlayer

local shakeIntensity = 0
local shakeDecay = 5

local bobTimer = 0
local BOB_SPEED = 10
local BOB_AMOUNT = 0.12

function CameraController.init()
	RunService.RenderStepped:Connect(function(dt)
		local character = player.Character
		if character and character:FindFirstChild("Humanoid") and character:FindFirstChild("HumanoidRootPart") then
			local humanoid = character.Humanoid
			local hrp = character.HumanoidRootPart
			local moveVelocity = hrp.AssemblyLinearVelocity * Vector3.new(1, 0, 1)

			-- Head bobbing effect while moving
			if moveVelocity.Magnitude > 1 and humanoid.FloorMaterial ~= Enum.Material.Air then
				bobTimer += dt * BOB_SPEED
				local bobOffset = Vector3.new(
					math.cos(bobTimer * 0.5) * BOB_AMOUNT,
					math.abs(math.sin(bobTimer)) * BOB_AMOUNT,
					0
				)
				camera.CFrame = camera.CFrame * CFrame.new(bobOffset)
			else
				bobTimer = 0
			end
		end

		-- Camera Shake Effect
		if shakeIntensity > 0 then
			local offsetX = (math.random() - 0.5) * shakeIntensity
			local offsetY = (math.random() - 0.5) * shakeIntensity
			local offsetZ = (math.random() - 0.5) * shakeIntensity

			camera.CFrame = camera.CFrame * CFrame.Angles(math.rad(offsetX), math.rad(offsetY), math.rad(offsetZ))
			shakeIntensity = math.max(0, shakeIntensity - (shakeDecay * dt))
		end
	end)
end

function CameraController.shake(intensity: number, duration: number?)
	shakeIntensity = intensity or 2.0
end

function CameraController.setFOV(targetFOV: number, duration: number?)
	duration = duration or 0.3
	TweenService:Create(camera, TweenInfo.new(duration, Enum.EasingStyle.Quad, Enum.EasingDirection.Out), { FieldOfView = targetFOV }):Play()
end

function CameraController.isLookingAt(targetPosition: Vector3, maxAngleDegrees: number): boolean
	local lookVector = camera.CFrame.LookVector
	local directionToTarget = (targetPosition - camera.CFrame.Position).Unit
	local dot = lookVector:Dot(directionToTarget)
	local angle = math.deg(math.acos(math.clamp(dot, -1, 1)))
	return angle <= (maxAngleDegrees or 45)
end

return CameraController
