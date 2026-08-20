-- HidingService.luau
-- Manages closet hiding, Hide anti-camping mechanics, and Jack closet jumpscare

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")
local Players = game:GetService("Players")

local GameConfig = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("GameConfig"))
local SoundManager = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("SoundManager"))
local NetworkEvents = require(ReplicatedStorage:WaitForChild("Network"):WaitForChild("NetworkEvents"))

local HidingService = {}
local hiddenPlayers = {} -- Map player -> { Closet = Instance, StartTime = number, OriginalCFrame = CFrame }

function HidingService.init()
	local hideEvent = NetworkEvents.getEvent("ToggleHide")

	hideEvent.OnServerEvent:Connect(function(player, closetPart)
		if hiddenPlayers[player] then
			HidingService.exitCloset(player)
		else
			if closetPart and closetPart:IsA("BasePart") then
				-- Check Jack Jumpscare Chance
				if math.random() < GameConfig.JACK.SpawnChanceInCloset then
					SoundManager.playSound("JackScare", closetPart)
					local jumpscareEvent = NetworkEvents.getEvent("Jumpscare")
					jumpscareEvent:FireClient(player, "Jack")
					return
				end

				HidingService.enterCloset(player, closetPart)
			end
		end
	end)

	-- Monitor closet anti-camping timer (Hide entity)
	RunService.Heartbeat:Connect(function()
		local now = os.clock()
		for player, data in pairs(hiddenPlayers) do
			if player and player.Character and player.Character:FindFirstChild("Humanoid") then
				local timeHidden = now - data.StartTime
				
				-- Send warning to client when reaching Hide warning threshold
				if timeHidden >= GameConfig.HIDE.WarningTime and not data.Warned then
					data.Warned = true
					local warnEvent = NetworkEvents.getEvent("HideWarning")
					warnEvent:FireClient(player, true)
				end

				-- Exceeding max closet time results in Hide ejecting & damaging player
				if timeHidden >= GameConfig.HIDE.MaxClosetTime then
					HidingService.ejectPlayer(player)
				end
			else
				hiddenPlayers[player] = nil
			end
		end
	end)
end

function HidingService.enterCloset(player: Player, closetPart: BasePart)
	local character = player.Character
	if not character or not character:FindFirstChild("HumanoidRootPart") then return end

	local hrp = character.HumanoidRootPart
	local originalCFrame = hrp.CFrame

	-- Store hiding data
	hiddenPlayers[player] = {
		Closet = closetPart,
		StartTime = os.clock(),
		OriginalCFrame = originalCFrame,
		Warned = false,
	}

	-- Hide character model and disable collisions
	for _, part in ipairs(character:GetDescendants()) do
		if part:IsA("BasePart") or part:IsA("Decal") then
			part.Transparency = 1
		end
	end
	hrp.Anchored = true
	hrp.CFrame = closetPart.CFrame

	SoundManager.playSound("ClosetEnter", closetPart)

	-- Update attribute on player for server checks
	player:SetAttribute("IsHidden", true)

	local hideEvent = NetworkEvents.getEvent("ToggleHide")
	hideEvent:FireClient(player, true, closetPart)
end

function HidingService.exitCloset(player: Player)
	local data = hiddenPlayers[player]
	if not data then return end

	local character = player.Character
	if character and character:FindFirstChild("HumanoidRootPart") then
		local hrp = character.HumanoidRootPart
		hrp.Anchored = false
		hrp.CFrame = data.OriginalCFrame + Vector3.new(0, 0, 3)

		-- Restore visibility
		for _, part in ipairs(character:GetDescendants()) do
			if part:IsA("BasePart") and part.Name ~= "HumanoidRootPart" then
				part.Transparency = 0
			elseif part:IsA("Decal") then
				part.Transparency = 0
			end
		end
	end

	SoundManager.playSound("ClosetExit", data.Closet)
	player:SetAttribute("IsHidden", false)
	hiddenPlayers[player] = nil

	local warnEvent = NetworkEvents.getEvent("HideWarning")
	warnEvent:FireClient(player, false)

	local hideEvent = NetworkEvents.getEvent("ToggleHide")
	hideEvent:FireClient(player, false)
end

function HidingService.ejectPlayer(player: Player)
	local character = player.Character
	if character and character:FindFirstChild("Humanoid") then
		character.Humanoid:TakeDamage(GameConfig.HIDE.EjectDamage)
	end
	HidingService.exitCloset(player)

	local msgEvent = NetworkEvents.getEvent("ShowNotification")
	msgEvent:FireClient(player, "HIDE kicked you out of the closet!")
end

function HidingService.isPlayerHidden(player: Player): boolean
	return player:GetAttribute("IsHidden") == true
end

return HidingService
