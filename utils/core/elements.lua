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

function elements.Text(props, optionalStyle, optionalProps)
	if type(props) == "string" then
		local textString = props
		props = optionalProps or {}
		props.text = textString
		props.style = optionalStyle or {}
	end

	local node = buildBaseNode(props, "text")
	node.text = props.text or ""

	if props.allowSelection then
		local id = props.id
		if not id then
			local info = debug.getinfo(2, "Sl")
			if info and info.short_src and info.currentline then
				id = "auto_txt_" .. info.short_src .. tostring(info.currentline)
			else
				id = "default_selectable_text"
			end
		end

		node.id = id
		node.focusable = true

		local isFocused = useState(id .. "_focus", false)
		local cursor = useState(id .. "_cursor", -1)
		local anchor = useState(id .. "_anchor", -1)

		node.isFocused = isFocused
		node.selectionStart = isFocused and anchor or -1
		node.selectionEnd = isFocused and cursor or -1

		node.cursorPosition = isFocused and cursor or -1

		node.onDragStart = function(mx, my, textIdx, clicks)
			if textIdx >= 0 then
				if clicks and clicks >= 3 then
					setState(id .. "_cursor", #node.text)
					setState(id .. "_anchor", 0)
				else
					local safeIdx = math.min(textIdx, #node.text)
					setState(id .. "_cursor", safeIdx)
					setState(id .. "_anchor", safeIdx)
				end

				setState(id .. "_focus", true)
			end
			if props.onDragStart then
				props.onDragStart(mx, my, textIdx, clicks)
			end
		end

		node.onDrag = function(dx, dy, mx, my, textIdx)
			if textIdx >= 0 then
				setState(id .. "_cursor", math.min(textIdx, #node.text))
			end

			if props.onDrag then
				props.onDrag(dx, dy, mx, my, textIdx)
			end
		end

		node.onBlur = function()
			setState(id .. "_focus", false)
			if props.onBlur then
				props.onBlur()
			end
		end

		node.onKeyDown = function(keyName, mods)
			if (keyName == "c" or keyName == "C") and (mods.ctrl or mods.gui) then
				local selMin = math.min(cursor, anchor)
				local selMax = math.max(cursor, anchor)
				if selMin ~= selMax then
					local selectedText = string.sub(node.text, selMin + 1, selMax)
					vulpis.setClipboardText(selectedText)
				end
			end

			if props.onKeyDown then
				props.onKeyDown(keyName, mods)
			end
		end
	end

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
		borderRadius = 10,

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

function elements.Image(props)
	if type(props) == "string" then
		props = { src = props }
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
