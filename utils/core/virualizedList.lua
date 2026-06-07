local function VirtualList(props)
	local itemHeight = tonumber(props.itemHeight) or 50
	local itemCount = tonumber(props.itemCount) or 0
	local renderItem = props.renderItem
	local overscan = props.overscan or 2

	local startIndex = useState(props.id .. "_startIndex", 0)
	local containerHeight = useState(props.id .. "_actualHeight", 500)

	local visibleCount = math.ceil(containerHeight / itemHeight) + (overscan * 2)
	local endIndex = math.min(itemCount - 1, startIndex + visibleCount - 1)

	local visibleNodes = {}

	for i = 1, visibleCount do
		visibleNodes[i] = {
			type = "box",
			id = props.id .. "_pool_slot_" .. i,
			style = { display = "none" },
		}
	end

	for dataIndex = startIndex, endIndex do
		local poolSlotIndex = (dataIndex % visibleCount) + 1
		local success, node = pcall(renderItem, dataIndex)

		if success and type(node) == "table" then
			node.id = props.id .. "_pool_slot_" .. poolSlotIndex

			node.style = node.style or {}
			node.style.position = "absolute"

			node.style.top = 0
			node.style.left = 0

			node.style.translateY = dataIndex * itemHeight

			node.style.w = "100%"
			node.style.h = itemHeight

			visibleNodes[poolSlotIndex] = node
		end
	end

	local totalScrollableHeight = itemCount * itemHeight

	local mergedStyle = { w = "100%", flexGrow = 1 }
	if props.style then
		for k, v in pairs(props.style) do
			mergedStyle[k] = v
		end
	end
	mergedStyle.overflow = "scroll"
	mergedStyle.position = "relative"

	mergedStyle.flexGrow = 1
	mergedStyle.flexShrink = 1

	return {
		type = "vbox",
		id = props.id,
		style = mergedStyle,

		onScroll = function(sx, sy, computedH)
			local newStartIndex = math.max(0, math.floor(sy / itemHeight) - overscan)

			if newStartIndex ~= startIndex then
				setState(props.id .. "_startIndex", newStartIndex)
			end

			if computedH ~= containerHeight and computedH > 0 then
				setState(props.id .. "_actualHeight", computedH)
			end
		end,
		children = {
			{
				type = "vbox",
				style = { w = "100%", h = totalScrollableHeight, position = "relative" },
				children = visibleNodes,
			},
		},
	}
end

return VirtualList
