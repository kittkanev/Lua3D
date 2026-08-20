-- NetworkEvents.luau
-- Helper module to create and retrieve RemoteEvents and RemoteFunctions safely

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")

local NetworkEvents = {}
local folderName = "RemoteEvents"

local function getFolder()
	local folder = ReplicatedStorage:FindFirstChild(folderName)
	if not folder then
		if RunService:IsServer() then
			folder = Instance.new("Folder")
			folder.Name = folderName
			folder.Parent = ReplicatedStorage
		else
			folder = ReplicatedStorage:WaitForChild(folderName, 10)
		end
	end
	return folder
end

function NetworkEvents.getEvent(eventName: string): RemoteEvent
	local folder = getFolder()
	local event = folder:FindFirstChild(eventName)
	if not event then
		if RunService:IsServer() then
			event = Instance.new("RemoteEvent")
			event.Name = eventName
			event.Parent = folder
		else
			event = folder:WaitForChild(eventName, 10) :: RemoteEvent
		end
	end
	return event :: RemoteEvent
end

function NetworkEvents.getFunction(funcName: string): RemoteFunction
	local folder = getFolder()
	local func = folder:FindFirstChild(funcName)
	if not func then
		if RunService:IsServer() then
			func = Instance.new("RemoteFunction")
			func.Name = funcName
			func.Parent = folder
		else
			func = folder:WaitForChild(funcName, 10) :: RemoteFunction
		end
	end
	return func :: RemoteFunction
end

return NetworkEvents
