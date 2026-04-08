local ws = {}

function ws.connect(url, onEvent)
	local id = vulpis.wsConnect(url, function(event)
		if onEvent then
			onEvent(event)
		end
	end)

	local connection = {
		id = id,
		send = function(self, message)
			if type(message) == "table" then
				local json = require("utils.core.json")
				message = json.encode(message)
			end
			return vulpis.wsSend(self.id, message)
		end,
		close = function(self)
			vulpis.wsClose(self.id)
		end,
	}

	return connection
end

return ws
