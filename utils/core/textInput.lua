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

	if isControlled and internalText ~= props.value then
		internalText = props.value
		setState(id .. "_text", internalText)
	end

	local currentText = internalText
	local isFocused = useState(id .. "_focus", false)

	local savedCursor = useState(id .. "_cursor", #currentText)
	local curPos = math.min(savedCursor, #currentText)

	local savedAnchor = useState(id .. "_anchor", curPos)
	local selAnchor = math.min(savedAnchor, #currentText)

	local currentTheme = themes[props.theme] or themes.dark
	local isEmpty = currentText == ""
	local displayText = isEmpty and (props.placeholder or " ") or currentText

	-- [!] FIX: A mutable reference that persists across multiple rapid-fire events in a single frame.
	local live = {
		text = currentText,
		cursor = curPos,
		anchor = selAnchor,
	}

	return {
		type = "text",
		text = displayText,
		focusable = true,
		isFocused = isFocused,

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
				local isE = live.text == ""
				if clicks and clicks >= 2 then
					live.anchor = 0
					live.cursor = #live.text
				else
					local targetIdx = isE and 0 or math.min(textIdx, #live.text)
					live.cursor = targetIdx
					live.anchor = targetIdx
				end
				setState(id .. "_anchor", live.anchor)
				setState(id .. "_cursor", live.cursor)
				setState(id .. "_focus", true)
			end
		end,

		onDrag = function(dx, dy, mx, my, textIdx)
			if textIdx >= 0 then
				local isE = live.text == ""
				local targetIdx = isE and 0 or math.min(textIdx, #live.text)
				if live.cursor ~= targetIdx then
					live.cursor = targetIdx
					setState(id .. "_cursor", targetIdx)
				end
			end
		end,

		onTextInput = function(char)
			local textToMod = live.text
			local insertPos = live.cursor

			local selMin, selMax = nil, nil
			if live.cursor ~= live.anchor then
				selMin = math.min(live.cursor, live.anchor)
				selMax = math.max(live.cursor, live.anchor)
			end

			if selMin then
				textToMod = string.sub(live.text, 1, selMin) .. string.sub(live.text, selMax + 1)
				insertPos = selMin
			end

			local newText = string.sub(textToMod, 1, insertPos) .. char .. string.sub(textToMod, insertPos + 1)
			local newPos = insertPos + #char

			-- [!] Update live ref synchronously
			live.text = newText
			live.cursor = newPos
			live.anchor = newPos

			setState(id .. "_cursor", newPos)
			setState(id .. "_anchor", newPos)
			if not isControlled then
				setState(id .. "_text", newText)
			end

			if props.onChange then
				props.onChange(newText)
			end
		end,

		onKeyDown = function(keyName, mods)
			local newText = live.text
			local newPos = live.cursor
			local newAnchor = live.anchor

			local selMin, selMax = nil, nil
			if live.cursor ~= live.anchor then
				selMin = math.min(live.cursor, live.anchor)
				selMax = math.max(live.cursor, live.anchor)
			end

			local isShift = (mods and mods.shift) or keyName == "LShift" or keyName == "RShift"

			if keyName == "Backspace" then
				if selMin then
					newText = string.sub(live.text, 1, selMin) .. string.sub(live.text, selMax + 1)
					newPos = selMin
				elseif live.cursor > 0 then
					newText = string.sub(live.text, 1, live.cursor - 1) .. string.sub(live.text, live.cursor + 1)
					newPos = live.cursor - 1
				end
				newAnchor = newPos
			elseif keyName == "Delete" then
				if selMin then
					newText = string.sub(live.text, 1, selMin) .. string.sub(live.text, selMax + 1)
					newPos = selMin
				elseif live.cursor < #live.text then
					newText = string.sub(live.text, 1, live.cursor) .. string.sub(live.text, live.cursor + 2)
				end
				newAnchor = newPos
			elseif keyName == "Left" then
				if not isShift and selMin then
					newPos = selMin
				else
					newPos = math.max(0, live.cursor - 1)
				end
				if not isShift then
					newAnchor = newPos
				end
			elseif keyName == "Right" then
				if not isShift and selMax then
					newPos = selMax
				else
					newPos = math.min(#live.text, live.cursor + 1)
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
				newPos = #live.text
				if not isShift then
					newAnchor = newPos
				end
			elseif keyName == "a" or keyName == "A" then
				if mods and (mods.ctrl or mods.gui) then
					newAnchor = 0
					newPos = #live.text
				end
			elseif keyName == "Return" or keyName == "KP_Enter" then
				if props.onSubmit then
					props.onSubmit(live.text)
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
					vulpis.setClipboardText(string.sub(live.text, selMin + 1, selMax))
				end
			elseif (keyName == "x" or keyName == "X") and (mods.ctrl or mods.gui) then
				if selMin then
					vulpis.setClipboardText(string.sub(live.text, selMin + 1, selMax))
					newText = string.sub(live.text, 1, selMin) .. string.sub(live.text, selMax + 1)
					newPos = selMin
					newAnchor = newPos
				end
			elseif (keyName == "v" or keyName == "V") and (mods.ctrl or mods.gui) then
				local pastedText = vulpis.getClipboardText()
				if pastedText and pastedText ~= "" then
					local textToMod = live.text
					local insertPos = live.cursor
					if selMin then
						textToMod = string.sub(live.text, 1, selMin) .. string.sub(live.text, selMax + 1)
						insertPos = selMin
					end
					newText = string.sub(textToMod, 1, insertPos) .. pastedText .. string.sub(textToMod, insertPos + 1)
					newPos = insertPos + #pastedText
					newAnchor = newPos
				end
			end

			-- Only dispatch state updates if something actually changed
			if newText ~= live.text or newPos ~= live.cursor or newAnchor ~= live.anchor then
				-- [!] Update live ref synchronously so the next event in this frame uses these values
				local oldText = live.text
				live.text = newText
				live.cursor = newPos
				live.anchor = newAnchor

				setState(id .. "_cursor", newPos)
				setState(id .. "_anchor", newAnchor)

				if not isControlled and newText ~= oldText then
					setState(id .. "_text", newText)
				end
				if props.onChange and newText ~= oldText then
					props.onChange(newText)
				end
			end
		end,
		key = props.key or id,
	}
end

return TextInput
