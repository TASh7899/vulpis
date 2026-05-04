local elements = {}

---@class VulpisProps
---@field id? string
---@field key? string
---@field onClick? fun(mx: number, my: number)
---@field onRightClick? fun(mx: number, my: number)
---@field onMouseEnter? fun()
---@field onMouseLeave? fun()
---@field children? VulpisNode[]
---@field style? VulpisStyle
-- (You can add your native shorthands back to these annotations later!)

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

-- Core property extractor
local function buildBaseNode(props, defaultType)
	if type(props) ~= "table" then
		props = {}
	end

	local node = {
		type = props.type or defaultType,
		style = elements.mergeStyles({}, props.style), -- Clone to avoid mutating
		children = props.children or {},
	}

	-- Map over all remaining props and pass them directly to C++!
	local reservedKeys = {
		type = true,
		style = true,
		children = true,
		text = true,
	}

	for k, v in pairs(props) do
		if not reservedKeys[k] then
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
	return buildBaseNode(props, "vbox")
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

	-- I left these Text shorthands here, but you can move them to C++ too if you want!
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
		color = "#000000",
	}

	local node = buildBaseNode(props, "hbox")
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
