-- RoomBuilder.luau
-- Programmatically generates 3D Room models with diverse layouts, themes, furniture, doors, and coins

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local GameConfig = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("GameConfig"))
local LootTable = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("LootTable"))

local RoomBuilder = {}

-- Helper to make an anchored block part
local function createPart(parent, name, size, cframe, color, material)
	local part = Instance.new("Part")
	part.Name = name
	part.Size = size
	part.CFrame = cframe
	part.Color = color or Color3.fromRGB(80, 80, 80)
	part.Material = material or Enum.Material.SmoothPlastic
	part.Anchored = true
	part.TopSurface = Enum.SurfaceType.Smooth
	part.BottomSurface = Enum.SurfaceType.Smooth
	part.Parent = parent
	return part
end

-- Spawn interactive 3D Gold Coin Pickup
function RoomBuilder.createCoinPickup(parent: Instance, cframe: CFrame, coinValue: number?): Part
	local val = coinValue or math.random(GameConfig.COIN.MIN_VALUE, GameConfig.COIN.MAX_VALUE)
	local coin = createPart(parent, "GoldCoin", Vector3.new(1.2, 0.2, 1.2), cframe, Color3.fromRGB(255, 215, 0), Enum.Material.Metal)
	coin.Shape = Enum.PartType.Cylinder
	coin.Rotation = Vector3.new(0, 0, 90)
	coin.CanCollide = false
	coin:SetAttribute("CoinValue", val)
	coin:SetAttribute("Collected", false)

	local prompt = Instance.new("ProximityPrompt")
	prompt.ObjectText = "Gold Coins"
	prompt.ActionText = "Collect (" .. tostring(val) .. " Gold)"
	prompt.HoldDuration = 0.1
	prompt.MaxActivationDistance = 8
	prompt.RequiresLineOfSight = false
	prompt.Parent = coin

	prompt.Triggered:Connect(function(player)
		local NetworkEvents = require(ReplicatedStorage:WaitForChild("Network"):WaitForChild("NetworkEvents"))
		local collectEvent = NetworkEvents.getEvent("CollectCoin")
		collectEvent:FireServer(coin)
	end)

	return coin
end

function RoomBuilder.buildRoomModel(roomNumber: number, isDark: boolean, layoutType: string?, themeType: string?): Model
	layoutType = layoutType or "Straight" -- Options: Straight, TurnLeft, TurnRight, Hallway, Large
	themeType = themeType or "Standard" -- Options: Standard, Library, Storage, Basement

	local roomModel = Instance.new("Model")
	roomModel.Name = "Room_" .. tostring(roomNumber)
	roomModel:SetAttribute("RoomNumber", roomNumber)
	roomModel:SetAttribute("LayoutType", layoutType)
	roomModel:SetAttribute("ThemeType", themeType)

	local size = GameConfig.ROOM_SIZE
	local halfX = size.X / 2
	local halfY = size.Y / 2
	local halfZ = size.Z / 2

	-- Adjust size for layout
	if layoutType == "Hallway" then
		size = Vector3.new(18, 15, 50)
		halfX = size.X / 2
		halfZ = size.Z / 2
	elseif layoutType == "Large" then
		size = Vector3.new(45, 18, 50)
		halfX = size.X / 2
		halfY = size.Y / 2
		halfZ = size.Z / 2
	end

	-- Theme colors and materials
	local floorColor = Color3.fromRGB(60, 40, 25)
	local wallColor = Color3.fromRGB(110, 95, 80)
	local wallMaterial = Enum.Material.WoodPlanks

	if themeType == "Library" then
		floorColor = Color3.fromRGB(90, 30, 20)
		wallColor = Color3.fromRGB(70, 50, 40)
		wallMaterial = Enum.Material.Wood
	elseif themeType == "Storage" then
		floorColor = Color3.fromRGB(80, 80, 85)
		wallColor = Color3.fromRGB(120, 120, 125)
		wallMaterial = Enum.Material.Concrete
	elseif themeType == "Basement" then
		floorColor = Color3.fromRGB(50, 50, 50)
		wallColor = Color3.fromRGB(70, 70, 75)
		wallMaterial = Enum.Material.Cobblestone
	end

	-- Primary Part / Root Frame
	local floor = createPart(roomModel, "Floor", Vector3.new(size.X, 1, size.Z), CFrame.new(0, 0, 0), floorColor, Enum.Material.WoodPlanks)
	roomModel.PrimaryPart = floor

	-- SpawnLocation for Room 1 to place players inside
	if roomNumber == 1 then
		local spawnLoc = Instance.new("SpawnLocation")
		spawnLoc.Name = "SpawnLocation"
		spawnLoc.Size = Vector3.new(6, 1, 6)
		spawnLoc.CFrame = CFrame.new(0, 2, -10)
		spawnLoc.Transparency = 1
		spawnLoc.CanCollide = false
		spawnLoc.Anchored = true
		spawnLoc.Duration = 0
		spawnLoc.Parent = roomModel
	end

	-- Ceiling
	createPart(roomModel, "Ceiling", Vector3.new(size.X, 1, size.Z), CFrame.new(0, size.Y, 0), Color3.fromRGB(180, 175, 160), Enum.Material.Plaster)

	-- Side Walls
	createPart(roomModel, "LeftWall", Vector3.new(1, size.Y, size.Z), CFrame.new(-halfX, halfY, 0), wallColor, wallMaterial)
	createPart(roomModel, "RightWall", Vector3.new(1, size.Y, size.Z), CFrame.new(halfX, halfY, 0), wallColor, wallMaterial)

	-- Door dimensions
	local doorWidth = 7
	local doorHeight = 10
	local sideWallWidth = (size.X - doorWidth) / 2

	-- Back Wall with Entrance Cutout
	createPart(roomModel, "BackWallLeft", Vector3.new(sideWallWidth, size.Y, 1), CFrame.new(-halfX + sideWallWidth/2, halfY, -halfZ), wallColor, wallMaterial)
	createPart(roomModel, "BackWallRight", Vector3.new(sideWallWidth, size.Y, 1), CFrame.new(halfX - sideWallWidth/2, halfY, -halfZ), wallColor, wallMaterial)
	createPart(roomModel, "BackWallTop", Vector3.new(doorWidth, size.Y - doorHeight, 1), CFrame.new(0, halfY + doorHeight/2, -halfZ), wallColor, wallMaterial)

	-- Exit Cutout Position based on layout
	local exitCF = CFrame.new(0, doorHeight/2, halfZ)
	if layoutType == "TurnLeft" then
		-- Exit on left wall
		exitCF = CFrame.new(-halfX, doorHeight/2, 0) * CFrame.Angles(0, math.rad(90), 0)
		-- Solid front wall
		createPart(roomModel, "FrontWallSolid", Vector3.new(size.X, size.Y, 1), CFrame.new(0, halfY, halfZ), wallColor, wallMaterial)
	elseif layoutType == "TurnRight" then
		-- Exit on right wall
		exitCF = CFrame.new(halfX, doorHeight/2, 0) * CFrame.Angles(0, math.rad(-90), 0)
		-- Solid front wall
		createPart(roomModel, "FrontWallSolid", Vector3.new(size.X, size.Y, 1), CFrame.new(0, halfY, halfZ), wallColor, wallMaterial)
	else
		-- Standard front wall exit
		createPart(roomModel, "FrontWallLeft", Vector3.new(sideWallWidth, size.Y, 1), CFrame.new(-halfX + sideWallWidth/2, halfY, halfZ), wallColor, wallMaterial)
		createPart(roomModel, "FrontWallRight", Vector3.new(sideWallWidth, size.Y, 1), CFrame.new(halfX - sideWallWidth/2, halfY, halfZ), wallColor, wallMaterial)
		createPart(roomModel, "FrontWallTop", Vector3.new(doorWidth, size.Y - doorHeight, 1), CFrame.new(0, halfY + doorHeight/2, halfZ), wallColor, wallMaterial)
	end

	-- Light Fixtures
	local lightsFolder = Instance.new("Folder")
	lightsFolder.Name = "Lights"
	lightsFolder.Parent = roomModel

	local lightPart = createPart(lightsFolder, "CeilingLight", Vector3.new(4, 0.5, 4), CFrame.new(0, size.Y - 0.5, 0), Color3.fromRGB(240, 240, 220), Enum.Material.Neon)
	local pointLight = Instance.new("PointLight")
	pointLight.Name = "LightSource"
	pointLight.Brightness = isDark and 0 or 2.5
	pointLight.Range = 35
	pointLight.Color = Color3.fromRGB(255, 220, 170)
	pointLight.Enabled = not isDark
	pointLight.Parent = lightPart

	-- Exit Door Setup
	local doorModel = Instance.new("Model")
	doorModel.Name = "Door"
	doorModel.Parent = roomModel

	local doorFrame = createPart(doorModel, "DoorFrame", Vector3.new(doorWidth + 0.5, doorHeight + 0.5, 1.2), exitCF, Color3.fromRGB(40, 25, 15), Enum.Material.Wood)
	doorFrame.CanCollide = false

	local doorPanel = createPart(doorModel, "DoorPanel", Vector3.new(doorWidth - 0.2, doorHeight, 0.6), exitCF, Color3.fromRGB(90, 50, 20), Enum.Material.Wood)
	doorPanel.Name = "DoorPanel"

	-- Check for Dupe Fake Door Spawn Chance
	local isDupe = (roomNumber >= GameConfig.DUPE.MinRoomToSpawn) and (math.random() < GameConfig.DUPE.SpawnChance)
	local doorDisplayNumber = isDupe and (roomNumber + math.random(-2, 2)) or roomNumber
	if doorDisplayNumber == roomNumber and isDupe then
		doorDisplayNumber = roomNumber + 1
	end

	doorModel:SetAttribute("IsOpened", false)
	doorModel:SetAttribute("IsLocked", math.random() < GameConfig.KEY_SPAWN_CHANCE)
	doorModel:SetAttribute("DoorNumber", roomNumber)
	doorModel:SetAttribute("IsDupe", isDupe)

	-- Sign
	local sign = Instance.new("SurfaceGui")
	sign.Name = "DoorNumberSign"
	sign.Face = Enum.NormalId.Back
	sign.Parent = doorPanel

	local label = Instance.new("TextLabel")
	label.Size = UDim2.new(1, 0, 1, 0)
	label.BackgroundTransparency = 1
	label.Text = tostring(doorDisplayNumber)
	label.TextColor3 = Color3.fromRGB(255, 215, 0)
	label.TextScaled = true
	label.Font = Enum.Font.SpecialElite
	label.Parent = sign

	-- ProximityPrompt on Door
	local doorPrompt = Instance.new("ProximityPrompt")
	doorPrompt.Name = "DoorPrompt"
	doorPrompt.ObjectText = "Door " .. tostring(doorDisplayNumber)
	doorPrompt.ActionText = doorModel:GetAttribute("IsLocked") and "Locked (Requires Key)" or "Open Door"
	doorPrompt.HoldDuration = 0.2
	doorPrompt.MaxActivationDistance = 7
	doorPrompt.RequiresLineOfSight = false
	doorPrompt.Parent = doorPanel

	-- Furniture Folder
	local furniture = Instance.new("Folder")
	furniture.Name = "Furniture"
	furniture.Parent = roomModel

	-- Key Spawn Point Marker Part
	local keyPoint = Instance.new("Part")
	keyPoint.Name = "KeySpawnPoint"
	keyPoint.Size = Vector3.new(1, 1, 1)
	keyPoint.CFrame = CFrame.new(-halfX + 4, 3, -halfZ + 6)
	keyPoint.Transparency = 1
	keyPoint.Anchored = true
	keyPoint.CanCollide = false
	keyPoint.Parent = furniture

	-- Add Decorative Bookshelves for Library theme
	if themeType == "Library" then
		for b = -1, 1, 2 do
			local shelf = createPart(furniture, "Bookshelf", Vector3.new(3, 12, 10), CFrame.new(b * (halfX - 2), 6, 0), Color3.fromRGB(60, 35, 20), Enum.Material.Wood)
		end
	end

	-- Add Drawers & Tables
	for i = 1, math.random(1, 3) do
		local side = (i % 2 == 0) and 1 or -1
		local tableCF = CFrame.new(side * (halfX - 4), 2, (i * 8) - halfZ + 5)

		local tablePart = createPart(furniture, "Table", Vector3.new(4, 3.5, 3), tableCF, Color3.fromRGB(70, 45, 25), Enum.Material.Wood)

		local drawer = createPart(furniture, "Drawer", Vector3.new(3.6, 1.2, 2.8), tableCF + Vector3.new(0, 0.5, 0.2), Color3.fromRGB(50, 30, 15), Enum.Material.Wood)
		drawer:SetAttribute("Searched", false)

		local drawerPrompt = Instance.new("ProximityPrompt")
		drawerPrompt.ObjectText = "Drawer"
		drawerPrompt.ActionText = "Search Drawer"
		drawerPrompt.HoldDuration = 0.4
		drawerPrompt.MaxActivationDistance = 6
		drawerPrompt.RequiresLineOfSight = false
		drawerPrompt.Parent = drawer

		-- Chance to spawn loose coin on top of table
		if math.random() < GameConfig.COIN.TABLE_SPAWN_CHANCE then
			RoomBuilder.createCoinPickup(furniture, tableCF + Vector3.new(0, 2.0, 0))
		end
	end

	-- Chance to spawn loose coin on floor
	if math.random() < GameConfig.COIN.FLOOR_SPAWN_CHANCE then
		local rx = math.random(-halfX + 4, halfX - 4)
		local rz = math.random(-halfZ + 4, halfZ - 4)
		RoomBuilder.createCoinPickup(furniture, CFrame.new(rx, 0.6, rz))
	end

	-- Add Closets for Hiding
	for i = 1, math.random(1, 2) do
		local xPos = (i == 1) and (-halfX + 3) or (halfX - 3)
		local closetCF = CFrame.new(xPos, 5, 0)

		local closet = createPart(furniture, "Closet", Vector3.new(4, 9, 3.5), closetCF, Color3.fromRGB(45, 25, 15), Enum.Material.Wood)

		local closetPrompt = Instance.new("ProximityPrompt")
		closetPrompt.Name = "ClosetPrompt"
		closetPrompt.ObjectText = "Wardrobe"
		closetPrompt.ActionText = "Hide"
		closetPrompt.HoldDuration = 0.2
		closetPrompt.MaxActivationDistance = 7
		closetPrompt.RequiresLineOfSight = false
		closetPrompt.Parent = closet
	end

	return roomModel
end

return RoomBuilder
