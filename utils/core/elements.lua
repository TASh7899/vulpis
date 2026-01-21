local elements = {}

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
	-- Support shorthand: el.Text("Hello")
	if type(props) == "string" then
		props = { text = props }
	end

	props = props or {}
	props.type = "text"
	props.style = props.style or {}

	-- [UPDATED] Directly use FontColor as requested.
	-- We no longer map 'color' to 'FontColor'.
	-- If FontColor is missing, default to White so it is visible.
	if not props.style.FontColor then
		props.style.FontColor = "#FFFFFF"
	end

	return props
end

return elements
