local M = {}

local currentFPS = 0
local frameCount = 0
local timeAccumulator = 0
local font = nil

local config = {
	x = 5,
	y = 16,
	fontPath = "third_party/NotoSans/NotoSans-Regular.ttf",
	fontSize = 16,
	colorGood = "#00FF00",
	colorBad = "#FF0000",
}

function M.update(dt)
	frameCount = frameCount + 1
	timeAccumulator = timeAccumulator + dt

	if timeAccumulator >= 0.5 then
		currentFPS = frameCount / timeAccumulator
		frameCount = 0
		timeAccumulator = 0
	end
end

function M.draw()
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

	local text = string.format("FPS: %d", math.floor(currentFPS))
	local color = (currentFPS < 30) and config.colorBad or config.colorGood

	vulpis.draw_text(text, font, config.x, config.y, color)
end

function M.enableFPS(options)
	if options then
		for k, v in pairs(options) do
			config[k] = v
		end
	end

	local old_tick = _G.on_tick
	_G.on_tick = function(dt)
		if old_tick then
			old_tick(dt)
		end
		M.update(dt)
	end

	local old_render = _G.on_render
	_G.on_render = function()
		if old_render then
			old_render()
		end
		M.draw()
	end
end

return M
