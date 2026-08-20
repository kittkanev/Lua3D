-- ClientMain.client.luau
-- Main client controller handling network events, UI updates, Screech/Eyes mechanics, and Jumpscares

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local TweenService = game:GetService("TweenService")
local UserInputService = game:GetService("UserInputService")
local SoundService = game:GetService("SoundService")

local NetworkEvents = require(ReplicatedStorage:WaitForChild("Network"):WaitForChild("NetworkEvents"))
local SoundManager = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("SoundManager"))
local GameConfig = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("GameConfig"))

local LightController = require(script.Parent:WaitForChild("Controllers"):WaitForChild("LightController"))
local CameraController = require(script.Parent:WaitForChild("Controllers"):WaitForChild("CameraController"))
local CrouchController = require(script.Parent:WaitForChild("Controllers"):WaitForChild("CrouchController"))
local ShopController = require(script.Parent:WaitForChild("Controllers"):WaitForChild("ShopController"))

local player = Players.LocalPlayer
local playerGui = player:WaitForChild("PlayerGui")

-- Initialize Controllers
LightController.init()
CameraController.init()
CrouchController.init()
ShopController.init()

-- Build Client UI Programmatically
local screenGui = Instance.new("ScreenGui")
screenGui.Name = "DoorsUI"
screenGui.ResetOnSpawn = false
screenGui.Parent = playerGui

-- Notification Banner
local notifLabel = Instance.new("TextLabel")
notifLabel.Name = "Notification"
notifLabel.Size = UDim2.new(0.5, 0, 0.08, 0)
notifLabel.Position = UDim2.new(0.25, 0, 0.1, 0)
notifLabel.BackgroundTransparency = 0.5
notifLabel.BackgroundColor3 = Color3.fromRGB(0, 0, 0)
notifLabel.TextColor3 = Color3.fromRGB(255, 230, 150)
notifLabel.TextScaled = true
notifLabel.Font = Enum.Font.SpecialElite
notifLabel.Text = ""
notifLabel.Visible = false
notifLabel.Parent = screenGui

-- Coins Display UI
local coinsFrame = Instance.new("Frame")
coinsFrame.Name = "CoinsFrame"
coinsFrame.Size = UDim2.new(0.2, 0, 0.06, 0)
coinsFrame.Position = UDim2.new(0.02, 0, 0.9, 0)
coinsFrame.BackgroundColor3 = Color3.fromRGB(20, 20, 20)
coinsFrame.BackgroundTransparency = 0.3
coinsFrame.BorderSizePixel = 0
coinsFrame.Parent = screenGui

local coinsCorner = Instance.new("UICorner")
coinsCorner.CornerRadius = UDim.new(0.2, 0)
coinsCorner.Parent = coinsFrame

local coinIcon = Instance.new("TextLabel")
coinIcon.Size = UDim2.new(0.25, 0, 1, 0)
coinIcon.BackgroundTransparency = 1
coinIcon.Text = "🪙"
coinIcon.TextScaled = true
coinIcon.Parent = coinsFrame

local coinText = Instance.new("TextLabel")
coinText.Size = UDim2.new(0.75, 0, 1, 0)
coinText.Position = UDim2.new(0.25, 0, 0, 0)
coinText.BackgroundTransparency = 1
coinText.TextColor3 = Color3.fromRGB(255, 215, 0)
coinText.TextScaled = true
coinText.Font = Enum.Font.FredokaOne
coinText.TextXAlignment = Enum.TextXAlignment.Left
coinText.Text = "0"
coinText.Parent = coinsFrame

-- Track leaderstats coins for local UI updates
local function setupCoinsListener()
	local leaderstats = player:WaitForChild("leaderstats", 5)
	if leaderstats then
		local coinsStat = leaderstats:WaitForChild("Coins", 5)
		if coinsStat then
			coinText.Text = tostring(coinsStat.Value)
			coinsStat.Changed:Connect(function(val)
				coinText.Text = tostring(val)
				-- Animate scaling
				TweenService:Create(coinsFrame, TweenInfo.new(0.15, Enum.EasingStyle.Quad, Enum.EasingDirection.Out, 0, true), { Size = UDim2.new(0.22, 0, 0.07, 0) }):Play()
			end)
		end
	end
end
task.spawn(setupCoinsListener)

-- Red Vignette (For Hide Warning)
local vignette = Instance.new("ImageLabel")
vignette.Name = "Vignette"
vignette.Size = UDim2.new(1, 0, 1, 0)
vignette.BackgroundTransparency = 1
vignette.Image = "rbxassetid://3570695787" -- Radial vignette texture ID
vignette.ImageColor3 = Color3.fromRGB(255, 0, 0)
vignette.ImageTransparency = 1
vignette.Parent = screenGui

-- Jumpscare Overlay Frame
local jumpscareFrame = Instance.new("Frame")
jumpscareFrame.Name = "JumpscareFrame"
jumpscareFrame.Size = UDim2.new(1, 0, 1, 0)
jumpscareFrame.BackgroundColor3 = Color3.fromRGB(0, 0, 0)
jumpscareFrame.Visible = false
jumpscareFrame.Parent = screenGui

local jumpscareText = Instance.new("TextLabel")
jumpscareText.Size = UDim2.new(0.8, 0, 0.3, 0)
jumpscareText.Position = UDim2.new(0.1, 0, 0.35, 0)
jumpscareText.BackgroundTransparency = 1
jumpscareText.TextColor3 = Color3.fromRGB(255, 0, 0)
jumpscareText.TextScaled = true
jumpscareText.Font = Enum.Font.Creepster
jumpscareText.Text = "YOU DIED"
jumpscareText.Parent = jumpscareFrame

-- Item Equipment Input Keybinds (Keys 1-4, E to consume)
UserInputService.InputBegan:Connect(function(input, gpe)
	if gpe then return end

	if input.KeyCode == Enum.KeyCode.E then
		local character = player.Character
		local tool = character and character:FindFirstChildOfClass("Tool")
		if tool then
			local useEvent = NetworkEvents.getEvent("UseItem")
			useEvent:FireServer(tool.Name)
		end
	end
end)

-- Network Event Handlers
local showNotifEvent = NetworkEvents.getEvent("ShowNotification")
showNotifEvent.OnClientEvent:Connect(function(message: string)
	notifLabel.Text = message
	notifLabel.Visible = true
	task.delay(2.5, function()
		if notifLabel.Text == message then
			notifLabel.Visible = false
		end
	end)
end)

local coinCollectedEvent = NetworkEvents.getEvent("CoinCollected")
coinCollectedEvent.OnClientEvent:Connect(function(val: number)
	SoundManager.playSound("CoinPickup")
	showNotifEvent:Fire("Picked up +" .. tostring(val) .. " Gold!")
end)

local flickerEvent = NetworkEvents.getEvent("FlickerLights")
flickerEvent.OnClientEvent:Connect(function(roomNum: number)
	LightController.flickerEffect(3.0)
	CameraController.shake(1.5, 3.0)
	SoundManager.playSound("RushFlicker")
end)

local hideWarnEvent = NetworkEvents.getEvent("HideWarning")
hideWarnEvent.OnClientEvent:Connect(function(isWarning: boolean)
	if isWarning then
		CameraController.shake(2.0, 5.0)
		SoundManager.playSound("HideWarning")
		TweenService:Create(vignette, TweenInfo.new(0.5), { ImageTransparency = 0.3 }):Play()
	else
		TweenService:Create(vignette, TweenInfo.new(0.5), { ImageTransparency = 1 }):Play()
	end
end)

local jumpscareEvent = NetworkEvents.getEvent("Jumpscare")
jumpscareEvent.OnClientEvent:Connect(function(entityName: string)
	jumpscareText.Text = "KILLED BY " .. string.upper(entityName)
	jumpscareFrame.Visible = true
	CameraController.shake(5.0, 1.5)
	SoundManager.playSound("RushScream")
	task.delay(3.0, function()
		jumpscareFrame.Visible = false
	end)
end)

-- Screech Spawn Mechanic Listener
local screechEvent = NetworkEvents.getEvent("ScreechSpawn")
screechEvent.OnClientEvent:Connect(function()
	SoundManager.playSound("ScreechPsst")

	-- Spawn invisible Screech target behind player camera
	local camera = workspace.CurrentCamera
	local screechOffset = (camera.CFrame.LookVector * -3) + Vector3.new(math.random(-2, 2), math.random(-1, 1), 0)
	local screechTargetPos = camera.CFrame.Position + screechOffset

	local startTime = os.clock()
	local defeated = false

	while os.clock() - startTime < GameConfig.SCREECH.AttackDelay do
		if CameraController.isLookingAt(screechTargetPos, 50) then
			defeated = true
			break
		end
		task.wait(0.1)
	end

	if defeated then
		SoundManager.playSound("ScreechScare")
		showNotifEvent:Fire("Screech ran away!")
	else
		-- Screech attacks
		SoundManager.playSound("ScreechScare")
		CameraController.shake(4.0, 0.5)
		local character = player.Character
		if character and character:FindFirstChild("Humanoid") then
			character.Humanoid:TakeDamage(GameConfig.SCREECH.Damage)
		end
	end
end)

-- Local Door Tween Animation Sync for StreamingEnabled
local setDoorStateEvent = NetworkEvents.getEvent("SetDoorState")
setDoorStateEvent.OnClientEvent:Connect(function(doorPanel: Part, isOpened: boolean)
	if not doorPanel or not doorPanel:IsA("BasePart") then return end

	local targetAngle = isOpened and math.rad(-90) or 0
	local pivotOffset = doorPanel:GetAttribute("HingeCFrame")
	
	if not pivotOffset then
		-- Fallback smooth tween on CFrame
		local originalCF = doorPanel:GetAttribute("OriginalCFrame") or doorPanel.CFrame
		if not doorPanel:GetAttribute("OriginalCFrame") then
			doorPanel:SetAttribute("OriginalCFrame", originalCF)
		end

		local newCF = isOpened and originalCF * CFrame.Angles(0, math.rad(-90), 0) * CFrame.new(-3, 0, 3) or originalCF
		TweenService:Create(doorPanel, TweenInfo.new(0.6, Enum.EasingStyle.Quad, Enum.EasingDirection.Out), { CFrame = newCF }):Play()
		
		if isOpened then
			SoundManager.playSound("DoorOpen", doorPanel)
		else
			SoundManager.playSound("DoorClose", doorPanel)
		end
	end
end)

print("ROBLOX DOORS CLONE CLIENT LOADED SUCCESSFULLY!")

-- Eyes Camera Angle Check Listener
local checkEyesEvent = NetworkEvents.getEvent("CheckEyesLooking")
checkEyesEvent.OnClientEvent:Connect(function(eyesPos: Vector3)
	if CameraController.isLookingAt(eyesPos, GameConfig.EYES.LookAngleThreshold) then
		local character = player.Character
		if character and character:FindFirstChild("Humanoid") then
			character.Humanoid:TakeDamage(GameConfig.EYES.DamagePerSecond / 2)
			CameraController.shake(2.0, 0.3)
		end
	end
end)
