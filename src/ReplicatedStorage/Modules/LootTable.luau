-- LootTable.luau
-- Weighted random selection utility for drawers and chests

local GameConfig = require(script.Parent:WaitForChild("GameConfig"))

local LootTable = {}

function LootTable.getRandomLoot(): string?
	if math.random() > GameConfig.LOOT_CHANCE then
		return nil
	end

	local totalWeight = 0
	for _, entry in ipairs(GameConfig.LOOT_TABLE) do
		totalWeight += entry.Weight
	end

	local roll = math.random() * totalWeight
	local current = 0

	for _, entry in ipairs(GameConfig.LOOT_TABLE) do
		current += entry.Weight
		if roll <= current then
			return entry.Item
		end
	end

	return GameConfig.LOOT_TABLE[1].Item
end

return LootTable
