---@meta
-- This file provides IntelliSense for the Vulpis UI Framework.
-- Do not require() this file in your actual code.

-------------------------------------------------------------------------------
-- TYPE ALIASES (Enables rich string autocomplete dropdowns)
-------------------------------------------------------------------------------

---@alias VulpisColor string | integer[] # Hex string (e.g., "#FF0000") or {r, g, b, a} table.
---@alias FlexAlign "start" | "center" | "end" | "stretch"
---@alias FlexJustify "start" | "end" | "center" | "space-between" | "space-around" | "space-evenly"
---@alias FlexWrap "nowrap" | "wrap" | "wrap-reverse"
---@alias PositionMode "relative" | "absolute"
---@alias OverflowMode "visible" | "hidden" | "scroll" | "auto"
---@alias AutoScrollMode "none" | "top" | "bottom"
---@alias ObjectFitMode "fill" | "cover" | "contain"
---@alias FontWeight "thin" | "normal" | "semi-bold" | "bold" | "very-bold"
---@alias FontStyle "normal" | "italics"
---@alias TextDecoration "none" | "underline" | "strike-through"
---@alias TextAlign "left" | "center" | "right"
---@alias NodeType "vbox" | "hbox" | "text" | "image"

-------------------------------------------------------------------------------
-- ENGINE GLOBALS (Lifecycle Hooks)
-------------------------------------------------------------------------------

---@class WindowConfig
---@field title? string
---@field mode? "full"|"whole screen"|string
---@field w? integer
---@field h? integer
---@field resizable? boolean

--- [VULPIS HOOK] Define window configuration.
---@return WindowConfig
function Window() end

--- [VULPIS HOOK] The main entry point that returns your UI tree.
---@return VulpisNode
function App() end

--- [VULPIS HOOK] Called every frame before rendering.
---@param dt number Delta time in seconds
function on_tick(dt) end

--- [VULPIS HOOK] Called during the render phase (for immediate mode drawing).
function on_render() end

-------------------------------------------------------------------------------
-- STATE MANAGEMENT
-------------------------------------------------------------------------------

--- Retrieves a value from the global state store.
---@param key string
---@param defaultVal? number|string|boolean
---@return number|string|boolean
function useState(key, defaultVal) end

--- Sets a value in the global state store and triggers a tree reconciliation.
---@param key string
---@param val number|string|boolean
function setState(key, val) end

-------------------------------------------------------------------------------
-- VDOM NODE & STYLES
-------------------------------------------------------------------------------

---@class VulpisStyle
---@field w? number|string Width (pixels or percentage like "100%")
---@field h? number|string Height (pixels or percentage like "100%")
---@field minWidth? number Minimum width in pixels
---@field maxWidth? number Maximum width in pixels
---@field minHeight? number Minimum height in pixels
---@field maxHeight? number Maximum height in pixels
---@field left? number Absolute or relative left offset
---@field top? number Absolute or relative top offset
---@field right? number Absolute or relative right offset
---@field bottom? number Absolute or relative bottom offset
---@field position? PositionMode Layout positioning behavior
---@field opacity? number Alpha transparency (0.0 to 1.0)
---@field zIndex? integer Depth sorting order
---@field gap? integer Space between child elements (alias for spacing)
---@field spacing? integer Space between child elements
---@field padding? integer Inner spacing applied to all sides
---@field paddingTop? integer Inner top spacing
---@field paddingBottom? integer Inner bottom spacing
---@field paddingLeft? integer Inner left spacing
---@field paddingRight? integer Inner right spacing
---@field margin? integer Outer spacing applied to all sides
---@field marginTop? integer Outer top spacing
---@field marginBottom? integer Outer bottom spacing
---@field marginLeft? integer Outer left spacing
---@field marginRight? integer Outer right spacing
---@field flexGrow? number Flexbox grow factor (e.g., 1.0 to fill space)
---@field flexShrink? number Flexbox shrink factor
---@field flexWrap? FlexWrap Multi-line wrapping behavior
---@field alignItems? FlexAlign Cross-axis alignment of children
---@field justifyContent? FlexJustify Main-axis alignment of children
---@field BGColor? VulpisColor Background color
---@field BGImage? string Path or URL to background image
---@field BGFit? ObjectFitMode How the background image scales
---@field borderRadius? number Corner rounding radius in pixels
---@field borderWidth? number Thickness of the border outline
---@field borderColor? VulpisColor Color of the border outline
---@field fontFamily? string Alias of the registered font to use
---@field fontSize? integer Font size in logical pixels
---@field fontWeight? FontWeight Font weight variant
---@field fontStyle? FontStyle Font style variant (e.g., italics)
---@field textDecoration? TextDecoration Underlines or strikethroughs
---@field textAlign? TextAlign Horizontal text alignment
---@field color? VulpisColor Text color
---@field fit? ObjectFitMode How an 'image' node scales its source
---@field overflow? OverflowMode How to handle content extending beyond bounds
---@field autoScroll? AutoScrollMode Automatic scroll behavior on content change
---@field wordWrap? boolean Whether text should wrap to the next line

---@class FontHandle
local FontHandle = {}

---@class VulpisNode
---@field type NodeType The core component type.
---@field key? string Unique identifier for VDOM diffing.
---@field id? string HTML-like ID for querying or drag-and-drop targets.
---@field text? string Text content (only valid if type is "text").
---@field src? string Image source path/URL (only valid if type is "image").
---@field style? VulpisStyle Visual properties and flexbox layout.
---@field children? VulpisNode[] Array of nested child nodes.
---@field focusable? boolean Can this node receive keyboard focus?
---@field isFocused? boolean Is this node currently focused?
---@field draggable? boolean Can this node be dragged by the mouse?
---@field cursorPosition? integer Index of the text cursor.
---@field selectionStart? integer Text selection start index.
---@field selectionEnd? integer Text selection end index.
---@field onClick? fun(mx: number, my: number) Fired on left-click release.
---@field onRightClick? fun(mx: number, my: number) Fired on right-click release.
---@field onMouseEnter? fun() Fired when the cursor enters the bounds.
---@field onMouseLeave? fun() Fired when the cursor exits the bounds.
---@field onDragStart? fun(mx: number, my: number, textIndex: integer, clicks: integer)
---@field onDrag? fun(dx: integer, dy: integer, mx: integer, my: integer, textIndex: integer)
---@field onDragEnd? fun(dropId: string|nil, dx: integer, dy: integer)
---@field onTextInput? fun(text: string) Fired when characters are typed.
---@field onKeyDown? fun(keyName: string, mods: {ctrl: boolean, shift: boolean, alt: boolean, gui: boolean}) Fired on raw key press.
---@field onFocus? fun() Fired when node gains focus.
---@field onBlur? fun() Fired when node loses focus.

-------------------------------------------------------------------------------
-- VULPIS NAMESPACE (C++ BINDINGS)
-------------------------------------------------------------------------------

---@class VulpisAPI
vulpis = {}

--- Clears the hard drive and memory texture cache.
---@return boolean success
function vulpis.clearCache() end

--- Forces the layout and paint to redraw instantly.
function vulpis.markDirty() end

--- Updates an existing font configuration or registers a new alias.
---@param alias string
---@param config {path: string, size?: integer, fallback?: boolean}
function vulpis.update_font_config(alias, config) end

--- Loads a font dynamically at runtime.
---@param path string
---@param size? integer
---@param flags? {bold?: boolean, italic?: boolean, thin?: boolean}
---@return FontHandle
function vulpis.load_font(path, size, flags) end

--- Immediate mode text drawing (Use ONLY inside `on_render`).
---@param text string
---@param font FontHandle
---@param x number
---@param y number
---@param color VulpisColor
function vulpis.draw_text(text, font, x, y, color) end

--- Checks if a key is currently held down.
---@param keyName string
---@return boolean
function vulpis.isKeyHeld(keyName) end

--- Checks if a key was pressed exactly on this frame.
---@param keyName string
---@return boolean
function vulpis.isKeyJustPressed(keyName) end

--- Sets the OS clipboard text.
---@param text string
function vulpis.setClipboardText(text) end

--- Gets the current OS clipboard text.
---@return string
function vulpis.getClipboardText() end

---@class FetchOptions
---@field method? "GET"|"POST"|"PUT"|"DELETE"|"PATCH"
---@field timeout? integer
---@field body? string
---@field headers? table<string, string>

---@class FetchResponse
---@field status integer
---@field body string
---@field error string

--- Asynchronous HTTP request.
---@param url string
---@param options_or_callback FetchOptions|fun(res: FetchResponse)
---@param callback? fun(res: FetchResponse)
function vulpis.fetch(url, options_or_callback, callback) end

---@class WsEventData
---@field type "open"|"message"|"error"|"close"
---@field data string

--- Connects to a websocket and returns a connection ID.
---@param url string
---@param callback fun(ev: WsEventData)
---@return integer connectionId
function vulpis.wsConnect(url, callback) end

--- Sends a message over an open WebSocket connection.
---@param id integer connectionId
---@param msg string
---@return boolean success
function vulpis.wsSend(id, msg) end

--- Closes a WebSocket connection.
---@param id integer connectionId
function vulpis.wsClose(id) end

---@class VulpisElementsModule
---@field Box fun(props: VulpisProps): VulpisNode
---@field VBox fun(props: VulpisProps): VulpisNode
---@field HBox fun(props: VulpisProps): VulpisNode
---@field Text fun(props: VulpisProps): VulpisNode
---@field Button fun(props: VulpisProps): VulpisNode
---@field Image fun(props: VulpisProps): VulpisNode

---@type VulpisElementsModule
package.loaded["utils.core.elements"] = nil
