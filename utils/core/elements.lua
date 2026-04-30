local elements = {}

---@class VulpisProps
---@field id? string
---@field key? string
---@field w? number|string Width (e.g., 200 or "100%")
---@field h? number|string Height
---@field p? integer Padding (all sides)
---@field px? integer Padding Horizontal (Left/Right)
---@field py? integer Padding Vertical (Top/Bottom)
---@field m? integer Margin (all sides)
---@field bg? VulpisColor Background color (Hex or table)
---@field rounded? number Border radius
---@field center? boolean Automatically center all children
---@field row? boolean Lay out children horizontally (like Flex Row)
---@field justify? FlexJustify Main-axis alignment
---@field align? FlexAlign Cross-axis alignment
---@field gap? integer Spacing between children
---@field onClick? fun(mx: number, my: number)
---@field onRightClick? fun(mx: number, my: number)
---@field onMouseEnter? fun()
---@field onMouseLeave? fun()
---@field children? VulpisNode[]
---@field style? VulpisStyle Fallback for explicit verbose styles

function elements.mergeStyles(...)
	local res = {}
	for _, styleObj in ipairs({ ... }) do
		if type(styleObj) == "table" then
			for k, v in pairs(styleObj) do
				res[k] = v
			end
		end
	end
	return res
end

-- Helper to merge child lists
function elements.mergeChildren(...)
	local res = {}
	for _, childList in ipairs({ ... }) do
		if type(childList) == "table" then
			for _, child in ipairs(childList) do
				table.insert(res, child)
			end
		end
	end
	return res
end

-- Core property extractor: translates shorthand props to engine styles
local function buildBaseNode(props, defaultType)
	if type(props) ~= "table" then
		props = {}
	end

	local node = {
		type = props.type or defaultType,
		style = elements.mergeStyles({}, props.style), -- Clone to avoid mutating
		children = props.children or {},
	}

	-- 1. Translate Shorthand Props to C++ Engine Styles
	if props.w then
		node.style.w = props.w
	end
	if props.h then
		node.style.h = props.h
	end
	if props.bg then
		node.style.BGColor = props.bg
	end
	if props.rounded then
		node.style.borderRadius = props.rounded
	end
	if props.gap then
		node.style.spacing = props.gap
	end
	if props.justify then
		node.style.justifyContent = props.justify
	end
	if props.align then
		node.style.alignItems = props.align
	end

	if props.p then
		node.style.padding = props.p
	end
	if props.px then
		node.style.paddingLeft = props.px
		node.style.paddingRight = props.px
	end
	if props.py then
		node.style.paddingTop = props.py
		node.style.paddingBottom = props.py
	end
	if props.m then
		node.style.margin = props.m
	end

	if props.center then
		node.style.alignItems = "center"
		node.style.justifyContent = "center"
	end

	if props.row then
		node.type = "hbox"
	end

	-- 2. Map over remaining props (events, ids, etc)
	local styleKeys = {
		w = true,
		h = true,
		bg = true,
		rounded = true,
		gap = true,
		justify = true,
		align = true,
		p = true,
		px = true,
		py = true,
		m = true,
		center = true,
		row = true,
	}

	for k, v in pairs(props) do
		if k ~= "type" and k ~= "style" and k ~= "children" and k ~= "text" and not styleKeys[k] then
			node[k] = v
		end
	end

	return node
end

-------------------------------------------------------------------------------
-- 3. COMPONENTS
-------------------------------------------------------------------------------

---@param props VulpisProps
---@return VulpisNode
function elements.Box(props)
	if type(props) ~= "table" then
		props = {}
	end
	return buildBaseNode(props, "vbox") -- Default to vbox, overridden if `row = true`
end

---@param props VulpisProps
---@return VulpisNode
function elements.VBox(props)
	if type(props) ~= "table" then
		props = {}
	end
	props.type = "vbox"
	return buildBaseNode(props, "vbox")
end

---@param props VulpisProps
---@return VulpisNode
function elements.HBox(props)
	if type(props) ~= "table" then
		props = {}
	end
	props.type = "hbox"
	return buildBaseNode(props, "hbox")
end

---@param props VulpisProps | { text: string, color?: VulpisColor, size?: integer, weight?: FontWeight, allowSelection?: boolean }
---@param optionalStyle? VulpisStyle
---@param optionalProps? table
---@return VulpisNode
function elements.Text(props, optionalStyle, optionalProps)
	if type(props) == "string" or type(props) == "number" then
		local textString = tostring(props)
		props = type(optionalProps) == "table" and optionalProps or {}
		props.text = textString
		props.style = type(optionalStyle) == "table" and optionalStyle or {}
	elseif type(props) ~= "table" then
		props = {}
	end

	local node = buildBaseNode(props, "text")
	node.text = props.text or ""

	-- Map shorthand text styles
	if props.color then
		node.style.color = props.color
	end
	if props.size then
		node.style.fontSize = props.size
	end
	if props.weight then
		node.style.fontWeight = props.weight
	end

	return node
end

---@param props VulpisProps | { text?: string }
---@return VulpisNode
function elements.Button(props)
	if type(props) == "string" or type(props) == "number" then
		props = { text = tostring(props) }
	elseif type(props) ~= "table" then
		props = {}
	end

	-- Default styles combined into one table
	local defaultStyle = {
		paddingTop = 10,
		paddingBottom = 10,
		paddingLeft = 20,
		paddingRight = 20,
		BGColor = "#FFAC1C",
		alignItems = "center",
		justifyContent = "center",
		borderRadius = 10,

		-- Default text styles
		color = "#000000",
	}

	-- Build the node to extract the shorthands into node.style
	local node = buildBaseNode(props, "hbox")

	-- Merge the user's unified style with the defaults
	local mergedStyle = elements.mergeStyles(defaultStyle, node.style)

	local textKeys = {
		color = true,
		fontSize = true,
		fontFamily = true,
		fontWeight = true,
		fontStyle = true,
		textDecoration = true,
	}

	local boxStyle = {}
	local textStyle = {}

	for k, v in pairs(mergedStyle) do
		if textKeys[k] then
			textStyle[k] = v
		else
			boxStyle[k] = v
		end
	end

	node.style = boxStyle

	if props.text and #node.children == 0 then
		local textNode = elements.Text({
			text = props.text,
			style = textStyle,
		})
		node.children = { textNode }
	end

	return node
end

---@param props VulpisProps | { src: string }
---@return VulpisNode
function elements.Image(props)
	if type(props) == "string" then
		props = { src = props }
	elseif type(props) ~= "table" then
		props = {}
	end

	local node = buildBaseNode(props, "image")
	node.src = props.src or ""

	-- Ensure images have a default size if not provided in style
	if not node.style.w then
		node.style.w = 100
	end
	if not node.style.h then
		node.style.h = 100
	end

	return node
end

return elements
