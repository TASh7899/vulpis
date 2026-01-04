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
	
	-- default to horizontal box unless direction == "vertical"
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

return elements
