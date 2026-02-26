-- utils/core/store.lua
local store = {}

function store.create(setup_function)
	local state = {}

	-- The internal set function
	local function set(updates)
		local changed = false
		for k, v in pairs(updates) do
			if state[k] ~= v then
				state[k] = v
				changed = true
			end
		end

		-- Safely tell the C++ engine to rebuild the VDOM on the next tick
		if changed and vulpis and vulpis.markDirty then
			vulpis.markDirty()
		end
	end

	-- The internal get function
	local function get()
		return state
	end

	-- Initialize the store using the user's setup function
	-- This allows the user to define state AND actions!
	state = setup_function(get, set)

	-- The Hook: Used by App() to read the data
	local function useStore()
		return state
	end

	return useStore
end

return store
