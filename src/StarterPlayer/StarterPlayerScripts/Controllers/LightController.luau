-- LightController.luau
-- Manages lighting atmosphere, atmospheric fog, and client screen effects

local Lighting = game:GetService("Lighting")
local TweenService = game:GetService("TweenService")

local LightController = {}

function LightController.init()
	-- Configure dark hotel atmosphere
	Lighting.Ambient = Color3.fromRGB(15, 15, 20)
	Lighting.OutdoorAmbient = Color3.fromRGB(10, 10, 15)
	Lighting.Brightness = 0.2
	Lighting.ClockTime = 0 -- Midnight
	Lighting.FogColor = Color3.fromRGB(10, 10, 15)
	Lighting.FogEnd = 80
	Lighting.FogStart = 10

	-- Add ColorCorrection for horror feel
	local colorCorrection = Lighting:FindFirstChildOfClass("ColorCorrectionEffect") or Instance.new("ColorCorrectionEffect")
	colorCorrection.Brightness = -0.05
	colorCorrection.Contrast = 0.15
	colorCorrection.Saturation = -0.2
	colorCorrection.Parent = Lighting

	-- Add Bloom
	local bloom = Lighting:FindFirstChildOfClass("BloomEffect") or Instance.new("BloomEffect")
	bloom.Intensity = 0.3
	bloom.Size = 24
	bloom.Threshold = 0.8
	bloom.Parent = Lighting
end

function LightController.flickerEffect(duration: number)
	local origAmbient = Lighting.Ambient
	local origBrightness = Lighting.Brightness

	local startTime = os.clock()
	task.spawn(function()
		while os.clock() - startTime < (duration or 3) do
			Lighting.Ambient = Color3.fromRGB(2, 2, 5)
			Lighting.Brightness = 0.02
			task.wait(math.random() * 0.15)
			Lighting.Ambient = origAmbient
			Lighting.Brightness = origBrightness
			task.wait(math.random() * 0.2)
		end
		Lighting.Ambient = origAmbient
		Lighting.Brightness = origBrightness
	end)
end

return LightController
