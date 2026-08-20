-- Shared Game Settings and Constants for Doors Clone

local GameConfig = {
	-- Room Generation Configuration
	ROOM_LIMIT = 100,
	ROOM_SIZE = Vector3.new(30, 15, 40), -- Base dimensions for standard rooms
	PRELOAD_ROOM_COUNT = 5, -- Number of generated rooms to keep active ahead
	DESPAWN_ROOM_OFFSET = 3, -- Rooms past this offset behind player will despawn

	-- Door Configuration
	KEY_SPAWN_CHANCE = 0.35, -- Chance a door is locked and requires a key
	DOOR_AUTO_CLOSE_DELAY = 1.0, -- Delay in seconds before previous door closes behind player

	-- Coin Settings
	COIN = {
		MIN_VALUE = 5,
		MAX_VALUE = 25,
		LOOT_CHANCE_DRAWER = 0.60, -- Chance to get coins from a drawer
		TABLE_SPAWN_CHANCE = 0.40, -- Chance coins spawn on tables/shelves
		FLOOR_SPAWN_CHANCE = 0.25, -- Chance coins spawn on floor
	},

	-- Item Settings
	ITEMS = {
		Flashlight = {
			Name = "Flashlight",
			MaxBattery = 100,
			DrainRate = 2.5, -- Battery drain per second
			LightRange = 40,
			LightBrightness = 3,
			Color = Color3.fromRGB(255, 240, 200),
		},
		Lighter = {
			Name = "Lighter",
			MaxFuel = 100,
			DrainRate = 1.2,
			LightRange = 18,
			LightBrightness = 1.5,
			Color = Color3.fromRGB(255, 170, 80),
		},
		Vitamins = {
			Name = "Vitamins",
			SpeedBoost = 12, -- Additional WalkSpeed
			Duration = 10, -- Seconds
		},
		Key = {
			Name = "Key",
		},
		Batteries = {
			Name = "Batteries",
			RestoreAmount = 50,
		}
	},

	-- Drawers Loot Settings
	LOOT_CHANCE = 0.6,
	LOOT_TABLE = {
		{ Item = "Coins", Weight = 40 },
		{ Item = "Flashlight", Weight = 10 },
		{ Item = "Lighter", Weight = 20 },
		{ Item = "Vitamins", Weight = 15 },
		{ Item = "Batteries", Weight = 15 },
	},

	-- Entity Settings
	RUSH = {
		MinRoomToSpawn = 3,
		SpawnChance = 0.20,
		FlickerDuration = 3.0,
		Speed = 75, -- Studs per second movement through doors
		KillRadius = 12,
		JumpscareDuration = 2.0,
	},

	AMBUSH = {
		MinRoomToSpawn = 8,
		SpawnChance = 0.12,
		FlickerDuration = 4.0,
		Speed = 100, -- Faster than Rush
		KillRadius = 12,
		MinRebounds = 2,
		MaxRebounds = 4,
	},

	EYES = {
		MinRoomToSpawn = 4,
		SpawnChance = 0.25,
		DamagePerSecond = 20,
		LookAngleThreshold = 45, -- Degrees camera angle looking at Eyes to trigger damage
	},

	DUPE = {
		MinRoomToSpawn = 5,
		SpawnChance = 0.30,
		Damage = 45,
	},

	JACK = {
		SpawnChanceInCloset = 0.08, -- 8% chance to jump scare on closet enter
	},

	SCREECH = {
		MinRoomToSpawn = 2,
		SpawnChanceInDark = 0.45,
		AttackDelay = 2.5, -- Time player has to look at Screech before damage
		Damage = 40,
	},

	HIDE = {
		MaxClosetTime = 12.0, -- Max time allowed in closet before Hide ejects player
		WarningTime = 8.0, -- Time when screen starts shaking and red vignette appears
		EjectDamage = 40,
	},

	HALT = {
		MinRoomToSpawn = 7,
		SpawnChance = 0.20,
	},

	TIMOTHY = {
		SpawnChanceInDrawer = 0.15,
	},

	FIGURE = {
		LibraryRoomNumber = 10, -- Featured Library Room
	},

	PLAYER_BASE_SPEED = 16,
	PLAYER_MAX_HEALTH = 100,
}

return GameConfig
