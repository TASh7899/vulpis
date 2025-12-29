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

function elements.Box(props)
	props = props or {}
	
	-- Default to horizontal box unless direction == "vertical"
	local boxType = "hbox"
	if props.direction == "vertical" then
		boxType = "vbox"
	end
	
	local node = {
		type = boxType,
		style = props.style or {},
		children = props.children or {},
	}
	return node
end

function elements.VBox(props)
	props = props or {}
	props.direction = "vertical"
	return elements.Box(props)
end

function elements.HBox(props)
	props = props or {}
	-- direction defaults to horizontal, so we can just call Box
	return elements.Box(props)
end

-- Example reusable component: Card
function elements.Card(props)
	props = props or {}
	
	local defaultStyle = {
		BGColor = "#2a2a2a",
		padding = 16,
		w = 200,
		h = 150
	}
	
	local mergedStyle = elements.mergeStyles(defaultStyle, props.style or {})
	
	return elements.Box({
		direction = "vertical",
		style = mergedStyle,
		children = props.children or {}
	})
end

return elements
