-- EntityService.luau
-- Coordinates monster spawns based on progression, room opening, and environmental factors

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Players = game:GetService("Players")

local GameConfig = require(ReplicatedStorage:WaitForChild("Modules"):WaitForChild("GameConfig"))
local RushEntity = require(script.Parent.Parent:WaitForChild("Entities"):WaitForChild("RushEntity"))
local AmbushEntity = require(script.Parent.Parent:WaitForChild("Entities"):WaitForChild("AmbushEntity"))
local ScreechEntity = require(script.Parent.Parent:WaitForChild("Entities"):WaitForChild("ScreechEntity"))
local EyesEntity = require(script.Parent.Parent:WaitForChild("Entities"):WaitForChild("EyesEntity"))
local HaltEntity = require(script.Parent.Parent:WaitForChild("Entities"):WaitForChild("HaltEntity"))
local FigureEntity = require(script.Parent.Parent:WaitForChild("Entities"):WaitForChild("FigureEntity"))
local RoomManager = require(script.Parent:WaitForChild("RoomManager"))

local EntityService = {}

function EntityService.init()
	-- Initialization hooks if required
end

function EntityService.onRoomOpened(roomNumber: number)
	local currentRoom = RoomManager.getRoom(roomNumber)
	if not currentRoom then return end

	-- Check for Figure in Library Room (e.g. Room 10 or 50)
	if roomNumber == GameConfig.FIGURE.LibraryRoomNumber or currentRoom:GetAttribute("ThemeType") == "Library" then
		FigureEntity.spawnInLibrary(currentRoom)
		return
	end

	-- Check for Halt spawn
	if roomNumber >= GameConfig.HALT.MinRoomToSpawn and math.random() < GameConfig.HALT.SpawnChance then
		HaltEntity.spawn(roomNumber)
		return
	end

	-- Check for Ambush spawn
	if roomNumber >= GameConfig.AMBUSH.MinRoomToSpawn and math.random() < GameConfig.AMBUSH.SpawnChance then
		AmbushEntity.spawn(roomNumber)
	-- Check for Rush spawn
	elseif roomNumber >= GameConfig.RUSH.MinRoomToSpawn and math.random() < GameConfig.RUSH.SpawnChance then
		RushEntity.spawn(roomNumber)
	end

	-- Check for Eyes spawn
	if roomNumber >= GameConfig.EYES.MinRoomToSpawn and math.random() < GameConfig.EYES.SpawnChance then
		EyesEntity.spawnInRoom(currentRoom)
	end

	-- Check for Screech spawn in dark room
	if roomNumber >= GameConfig.SCREECH.MinRoomToSpawn then
		for _, player in ipairs(Players:GetPlayers()) do
			ScreechEntity.checkAndSpawn(player, currentRoom)
		end
	end
end

return EntityService
