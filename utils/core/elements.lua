local elements = {}

if not _G.Vulpis then _G.Vulpis = {} end
if not _G.Vulpis.Input then
	_G.Vulpis.Input = { active = {}, hovered = {}, focused = nil }
end

function _G.Vulpis.getInteractionState(key)
	local hovered = false
	if _G.Vulpis.Input.hovered and _G.Vulpis.Input.hovered[key] then hovered = true end

	local pressed = false
	if _G.Vulpis.Input.active and _G.Vulpis.Input.active[key] then pressed = true end

	local focused = (_G.Vulpis.Input.focused == key)

	return { hovered = hovered, pressed = pressed, focused = focused }
end

local DEFAULT_FONT_PATH = "src/assets/font.ttf"
local DEFAULT_FONT_SIZE = 18

local _systemFont = nil

local function getSystemFont()
	if _systemFont then
		return _systemFont
	end

	if load_font then
		local status, font = pcall(load_font, DEFAULT_FONT_PATH, DEFAULT_FONT_SIZE)

		if status and font then
			_systemFont = font
			return font
		else
			print("[Elements] WARNING: Could not auto-load font at: " .. DEFAULT_FONT_PATH)
		end
	end

	return nil
end

function elements.setSystemFont(font)
	_systemFont = font
end

function elements.mergeStyles(base, override, state)
	local res = {}

	if type(base) == "function" and state then
		base = base(state)
	end
	if type(override) == "function" and state then
		override = override(state)
	end

	for k, v in pairs(base or {}) do
		res[k] = v
	end
	for k, v in pairs(override or {}) do
		res[k] = v
	end
	return res
end

function elements.mergeChildren(listA, listB)
	local res = {}
	if listA then
		for _, child in ipairs(listA) do
			table.insert(res, child)
		end
	end
	if listB then
		for _, child in ipairs(listB) do
			table.insert(res, child)
		end
	end
	return res
end

function elements.Box(props)
	props = props or {}
	local t = props.type or "hbox"

	local node = {
		type = t,
		style = props.style or {},
		resolveStyle = function(self, state)
			if type(self.style) == "function" then
				return self.style(state)
			end
			return self.style or {}
		end,
		children = props.children or {},
		onClick = props.onClick,
		key = props.key,
	}

	return node
end

function elements.VBox(props)
	props = props or {}
	props.type = "vbox"
	return elements.Box(props)
end

function elements.HBox(props)
	props = props or {}
	props.type = "hbox"
	return elements.Box(props)
end

function elements.Text(props)
	if type(props) == "string" then
		props = { text = props }
	end

	props = props or {}
	props.type = "text"
	props.style = props.style or {}

	if not props.style.font then
		props.style.font = getSystemFont()
	end

	if not props.style.color then
		props.style.color = { 255, 255, 255, 255 }
	end

	return props
end

return elements
