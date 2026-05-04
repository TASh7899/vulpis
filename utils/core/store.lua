local store = {}

function store.create(setup_function)
	local state = {}

	local function set(updates)
		local changed = false
		for k, v in pairs(updates) do
			if state[k] ~= v then
				state[k] = v
				changed = true
			end
		end

		if changed and vulpis and vulpis.markDirty then
			vulpis.markDirty()
		end
	end

	local function get()
		return state
	end

	state = setup_function(get, set)

	local function useStore()
		return state
	end

	return useStore
end

return store
