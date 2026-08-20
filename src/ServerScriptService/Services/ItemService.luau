-- ItemService.luau
-- Manages item usage, Vitamin speed boosts, Battery charges, and item pickups

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Players = game:GetService("Players")

local GameConfig = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("GameConfig"))
local NetworkEvents = require(ReplicatedStorage:WaitForChild("Network"):WaitForChild("NetworkEvents"))

local ItemService = {}


local itemPrices = {
	Flashlight = 100,
	Lighter = 50,
	Vitamins = 75,
	Batteries = 25,
	Lockpick = 50
}

function ItemService.init()
	local purchaseEvent = NetworkEvents.getEvent("PurchaseItem")
	purchaseEvent.OnServerEvent:Connect(function(player, itemName)
		local price = itemPrices[itemName]
		if not price then return end
		local leaderstats = player:FindFirstChild("leaderstats")
		local coins = leaderstats and leaderstats:FindFirstChild("Coins")
		if coins and coins.Value >= price then
			coins.Value -= price
			local tool = ItemService.spawnItem(itemName, CFrame.new(), player.Backpack)
			local showNotif = NetworkEvents.getEvent("ShowNotification")
			showNotif:FireClient(player, "Purchased " .. itemName .. "!")
		end
	end)

	local useItemEvent = NetworkEvents.getEvent("UseItem")

	useItemEvent.OnServerEvent:Connect(function(player, itemName)
		local character = player.Character
		if not character then return end

		local tool = character:FindFirstChild(itemName)
		if not tool then return end

		if itemName == "Vitamins" then
			local humanoid = character:FindFirstChildOfClass("Humanoid")
			if humanoid then
				humanoid.WalkSpeed = GameConfig.PLAYER_BASE_SPEED + GameConfig.ITEMS.Vitamins.SpeedBoost
				tool:Destroy()

				task.delay(GameConfig.ITEMS.Vitamins.Duration, function()
					if humanoid and humanoid.Parent then
						humanoid.WalkSpeed = GameConfig.PLAYER_BASE_SPEED
					end
				end)
			end
		elseif itemName == "Batteries" then
			local flashlight = character:FindFirstChild("Flashlight")
			if flashlight then
				flashlight:SetAttribute("Battery", GameConfig.ITEMS.Flashlight.MaxBattery)
				tool:Destroy()
			end
		end
	end)
end

function ItemService.spawnItem(itemName: string, cframe: CFrame, parent: Instance): Tool?
	if itemName == "Coins" then
		local RoomBuilder = require(script.Parent:WaitForChild("RoomBuilder"))
		RoomBuilder.createCoinPickup(parent, cframe)
		return nil
	end

	local tool = Instance.new("Tool")
	tool.Name = itemName
	tool.CanBeDropped = true

	local handle = Instance.new("Part")
	handle.Name = "Handle"
	handle.Size = Vector3.new(1, 1, 1)
	handle.CFrame = cframe
	handle.Color = Color3.fromRGB(150, 150, 150)
	handle.Material = Enum.Material.Metal
	handle.Parent = tool

	if itemName == "Flashlight" then
		handle.Color = Color3.fromRGB(30, 30, 30)
		tool:SetAttribute("Battery", GameConfig.ITEMS.Flashlight.MaxBattery)
		
		local spot = Instance.new("SpotLight")
		spot.Name = "LightSource"
		spot.Enabled = false
		spot.Range = GameConfig.ITEMS.Flashlight.LightRange
		spot.Brightness = GameConfig.ITEMS.Flashlight.LightBrightness
		spot.Color = GameConfig.ITEMS.Flashlight.Color
		spot.Parent = handle

	elseif itemName == "Lighter" then
		handle.Color = Color3.fromRGB(180, 50, 50)
		tool:SetAttribute("Fuel", GameConfig.ITEMS.Lighter.MaxFuel)

		local point = Instance.new("PointLight")
		point.Name = "LightSource"
		point.Enabled = false
		point.Range = GameConfig.ITEMS.Lighter.LightRange
		point.Brightness = GameConfig.ITEMS.Lighter.LightBrightness
		point.Color = GameConfig.ITEMS.Lighter.Color
		point.Parent = handle
	end

	tool.Parent = parent
	return tool
end

return ItemService
