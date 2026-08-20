-- RoomManager.luau
-- Manages room spawning, layout positioning, door auto-closing, streaming compatibility, and interactions

local Workspace = game:GetService("Workspace")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local TweenService = game:GetService("TweenService")
local Task = task

local GameConfig = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("GameConfig"))
local SoundManager = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("SoundManager"))
local LootTable = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("LootTable"))
local NetworkEvents = require(ReplicatedStorage:WaitForChild("Network"):WaitForChild("NetworkEvents"))
local RoomBuilder = require(script.Parent:WaitForChild("RoomBuilder"))

local RoomManager = {}
local activeRooms = {} -- Map roomNumber -> Model
local roomCFrames = {} -- Map roomNumber -> CFrame position/orientation
local highestRoomOpened = 0
local roomsFolder = nil

function RoomManager.init()
	roomsFolder = Workspace:FindFirstChild("GeneratedRooms")
	if not roomsFolder then
		roomsFolder = Instance.new("Folder")
		roomsFolder.Name = "GeneratedRooms"
		roomsFolder.Parent = Workspace
	end

	-- Pre-generate initial set of rooms
	RoomManager.generateRoom(1)
	for i = 2, GameConfig.PRELOAD_ROOM_COUNT do
		RoomManager.generateRoom(i)
	end
end

function RoomManager.getRoomCFrame(roomNumber: number): CFrame
	if roomCFrames[roomNumber] then
		return roomCFrames[roomNumber]
	end

	if roomNumber == 1 then
		local cf = CFrame.new(0, 0, 0)
		roomCFrames[1] = cf
		return cf
	end

	-- Get previous room CFrame
	local prevCF = RoomManager.getRoomCFrame(roomNumber - 1)
	local prevRoom = activeRooms[roomNumber - 1]
	local layoutType = prevRoom and prevRoom:GetAttribute("LayoutType") or "Straight"

	local offset = Vector3.new(0, 0, GameConfig.ROOM_SIZE.Z)
	local newCF = prevCF * CFrame.new(offset)

	if layoutType == "TurnLeft" then
		newCF = prevCF * CFrame.new(0, 0, GameConfig.ROOM_SIZE.Z / 2) * CFrame.Angles(0, math.rad(90), 0) * CFrame.new(0, 0, GameConfig.ROOM_SIZE.Z / 2)
	elseif layoutType == "TurnRight" then
		newCF = prevCF * CFrame.new(0, 0, GameConfig.ROOM_SIZE.Z / 2) * CFrame.Angles(0, math.rad(-90), 0) * CFrame.new(0, 0, GameConfig.ROOM_SIZE.Z / 2)
	end

	roomCFrames[roomNumber] = newCF
	return newCF
end

function RoomManager.generateRoom(roomNumber: number): Model
	if activeRooms[roomNumber] then
		return activeRooms[roomNumber]
	end

	local isDark = (roomNumber > 2) and (math.random() < 0.3)
	
	-- Pick layout and theme
	local layoutOptions = {"Straight", "Straight", "Straight", "TurnLeft", "TurnRight", "Hallway", "Large"}
	local layoutType = (roomNumber == 1) and "Straight" or layoutOptions[math.random(1, #layoutOptions)]

	local themeOptions = {"Standard", "Standard", "Library", "Storage", "Basement"}
	local themeType = themeOptions[math.random(1, #themeOptions)]

	if roomNumber == GameConfig.FIGURE.LibraryRoomNumber then
		layoutType = "Large"
		themeType = "Library"
	end

	local roomModel = RoomBuilder.buildRoomModel(roomNumber, isDark, layoutType, themeType)

	local roomCF = RoomManager.getRoomCFrame(roomNumber)
	roomModel:PivotTo(roomCF)
	roomModel.Parent = roomsFolder
	activeRooms[roomNumber] = roomModel

	-- Set up Door interaction
	local doorModel = roomModel:WaitForChild("Door") :: Model
	local doorPanel = doorModel:WaitForChild("DoorPanel") :: Part
	local doorPrompt = doorPanel:WaitForChild("DoorPrompt") :: ProximityPrompt

	-- Spawn Key if door is locked
	if doorModel:GetAttribute("IsLocked") then
		RoomManager.spawnKeyForRoom(roomNumber)
	end

	doorPrompt.Triggered:Connect(function(player)
		RoomManager.onDoorInteract(player, roomNumber)
	end)

	-- Set up interaction on Furniture Drawers
	local furniture = roomModel:WaitForChild("Furniture")
	for _, child in ipairs(furniture:GetChildren()) do
		if child.Name == "Drawer" then
			local prompt = child:FindFirstChildOfClass("ProximityPrompt")
			if prompt then
				prompt.Triggered:Connect(function(player)
					RoomManager.onDrawerInteract(player, child)
				end)
			end
		end
	end

	return roomModel
end

function RoomManager.spawnKeyForRoom(roomNumber: number)
	local targetRoomNum = math.max(1, roomNumber - 1)
	local room = activeRooms[targetRoomNum] or activeRooms[roomNumber]
	if not room then return end

	local furniture = room:FindFirstChild("Furniture")
	if not furniture then return end

	local keySpawn = furniture:FindFirstChild("KeySpawnPoint")
	if not keySpawn then return end

	local keyTool = Instance.new("Tool")
	keyTool.Name = "Key Door " .. tostring(roomNumber)
	keyTool.ToolTip = "Unlocks Door " .. tostring(roomNumber)
	keyTool:SetAttribute("TargetDoor", roomNumber)

	local handle = Instance.new("Part")
	handle.Name = "Handle"
	handle.Size = Vector3.new(0.5, 1.2, 0.2)
	handle.CFrame = keySpawn.CFrame
	handle.Color = Color3.fromRGB(240, 200, 50)
	handle.Material = Enum.Material.Metal
	handle.Anchored = false
	handle.CanCollide = false
	handle.Parent = keyTool

	local keyPart = Instance.new("Part")
	keyPart.Name = "DroppedKey"
	keyPart.Size = Vector3.new(1, 0.4, 1)
	keyPart.CFrame = keySpawn.CFrame
	keyPart.Color = Color3.fromRGB(240, 200, 50)
	keyPart.Material = Enum.Material.Metal
	keyPart.Anchored = true
	keyPart.Parent = room

	local keyPrompt = Instance.new("ProximityPrompt")
	keyPrompt.ActionText = "Pick up Key (" .. tostring(roomNumber) .. ")"
	keyPrompt.ObjectText = "Key"
	keyPrompt.Parent = keyPart

	keyPrompt.Triggered:Connect(function(player)
		if player.Character and player.Character:FindFirstChildOfClass("Humanoid") then
			keyTool.Parent = player.Backpack
			keyPart:Destroy()
			SoundManager.playSound("ItemPickup", player.Character.PrimaryPart)
		end
	end)
end

function RoomManager.closeDoor(roomNumber: number)
	local roomModel = activeRooms[roomNumber]
	if not roomModel then return end

	local doorModel = roomModel:FindFirstChild("Door") :: Model
	if not doorModel then return end

	local isOpen = doorModel:GetAttribute("IsOpened")
	if not isOpen then return end

	doorModel:SetAttribute("IsOpened", false)
	local doorPanel = doorModel:FindFirstChild("DoorPanel") :: Part
	if doorPanel then
		local originalCF = doorPanel:GetAttribute("OriginalCFrame") or doorPanel.CFrame
		local tween = TweenService:Create(doorPanel, TweenInfo.new(0.5, Enum.EasingStyle.Quad, Enum.EasingDirection.In), {
			CFrame = originalCF
		})
		tween:Play()
		SoundManager.playSound("DoorClose", doorPanel)

		local setDoorStateEvent = NetworkEvents.getEvent("SetDoorState")
		setDoorStateEvent:FireAllClients(doorPanel, false)
	end
end

function RoomManager.onDoorInteract(player: Player, roomNumber: number)
	local roomModel = activeRooms[roomNumber]
	if not roomModel then return end

	local doorModel = roomModel:FindFirstChild("Door") :: Model
	if not doorModel then return end

	local isOpen = doorModel:GetAttribute("IsOpened")
	if isOpen then return end

	local isLocked = doorModel:GetAttribute("IsLocked")
	if isLocked then
		local character = player.Character
		local holdingTool = character and character:FindFirstChildOfClass("Tool")
		if holdingTool and holdingTool:GetAttribute("TargetDoor") == roomNumber then
			doorModel:SetAttribute("IsLocked", false)
			holdingTool:Destroy()
			SoundManager.playSound("DoorUnlock", doorModel:FindFirstChild("DoorPanel") or doorModel.PrimaryPart)
		else
			local msgEvent = NetworkEvents.getEvent("ShowNotification")
			msgEvent:FireClient(player, "You need Key #" .. tostring(roomNumber) .. "!")
			return
		end
	end

	-- Check Dupe Fake Door Trigger
	if doorModel:GetAttribute("IsDupe") == true then
		SoundManager.playSound("DupeAttack", doorPanel or roomModel.PrimaryPart)
		local character = player.Character
		if character and character:FindFirstChild("Humanoid") then
			character.Humanoid:TakeDamage(GameConfig.DUPE.Damage)
		end
		local jumpscareEvent = NetworkEvents.getEvent("Jumpscare")
		jumpscareEvent:FireClient(player, "Dupe")
		return
	end

	-- Open Door
	doorModel:SetAttribute("IsOpened", true)
	local doorPanel = doorModel:FindFirstChild("DoorPanel") :: Part
	if doorPanel then
		local prompt = doorPanel:FindFirstChildOfClass("ProximityPrompt")
		if prompt then prompt.Enabled = false end

		if not doorPanel:GetAttribute("OriginalCFrame") then
			doorPanel:SetAttribute("OriginalCFrame", doorPanel.CFrame)
		end

		local targetCFrame = doorPanel.CFrame * CFrame.Angles(0, math.rad(-100), 0) * CFrame.new(-3, 0, 1.5)
		local tween = TweenService:Create(doorPanel, TweenInfo.new(0.6, Enum.EasingStyle.Quad, Enum.EasingDirection.Out), {
			CFrame = targetCFrame
		})
		tween:Play()
		SoundManager.playSound("DoorOpen", doorPanel)

		-- Broadcast door opened event for clients/streaming
		local setDoorStateEvent = NetworkEvents.getEvent("SetDoorState")
		setDoorStateEvent:FireAllClients(doorPanel, true)
	end

	-- Auto-close previous door behind player
	if roomNumber > 1 then
		Task.delay(GameConfig.DOOR_AUTO_CLOSE_DELAY, function()
			RoomManager.closeDoor(roomNumber - 1)
		end)
	end

	-- Track progression and spawn upcoming rooms
	if roomNumber > highestRoomOpened then
		highestRoomOpened = roomNumber
		local nextRoomToGenerate = roomNumber + GameConfig.PRELOAD_ROOM_COUNT
		if nextRoomToGenerate <= GameConfig.ROOM_LIMIT then
			RoomManager.generateRoom(nextRoomToGenerate)
		end

		-- Clean up old rooms past DESPAWN_ROOM_OFFSET
		local oldRoomNum = roomNumber - GameConfig.DESPAWN_ROOM_OFFSET
		if oldRoomNum > 0 and activeRooms[oldRoomNum] then
			activeRooms[oldRoomNum]:Destroy()
			activeRooms[oldRoomNum] = nil
		end

		-- Trigger Entity Service on room opened
		local EntityService = require(script.Parent.Parent:WaitForChild("Services"):WaitForChild("EntityService"))
		EntityService.onRoomOpened(roomNumber)
	end
end

function RoomManager.onDrawerInteract(player: Player, drawerPart: Part)
	if drawerPart:GetAttribute("Searched") == true then return end
	drawerPart:SetAttribute("Searched", true)

	local prompt = drawerPart:FindFirstChildOfClass("ProximityPrompt")
	if prompt then prompt.Enabled = false end

	SoundManager.playSound("DrawerOpen", drawerPart)

	-- Chance to spawn Timothy Spider jumpscare
	if math.random() < GameConfig.TIMOTHY.SpawnChanceInDrawer then
		local TimothyEntity = require(script.Parent.Parent:WaitForChild("Entities"):WaitForChild("TimothyEntity"))
		TimothyEntity.trigger(player, drawerPart)
		return
	end

	-- Roll for loot
	local itemName = LootTable.getRandomLoot()
	if itemName then
		local ItemService = require(script.Parent.Parent:WaitForChild("Services"):WaitForChild("ItemService"))
		ItemService.spawnItem(itemName, drawerPart.CFrame + Vector3.new(0, 1, 0), drawerPart.Parent)
		
		local msgEvent = NetworkEvents.getEvent("ShowNotification")
		msgEvent:FireClient(player, "Found " .. itemName .. " in drawer!")
	else
		local msgEvent = NetworkEvents.getEvent("ShowNotification")
		msgEvent:FireClient(player, "Empty drawer...")
	end
end

function RoomManager.getRoom(roomNumber: number): Model?
	return activeRooms[roomNumber]
end

function RoomManager.getHighestRoomOpened(): number
	return highestRoomOpened
end

return RoomManager
