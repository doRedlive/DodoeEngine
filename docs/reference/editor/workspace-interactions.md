# Editor workspace interactions

The workspace keeps the arrangement from `docs/main-window-preview.html`, with
refined island panels and Dracula colors sampled from the VS Code reference: editor content
`#282a36`, navigation and chrome `#21222c`, and inactive tabs `#191a21`.
Green, cyan, pink, yellow, orange, and purple distinguish actions, resource types,
log levels, and script tokens. Dock islands have consistent 8-pixel outer spacing
and gutters, soft 8-pixel corners, and subtle neutral outlines. Each title and
body form one island; inner splitters remain compact.
There is a tabbed Hierarchy / Project area, a central Scene / Script
area, an Inspector on the right, and a single expanded tool below the scene.
Console, History, Terminal, Workflow, and Tile Palette share that slot: opening
one closes the others, including through Window menus and restored layouts.
Clicking the active Console / History / Terminal status button collapses it.
The status bar is taller, with compact rounded buttons and an inset that keeps
their selected backgrounds clear of the window corners. The runtime toolbar is
also taller; neither bar relies on extra space between labels for its size.
The color dialog uses editor chrome and the workspace palette. The asset picker
also opts into that palette as a standalone popup, with a wider 680-pixel layout,
larger name cells, full-path tooltips, and placement constrained to its screen.
The layout is saved separately as `*_workspace_v2.state`; Window > Reset Layout
restores the new default without overwriting the previous layout file.
The top toolbar uses a single-line brand and 28-pixel controls. Dock titles use
small inset tabs and one close action for the active panel; floating remains available
by dragging or through the title context menu. Hovering the Scene no longer
replaces the bottom tabs with shortcut instructions.
The Scene tool row is a native horizontal layout that measures the full button
boxes and keeps space above and below their borders. It has no fixed or stylesheet
maximum height. The T/R/S snap-step input boxes have been removed; snapping still
uses the existing backend defaults and can be toggled with the grid button or Ctrl.

## Controls

| Area | Input | Behavior |
| --- | --- | --- |
| Scene | Q / W / E / R | Hand / Move / Rotate / Scale, while the viewport has focus |
| Scene | F | Frame the selected object in 2D or 3D |
| Scene | Left drag with Hand | Pan without changing selection |
| Scene | Middle drag | Pan |
| Scene | Alt + left drag | Orbit in 3D; pan in 2D |
| Scene | Alt + right drag | Dolly / zoom |
| Scene | Right drag + WASD / QE | Fly; Shift increases movement speed |
| Scene | Right click without navigating | Scene tools and Frame Selected menu |
| Scene | Ctrl click / Shift click | Toggle / add to object selection |
| Gizmo | Axis / plane handle drag | Constrain translation to an axis / plane |
| Gizmo | Ctrl during drag | Temporarily snap |
| Gizmo | Esc during drag | Restore the original transforms |
| Hierarchy | Double click or F | Frame the selected object |
| Hierarchy | Drop onto empty space | Reparent to the scene root |
| Hierarchy / Project | F2 | Rename the selected row; text editing keeps normal text shortcuts |
| Project | Single / double click | Inspect / open; folders open on double click |
| Project | Search or type filter | Search the asset tree, including nested folders |
| Project | Back / parent; Alt+Left / Alt+Up | Navigate folders |
| Project | List button | Switch between thumbnails and compact rows |
| Inspector | Drag a numeric label or X/Y/Z | Apply values continuously; one gesture produces one undo entry |
| Inspector | Shift / Ctrl during numeric drag | Fine / coarse adjustment, without a jump when modifiers change |
| Inspector | Esc during numeric drag | Restore the original value |
| Inspector | Click the value | Type a number; commit with Enter or focus change |
| Inspector | Component header right click | Copy current values, paste, reset, reorder, or remove where supported |
| Runtime | F5 / F6 / Shift+F5 | Play / Pause or Resume / Stop |

Assets itself is protected from renaming. Renaming child folders moves their
contents and metadata together. Folder copy, cut, duplicate, and delete remain
disabled in the context menu; existing file operations remain available.
The Terminal entry still reports the existing unimplemented terminal state.

Asset inspectors emphasize the icon, name, preview, and import settings. Paths,
GUIDs, and importer identifiers live in collapsed Technical details; copy and
reveal actions remain available. Apply and Revert are enabled only when import
settings differ from the original values.

## Interaction acceptance checks

- Drag Transform X, verify the scene responds before release, then undo once.
  Repeat with several selected objects whose Y and Z values differ: those axes
  must remain distinct. Repeat on an initially integral value with a fractional
  adjustment, and cancel a gesture with Esc.
- Copy a component after editing it, then paste or reset it. The fields must
  show the new values immediately, and the copied values must be the current ones.
- Rename a nested folder with F2 and the context menu, including one containing
  imported assets. Confirm GUIDs persist, the directory remains navigable, and
  Assets cannot be renamed. Try a duplicate or invalid name and verify the old
  row is restored.
- Search for an asset in a different folder, combine search with a type filter,
  clear both, and switch between list and thumbnail views. Refresh while a scene
  object is selected: the selection must not be stolen by the asset browser.
- In 2D and 3D, compare visible gizmo handles with their hover areas. Check plane
  movement, rotation across the angle boundary, scale sensitivity at several
  zoom levels, and cancelling a multi-object drag with Esc.
- Hold movement keys with the right mouse button, then move focus to an input.
  Camera movement must stop. A right-button drag must not open the context menu.
- Close, reopen, float, and redock panels. Check status buttons, log counts,
  layout persistence, and restoring the default layout.
- Open Console, History, and Terminal in succession from both the status bar and
  Window menu. Only one tool may remain open, even after floating or splitting
  them. Repeat with Workflow / Tile Palette and the log-count shortcut. Click
  the active status button to collapse it; restore a layout containing multiple
  bottom tools and verify it retains only one. A fully collapsed layout must
  remain collapsed after reopening.
- Open color fields in the Inspector and remote script controls. Check readable
  RGB / HSV / HTML inputs, swatches, alpha where supported, screen color picking,
  accept, cancel, and the close button. Inspect script toolbars, Workflow headers,
  details, and scrollbars for consistent Dracula surfaces.
- Open an asset picker near the right and bottom screen edges, including at
  higher display scaling. Confirm the entire popup stays on screen and that
  searching, selecting None, and selecting an asset still work.

Source and resource validation does not replace these runtime interaction checks.

## Scene overlays

Editor scene tools live in `engine/src/editor/scene/`. `SceneGizmoController`
owns handle geometry, hit testing, snapping, and drag state. Its context borrows
the current scene, camera, selection, and event sink for each operation; it does
not retain a runtime application or backend. `SceneGrid` configures the grid
independently of selection, `SceneSelectionBounds` supplies sprite and mesh
bounds, and `TilemapOverlay` draws the tile-edit guides. RuntimeBackend routes
input, publishes draw data, and handles runtime picking in RuntimeBackendPicking.

The editor camera uses Y-up, zero-to-one clip coordinates. Both screen projection
and mouse unprojection convert from the viewport's top-left origin. The RHI
already handles Vulkan's viewport inversion, so the editor projection does not
flip Y a second time. The editor background is independent of the game camera.

Selection bounds use sprite natural size (texture dimensions, UV crop, and
pixels per unit), its rotation/scale, or mesh bounds and ancestor transforms.
Empty objects display a pivot marker. Sprite instance data retains rotation and
reflection so the image and its selection geometry agree.

- With no selection, switch between 2D and 3D and zoom: minor/major grid lines
  and origin axes remain visible. Check tiny and very large zoom scales.
- In 2D, X points right and Y points up. Hit and drag either axis or the XY plane
  at 100%, 150%, and 200% display scaling; drawing and picking must agree.
- Select cropped sprites with non-default pixels per unit, rotate them, and use
  negative/nonuniform scale. Their outlines must follow all four rendered corners.
  Frame Selected must include the actual image rather than a unit-sized box.
- Select multiple sprites or meshes, switch tools, and cancel a drag with Esc.
  The prior transforms and one-gesture undo behavior must remain intact.
- Check rounded folder selection and status-bar buttons near the window's
  bottom corners, both maximized and restored.
