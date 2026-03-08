local M = {}

local currentFPS = 0
local frameCount = 0
local timeAccumulator = 0
local font = nil

local cachedText = "FPS: 0"
local cachedColor = "#00FF00"

local config = {
	x = 5,
	y = 16,
	fontPath = "third_party/NotoSans/NotoSans-Regular.ttf",
	fontSize = 16,
	colorGood = "#42f557",
	colorBad = "#f54242",
}

function M.updateFPS(dt)
	frameCount = frameCount + 1
	timeAccumulator = timeAccumulator + dt

	if timeAccumulator >= 0.5 then
		currentFPS = frameCount / timeAccumulator
		frameCount = 0
		timeAccumulator = 0
	end

	cachedText = string.format("FPS: %d", math.floor(currentFPS))
	cachedColor = (currentFPS < 30) and config.colorBad or config.colorGood
end

function M.drawFPS()
	if not font then
		font = vulpis.load_font(config.fontPath, config.fontSize)

		if not font then
			if not M._warned then
				print("[FPS] Error: Could not load font at:", config.fontPath)
				M._warned = true
			end
			return
		end
	end

	vulpis.draw_text(cachedText, font, config.x, config.y, cachedColor)
end

return M
