local elements = require("utils.core.elements")

local router = {}
local routes = {}

function router.define(routeTable)
	routes = routeTable
end

function router.push(path)
	setState("__router_current_path", path)
end

function router.render()
	local currentPath = useState("__router_current_path", "/")

	local screenComponent = routes[currentPath]

	if screenComponent then
		return screenComponent()
	else
		return elements.VBox({
			style = { w = "100%", h = "100%", BGColor = "#000000", alignItems = "center", justifyContent = "center" },
			children = {
				elements.Text({
					text = "404 - ROUTE NOT FOUND",
					style = { color = "#EF4444", fontSize = 24, marginBottom = 10 },
				}),
				elements.Text({
					text = "Path: " .. tostring(currentPath),
					style = { color = "#A1A1AA", fontSize = 16 },
				}),
			},
		})
	end
end

return router
