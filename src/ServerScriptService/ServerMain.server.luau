-- ServerMain.server.luau
-- Main entry point for ServerScriptService initialization

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local RoomManager = require(script.Parent:WaitForChild("Services"):WaitForChild("RoomManager"))
local ItemService = require(script.Parent:WaitForChild("Services"):WaitForChild("ItemService"))
local HidingService = require(script.Parent:WaitForChild("Services"):WaitForChild("HidingService"))
local EntityService = require(script.Parent:WaitForChild("Services"):WaitForChild("EntityService"))
local NetworkEvents = require(ReplicatedStorage:WaitForChild("Network"):WaitForChild("NetworkEvents"))

-- Set up Leaderstats and Coin tracking for connecting players
Players.PlayerAdded:Connect(function(player)
	local leaderstats = Instance.new("Folder")
	leaderstats.Name = "leaderstats"
	leaderstats.Parent = player

	local coinsStat = Instance.new("IntValue")
	coinsStat.Name = "Coins"
	coinsStat.Value = 0
	coinsStat.Parent = leaderstats

	player.CharacterAdded:Connect(function(character)
		local humanoid = character:WaitForChild("Humanoid") :: Humanoid
		humanoid.Died:Connect(function()
			print(player.Name .. " died.")
		end)
	end)
end)

-- Initialize Game Services
RoomManager.init()
ItemService.init()
HidingService.init()
EntityService.init()

-- Set up remote handler for coin collection
local collectCoinEvent = NetworkEvents.getEvent("CollectCoin")
collectCoinEvent.OnServerEvent:Connect(function(player, coinPart)
	if not coinPart or not coinPart:IsA("BasePart") then return end
	if not coinPart:IsDescendantOf(workspace) then return end
	if coinPart:GetAttribute("Collected") == true then return end

	local coinValue = coinPart:GetAttribute("CoinValue") or math.random(5, 15)
	
	-- Verify proximity
	local character = player.Character
	if character and character:FindFirstChild("HumanoidRootPart") then
		local dist = (character.HumanoidRootPart.Position - coinPart.Position).Magnitude
		if dist <= 15 then
			coinPart:SetAttribute("Collected", true)
			
			local leaderstats = player:FindFirstChild("leaderstats")
			if leaderstats then
				local coinsStat = leaderstats:FindFirstChild("Coins")
				if coinsStat then
					coinsStat.Value += coinValue
				end
			end

			-- Notify client
			local coinCollectedClient = NetworkEvents.getEvent("CoinCollected")
			coinCollectedClient:FireClient(player, coinValue)

			coinPart:Destroy()
		end
	end
end)

print("ROBLOX DOORS CLONE SERVER INITIALIZED SUCCESSFULLY!")
