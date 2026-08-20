-- CrouchController.luau
-- Handles player crouching/pronation state (Left Control / C key), speed adjustment, and stance attribute for noise detection

local UserInputService = game:GetService("UserInputService")
local Players = game:GetService("Players")
local TweenService = game:GetService("TweenService")

local CrouchController = {}
local player = Players.LocalPlayer

local isCrouched = false
local NORMAL_SPEED = 16
local CROUCH_SPEED = 8
local NORMAL_HIPHEIGHT = 2
local CROUCH_HIPHEIGHT = 0.5

function CrouchController.init()
	UserInputService.InputBegan:Connect(function(input, gpe)
		if gpe then return end

		if input.KeyCode == Enum.KeyCode.LeftControl or input.KeyCode == Enum.KeyCode.C then
			CrouchController.toggleCrouch()
		end
	end)
end

function CrouchController.toggleCrouch()
	isCrouched = not isCrouched
	local character = player.Character
	if not character then return end

	local humanoid = character:FindFirstChildOfClass("Humanoid")
	if not humanoid then return end

	local targetSpeed = isCrouched and CROUCH_SPEED or NORMAL_SPEED
	local targetHipHeight = isCrouched and CROUCH_HIPHEIGHT or NORMAL_HIPHEIGHT

	humanoid.WalkSpeed = targetSpeed
	
	local tween = TweenService:Create(humanoid, TweenInfo.new(0.3, Enum.EasingStyle.Quad, Enum.EasingDirection.Out), {
		HipHeight = targetHipHeight
	})
	tween:Play()

	character:SetAttribute("IsCrouching", isCrouched)
	player:SetAttribute("IsCrouching", isCrouched)
end

function CrouchController.isCrouching(): boolean
	return isCrouched
end

return CrouchController
