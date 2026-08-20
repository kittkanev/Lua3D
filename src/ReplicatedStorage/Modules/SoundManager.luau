-- SoundManager.luau
-- Shared audio player utility

local SoundService = game:GetService("SoundService")

local SoundManager = {}

local sounds = {
	DoorOpen = { SoundId = "rbxassetid://9114223178", Volume = 0.8 },
	DoorClose = { SoundId = "rbxassetid://9114223178", Volume = 0.8 },
	DoorUnlock = { SoundId = "rbxassetid://9114221234", Volume = 0.7 },
	DrawerOpen = { SoundId = "rbxassetid://9114225000", Volume = 0.6 },
	ItemPickup = { SoundId = "rbxassetid://9114226000", Volume = 0.5 },
	CoinPickup = { SoundId = "rbxassetid://9114226000", Volume = 0.6 },
	RushFlicker = { SoundId = "rbxassetid://9114227000", Volume = 1.0 },
	RushScream = { SoundId = "rbxassetid://9114228000", Volume = 1.0 },
	AmbushScream = { SoundId = "rbxassetid://9114228000", Volume = 1.2 },
	ScreechPsst = { SoundId = "rbxassetid://9114229000", Volume = 0.9 },
	ScreechScare = { SoundId = "rbxassetid://9114230000", Volume = 1.0 },
	HideWarning = { SoundId = "rbxassetid://9114231000", Volume = 0.8 },
	ClosetEnter = { SoundId = "rbxassetid://9114232000", Volume = 0.7 },
	ClosetExit = { SoundId = "rbxassetid://9114233000", Volume = 0.7 },
	EyesWhisper = { SoundId = "rbxassetid://9114229000", Volume = 0.8 },
	DupeAttack = { SoundId = "rbxassetid://9114228000", Volume = 1.0 },
	JackScare = { SoundId = "rbxassetid://9114228000", Volume = 1.0 },
}

function SoundManager.playSound(soundName: string, parent: Instance?)
	local data = sounds[soundName]
	local sound = Instance.new("Sound")
	sound.Name = soundName
	if data then
		sound.SoundId = data.SoundId
		sound.Volume = data.Volume
	else
		sound.SoundId = soundName
		sound.Volume = 0.5
	end
	sound.Parent = parent or SoundService
	sound:Play()
	sound.Ended:Connect(function()
		sound:Destroy()
	end)
	return sound
end

return SoundManager
