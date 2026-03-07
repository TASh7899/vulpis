local elements = require("utils.core.elements")

local themes = {
	dark = { bg_normal = "#2a2b2e", bg_focused = "#3f4147", text = "#ffffff", placeholder = "#888888" },
	light = { bg_normal = "#e8eaed", bg_focused = "#ffffff", text = "#202124", placeholder = "#a0a0a0" },
	high_contrast = { bg_normal = "#000000", bg_focused = "#222222", text = "#00ff00", placeholder = "#005500" },
}

function TextInput(props)
	props = props or {}

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
		setState(id .. "_anchor", #internalText)
	end

	local currentText = isControlled and props.value or internalText
	local isFocused = useState(id .. "_focus", false)

	local savedCursor = useState(id .. "_cursor", #currentText)
	local curPos = math.min(savedCursor, #currentText)

	-- Track the "Anchor" (where the user first held Shift)
	local savedAnchor = useState(id .. "_anchor", curPos)
	local selAnchor = math.min(savedAnchor, #currentText)

	local currentTheme = themes[props.theme] or themes.dark
	local isEmpty = currentText == ""
	local displayText = isEmpty and (props.placeholder or " ") or currentText

	-- Helper to get sorted selection range
	local function getSelection()
		if curPos ~= selAnchor then
			return math.min(curPos, selAnchor), math.max(curPos, selAnchor)
		end
		return nil, nil
	end

	return {
		type = "text",
		text = displayText,
		focusable = true,
		isFocused = isFocused,

		-- Send exact cursor and selection indices to C++
		cursorPosition = isFocused and (isEmpty and 0 or curPos) or -1,
		selectionStart = isFocused and (isEmpty and 0 or selAnchor) or -1,
		selectionEnd = isFocused and (isEmpty and 0 or curPos) or -1,

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

		onDragStart = function(mx, my, textIdx, clicks)
			if textIdx >= 0 then
				if clicks and clicks >= 2 then
					setState(id .. "_anchor", 0)
					setState(id .. "_cursor", #currentText)
					setState(id .. "_focus", true)
				else
					local targetIdx = isEmpty and 0 or math.min(textIdx, #currentText)
					setState(id .. "_cursor", targetIdx)
					setState(id .. "_anchor", targetIdx)
					setState(id .. "_focus", true)
				end
			end
		end,

		onDrag = function(dx, dy, mx, my, textIdx)
			if textIdx >= 0 then
				local targetIdx = isEmpty and 0 or math.min(textIdx, #currentText)
				if curPos ~= targetIdx then
					setState(id .. "_cursor", targetIdx)
				end
			end
		end,

		onTextInput = function(char)
			local textToMod = currentText
			local insertPos = curPos
			local selMin, selMax = getSelection()

			-- If text is selected, delete it first before inserting the new character
			if selMin then
				textToMod = string.sub(currentText, 1, selMin) .. string.sub(currentText, selMax + 1)
				insertPos = selMin
			end

			local newText = string.sub(textToMod, 1, insertPos) .. char .. string.sub(textToMod, insertPos + 1)
			local newPos = insertPos + #char

			setState(id .. "_cursor", newPos)
			setState(id .. "_anchor", newPos) -- Clear selection

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
			local newAnchor = selAnchor
			local selMin, selMax = getSelection()

			-- Check if Shift is held down
			local isShift = (mods and mods.shift) or keyName == "LShift" or keyName == "RShift"

			if keyName == "Backspace" then
				if selMin then
					newText = string.sub(currentText, 1, selMin) .. string.sub(currentText, selMax + 1)
					newPos = selMin
				elseif curPos > 0 then
					newText = string.sub(currentText, 1, curPos - 1) .. string.sub(currentText, curPos + 1)
					newPos = curPos - 1
				end
				newAnchor = newPos -- Clear selection
			elseif keyName == "Delete" then
				if selMin then
					newText = string.sub(currentText, 1, selMin) .. string.sub(currentText, selMax + 1)
					newPos = selMin
				elseif curPos < #currentText then
					newText = string.sub(currentText, 1, curPos) .. string.sub(currentText, curPos + 2)
				end
				newAnchor = newPos -- Clear selection
			elseif keyName == "Left" then
				if not isShift and selMin then
					newPos = selMin -- Browser behavior: snap to left edge of selection
				else
					newPos = math.max(0, curPos - 1)
				end
				if not isShift then
					newAnchor = newPos
				end
			elseif keyName == "Right" then
				if not isShift and selMax then
					newPos = selMax -- Browser behavior: snap to right edge of selection
				else
					newPos = math.min(#currentText, curPos + 1)
				end
				if not isShift then
					newAnchor = newPos
				end
			elseif keyName == "Home" then
				newPos = 0
				if not isShift then
					newAnchor = newPos
				end
			elseif keyName == "End" then
				newPos = #currentText
				if not isShift then
					newAnchor = newPos
				end
			elseif keyName == "a" or keyName == "A" then
				-- Support Ctrl+A / Cmd+A to Select All
				if mods and (mods.ctrl or mods.gui) then
					newAnchor = 0
					newPos = #currentText
				end
			elseif keyName == "Return" or keyName == "KP_Enter" then
				if props.onSubmit then
					props.onSubmit(currentText)
					if not isControlled then
						setState(id .. "_cursor", 0)
						setState(id .. "_anchor", 0)
						setState(id .. "_text", "")
					end
					newText = ""
					newPos = 0
					newAnchor = 0
				end
			elseif (keyName == "c" or keyName == "C") and (mods.ctrl or mods.gui) then
				if selMin then
					local selectedText = string.sub(currentText, selMin + 1, selMax)
					vulpis.setClipboardText(selectedText)
				end
			elseif (keyName == "x" or keyName == "X") and (mods.ctrl or mods.gui) then
				if selMin then
					local selectedText = string.sub(currentText, selMin + 1, selMax)
					vulpis.setClipboardText(selectedText)
					newText = string.sub(currentText, 1, selMin) .. string.sub(currentText, selMax + 1)
					newPos = selMin
					newAnchor = newPos
				end
			elseif (keyName == "v" or keyName == "V") and (mods.ctrl or mods.gui) then
				local pastedText = vulpis.getClipboardText()
				if pastedText and pastedText ~= "" then
					local textToMod = currentText
					local insertPos = curPos

					if selMin then
						textToMod = string.sub(currentText, 1, selMin) .. string.sub(currentText, selMax + 1)
						insertPos = selMin
					end

					newText = string.sub(textToMod, 1, insertPos) .. pastedText .. string.sub(textToMod, insertPos + 1)
					newPos = curPos + #pastedText
					newAnchor = newPos
				end
			end

			-- Only dispatch state updates if something actually changed
			if newText ~= currentText or newPos ~= curPos or newAnchor ~= selAnchor then
				setState(id .. "_cursor", newPos)
				setState(id .. "_anchor", newAnchor)

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
