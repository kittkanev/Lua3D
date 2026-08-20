-- ShopController.luau
-- Pre-Run Shop Interface UI for buying Flashlights, Lighters, Vitamins, Lockpicks, and Batteries with Gold Coins

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local TweenService = game:GetService("TweenService")

local NetworkEvents = require(ReplicatedStorage:WaitForChild("Network"):WaitForChild("NetworkEvents"))
local SoundManager = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("SoundManager"))

local ShopController = {}
local player = Players.LocalPlayer
local playerGui = player:WaitForChild("PlayerGui")

local shopFrame: Frame = nil
local shopPrices = {
	Flashlight = 100,
	Lighter = 50,
	Vitamins = 75,
	Batteries = 25,
	Lockpick = 50
}

function ShopController.init()
	local screenGui = playerGui:WaitForChild("DoorsUI", 5) or Instance.new("ScreenGui")
	screenGui.Name = "DoorsUI"
	screenGui.Parent = playerGui

	-- Open Shop Button
	local openShopBtn = Instance.new("TextButton")
	openShopBtn.Name = "ShopButton"
	openShopBtn.Size = UDim2.new(0.12, 0, 0.06, 0)
	openShopBtn.Position = UDim2.new(0.86, 0, 0.02, 0)
	openShopBtn.BackgroundColor3 = Color3.fromRGB(40, 30, 20)
	openShopBtn.TextColor3 = Color3.fromRGB(255, 215, 0)
	openShopBtn.TextScaled = true
	openShopBtn.Font = Enum.Font.FredokaOne
	openShopBtn.Text = "🛒 SHOP"
	openShopBtn.Parent = screenGui

	local btnCorner = Instance.new("UICorner")
	btnCorner.CornerRadius = UDim.new(0.2, 0)
	btnCorner.Parent = openShopBtn

	-- Shop Main Frame
	shopFrame = Instance.new("Frame")
	shopFrame.Name = "ShopFrame"
	shopFrame.Size = UDim2.new(0.5, 0, 0.65, 0)
	shopFrame.Position = UDim2.new(0.25, 0, 0.18, 0)
	shopFrame.BackgroundColor3 = Color3.fromRGB(25, 20, 15)
	shopFrame.BorderSizePixel = 0
	shopFrame.Visible = false
	shopFrame.Parent = screenGui

	local frameCorner = Instance.new("UICorner")
	frameCorner.CornerRadius = UDim.new(0.05, 0)
	frameCorner.Parent = shopFrame

	-- Title
	local title = Instance.new("TextLabel")
	title.Size = UDim2.new(1, 0, 0.15, 0)
	title.BackgroundTransparency = 1
	title.Text = "PRE-RUN SHOP"
	title.TextColor3 = Color3.fromRGB(255, 215, 0)
	title.TextScaled = true
	title.Font = Enum.Font.SpecialElite
	title.Parent = shopFrame

	-- Close Button
	local closeBtn = Instance.new("TextButton")
	closeBtn.Size = UDim2.new(0.08, 0, 0.12, 0)
	closeBtn.Position = UDim2.new(0.9, 0, 0.02, 0)
	closeBtn.BackgroundTransparency = 1
	closeBtn.Text = "❌"
	closeBtn.TextScaled = true
	closeBtn.Parent = shopFrame

	closeBtn.MouseButton1Click:Connect(function()
		shopFrame.Visible = false
	end)

	openShopBtn.MouseButton1Click:Connect(function()
		shopFrame.Visible = not shopFrame.Visible
	end)

	-- Items Layout Container
	local itemsContainer = Instance.new("Frame")
	itemsContainer.Size = UDim2.new(0.9, 0, 0.75, 0)
	itemsContainer.Position = UDim2.new(0.05, 0, 0.2, 0)
	itemsContainer.BackgroundTransparency = 1
	itemsContainer.Parent = shopFrame

	local uigrid = Instance.new("UIGridLayout")
	uigrid.CellSize = UDim2.new(0.3, 0, 0.45, 0)
	uigrid.CellPadding = UDim2.new(0.03, 0, 0.05, 0)
	uigrid.Parent = itemsContainer

	-- Populate Shop Items
	for itemName, price in pairs(shopPrices) do
		local card = Instance.new("Frame")
		card.BackgroundColor3 = Color3.fromRGB(45, 35, 25)
		card.Parent = itemsContainer

		local cardCorner = Instance.new("UICorner")
		cardCorner.CornerRadius = UDim.new(0.1, 0)
		cardCorner.Parent = card

		local nameLabel = Instance.new("TextLabel")
		nameLabel.Size = UDim2.new(1, 0, 0.3, 0)
		nameLabel.BackgroundTransparency = 1
		nameLabel.Text = itemName
		nameLabel.TextColor3 = Color3.fromRGB(240, 240, 240)
		nameLabel.TextScaled = true
		nameLabel.Font = Enum.Font.SourceSansBold
		nameLabel.Parent = card

		local buyBtn = Instance.new("TextButton")
		buyBtn.Size = UDim2.new(0.8, 0, 0.4, 0)
		buyBtn.Position = UDim2.new(0.1, 0, 0.5, 0)
		buyBtn.BackgroundColor3 = Color3.fromRGB(200, 160, 40)
		buyBtn.Text = price .. " 🪙"
		buyBtn.TextColor3 = Color3.fromRGB(0, 0, 0)
		buyBtn.TextScaled = true
		buyBtn.Font = Enum.Font.FredokaOne
		buyBtn.Parent = card

		local buyCorner = Instance.new("UICorner")
		buyCorner.CornerRadius = UDim.new(0.2, 0)
		buyCorner.Parent = buyBtn

		buyBtn.MouseButton1Click:Connect(function()
			local purchaseEvent = NetworkEvents.getEvent("PurchaseItem")
			purchaseEvent:FireServer(itemName)
		end)
	end
end

return ShopController
