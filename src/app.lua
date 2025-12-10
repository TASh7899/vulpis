local elements = require("utils.core.elements")

local VStack = elements.VStack
local HStack = elements.HStack
local Rect = elements.Rect

UI = VStack({
	style = {
		paddingTop = 40,
		paddingLeft = 80,
		paddingRight = 80,
		paddingBottom = 40,
		color = { 255, 0, 255 },
		spacing = 10,
	},

	children = {
		Rect({ style = { w = 500, h = 100, color = { 0, 0, 255 } } }),
		Rect({ style = { w = 500, h = 100, color = { 255, 0, 0, 100 } } }),
		Rect({ style = { w = 500, h = 100, color = { 255, 0, 0, 100 } } }),
		Rect({ style = { w = 100, h = 50, color = { 255, 200, 0 } } }),

		HStack({
			style = { spacing = 10 },

			children = {
				Rect({ style = { w = 100, h = 50, color = { 0, 255, 0 } } }),
				Rect({ style = { w = 100, h = 50, color = { 0, 255, 0 } } }),
				Rect({ style = { w = 100, h = 50, color = { 0, 200, 0 } } }),
				Rect({ style = { w = 100, h = 50, color = { 0, 200, 0 } } }),
			},
		}),
	},
})
