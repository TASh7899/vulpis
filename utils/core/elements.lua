local elements = {}

-- Helper to merge style tables
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

-- Helper to merge child lists
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

	-- The engine expects specific style keys like 'w', 'h', 'BGColor', etc.
	-- We pass the style table through as-is.
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

function elements.Text(props, optionalStyle)
	-- Support shorthand: elements.Text("Hello World") or elements.Text("Text", { color = "#FFF" })
	if type(props) == "string" then
		props = {
			text = props,
			style = optionalStyle or {},
		}
	end

	props = props or {}

	local node = {
		type = "text",
		text = props.text or "",
		style = props.style or {},
		children = props.children or {},
		onClick = props.onClick,
		key = props.key,
	}

	return node
end

return elements
