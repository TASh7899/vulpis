-- ==============================================================================
-- utils/core/flatList.lua
-- ==============================================================================

local VirtualList = require("utils.core.virualizedList")
local elements = require("utils.core.elements")

local function FlatList(props)
	-- Strict Validation
	assert(
		props.id,
		"[FlatList] CRITICAL ERROR: You MUST provide a stable string 'id' for the FlatList to prevent state loss."
	)
	assert(type(props.renderItem) == "function", "[FlatList] Error: 'renderItem' prop must be a function.")
	assert(type(props.data) == "table", "[FlatList] Error: 'data' prop must be a table (array).")

	local data = props.data or {}
	local renderItem = props.renderItem

	local numColumns = math.max(1, tonumber(props.numColumns) or 1)
	local itemHeight = tonumber(props.itemHeight) or 50
	local gap = tonumber(props.gap) or 0

	local totalItems = #data
	local totalRows = math.ceil(totalItems / numColumns)
	local rowHeight = itemHeight + gap

	return VirtualList({
		id = props.id,
		itemCount = totalRows,
		itemHeight = rowHeight,
		overscan = props.overscan or 2, -- Allows users to increase off-screen buffer
		style = props.style or { flexGrow = 1, w = "100%" },

		renderItem = function(rowIndex)
			local columns = {}
			for col = 0, numColumns - 1 do
				local index = (rowIndex * numColumns) + col + 1

				if index <= totalItems then
					local success, itemNode = pcall(renderItem, data[index], index)
					if not success or type(itemNode) ~= "table" then
						itemNode = elements.Box({
							style = {
								flexGrow = 1,
								h = itemHeight,
								margin = gap / 2,
								BGColor = "#F38BA8",
								alignItems = "center",
								justifyContent = "center",
							},
							children = { elements.Text({ text = "Error", color = "#11111B" }) },
						})
					else
						itemNode.style = itemNode.style or {}
						itemNode.style.flexGrow = 1
						itemNode.style.h = itemHeight
						itemNode.style.margin = gap / 2
					end
					table.insert(columns, itemNode)
				else
					table.insert(
						columns,
						elements.Box({
							style = { flexGrow = 1, margin = gap / 2 },
						})
					)
				end
			end

			return elements.HBox({
				style = { w = "100%", h = rowHeight, paddingLeft = gap / 2, paddingRight = gap / 2 },
				children = columns,
			})
		end,
	})
end

return FlatList
