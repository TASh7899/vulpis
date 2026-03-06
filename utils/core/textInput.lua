local elements = require("utils.core.elements")

local themes = {
	dark = {
		bg_normal = "#2a2b2e",
		bg_focused = "#3f4147",
		text = "#ffffff",
		placeholder = "#888888",
	},
	light = {
		bg_normal = "#e8eaed",
		bg_focused = "#ffffff",
		text = "#202124",
		placeholder = "#a0a0a0",
	},
	high_contrast = {
		bg_normal = "#000000",
		bg_focused = "#222222",
		text = "#00ff00",
		placeholder = "#005500",
	},
}

function TextInput(props)
	props = props or {}

	-- Automatically generate a stable ID if one isn't provided
	local id = props.id
	if not id then
		local info = debug.getinfo(2, "Sl")
		if info and info.short_src and info.currentline then
			id = "auto_" .. info.short_src .. "_" .. tostring(info.currentline)
		else
			id = "default_input"
		end
	end

	local isControlled = props.value ~= nil

	local internalText = useState(id .. "_text", props.defaultValue or "")
	local lastDefault = useState(id .. "_last_default", "\0")

	if not isControlled and (props.defaultValue or "") ~= lastDefault then
		internalText = props.defaultValue or ""
		setState(id .. "_text", internalText)
		setState(id .. "_last_default", internalText)
		setState(id .. "_cursor", #internalText)
	end

	local currentText = isControlled and props.value or internalText

	local isFocused = useState(id .. "_focus", false)

	local savedCursor = useState(id .. "_cursor", #currentText)
	local curPos = math.min(savedCursor, #currentText)

	local currentTheme = themes[props.theme] or themes.dark

	local isEmpty = currentText == ""
	local displayText = currentText
	if isEmpty then
		displayText = props.placeholder or " "
	end

	return {
		type = "text",
		text = displayText,

		focusable = true,
		isFocused = isFocused,
		cursorPosition = isFocused and (isEmpty and 0 or curPos) or -1,

		style = elements.mergeStyles({
			w = 200,
			h = 40,
			padding = 10,
			overflow = "hidden",
			wordWrap = false,
			BGColor = isFocused and currentTheme.bg_focused or currentTheme.bg_normal,
			color = isEmpty and currentTheme.placeholder or currentTheme.text,
		}, props.style),

		onFocus = function()
			setState(id .. "_focus", true)
		end,

		onBlur = function()
			setState(id .. "_focus", false)
		end,

		onTextInput = function(char)
			local newText = string.sub(currentText, 1, curPos) .. char .. string.sub(currentText, curPos + 1)
			local newPos = curPos + #char

			-- Always update cursor locally so typing feels instantly responsive
			setState(id .. "_cursor", newPos)

			-- ONLY save text internally if the component is Uncontrolled
			if not isControlled then
				setState(id .. "_text", newText)
			end

			if props.onChange then
				props.onChange(newText)
			end
		end,

		onKeyDown = function(keyName, mods)
			local newText = currentText
			local newPos = curPos

			if keyName == "Backspace" then
				if curPos > 0 then
					newText = string.sub(currentText, 1, curPos - 1) .. string.sub(currentText, curPos + 1)
					newPos = curPos - 1
				end
			elseif keyName == "Delete" then
				if curPos < #currentText then
					newText = string.sub(currentText, 1, curPos) .. string.sub(currentText, curPos + 2)
				end
			elseif keyName == "Left" then
				newPos = math.max(0, curPos - 1)
			elseif keyName == "Right" then
				newPos = math.min(#currentText, curPos + 1)
			elseif keyName == "Home" then
				newPos = 0
			elseif keyName == "End" then
				newPos = #currentText
			else
				if keyName == "Return" or keyName == "KP_Enter" then
					if props.onSubmit then
						props.onSubmit(currentText)

						-- Only automatically clear text if Uncontrolled
						if not isControlled then
							setState(id .. "_cursor", 0)
							setState(id .. "_text", "")
						end

						newText = ""
						newPos = 0
					end
				end
			end

			if newText ~= currentText or newPos ~= curPos then
				setState(id .. "_cursor", newPos)

				if not isControlled and newText ~= currentText then
					setState(id .. "_text", newText)
				end

				if props.onChange and newText ~= currentText then
					props.onChange(newText)
				end
			end
		end,

		key = props.key or id,
	}
end

return TextInput
