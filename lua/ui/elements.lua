local elements = {}

local function createStack(typeName, props)
	props = props or {}

	local node = {
		type = typeName,
		style = props.style or {},
		children = props.children or {},
	}
	return node
end

function elements.VStack(props)
	return createStack("vstack", props)
end

function elements.HStack(props)
	return createStack("hstack", props)
end

function elements.Rect(props)
	props = props or {}

	local node = {
		type = "rect",
		style = props.style or {},
		children = nil,
	}

	return node
end

return elements
