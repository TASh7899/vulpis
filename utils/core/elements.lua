local elements = {}

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

function elements.mergeStyles(base, override)
	local res = {}
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
