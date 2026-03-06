local elements = {}

-- Helper to merge style tables (supports passing nil safely)
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

-- Core property extractor to prevent duplicating reserved keys
local function buildBaseNode(props, defaultType)
	props = props or {}

	local node = {
		type = props.type or defaultType,
		style = props.style or {},
		children = props.children or {},
	}

	for k, v in pairs(props) do
		if k ~= "type" and k ~= "style" and k ~= "children" and k ~= "text" then
			node[k] = v
		end
	end

	return node
end

function elements.Box(props)
	return buildBaseNode(props, "hbox")
end

function elements.VBox(props)
	props = props or {}
	props.type = "vbox"
	return buildBaseNode(props, "vbox")
end

function elements.HBox(props)
	props = props or {}
	props.type = "hbox"
	return buildBaseNode(props, "hbox")
end

function elements.Text(props, optionalStyle)
	if type(props) == "string" then
		props = {
			text = props,
			style = optionalStyle or {},
		}
	end

	local node = buildBaseNode(props, "text")
	node.text = props.text or ""

	return node
end

function elements.Button(props)
	if type(props) == "string" then
		props = { text = props }
	end
	props = props or {}

	-- Default styles combined into one table
	local defaultStyle = {
		paddingTop = 10,
		paddingBottom = 10,
		paddingLeft = 20,
		paddingRight = 20,
		BGColor = "#FFAC1C",
		alignItems = "center",
		justifyContent = "center",

		-- Default text styles
		color = "#000000",
	}

	-- Merge the user's unified style with the defaults
	local mergedStyle = elements.mergeStyles(defaultStyle, props.style)

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

	local node = buildBaseNode(props, "hbox")
	node.style = boxStyle

	if props.text and not props.children then
		local textNode = elements.Text({
			text = props.text,
			style = textStyle,
		})
		node.children = { textNode }
	end

	return node
end

return elements
