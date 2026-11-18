# Droidulator UI guidelines

Use the shared ThemePalette for dark, light and custom themes. Use monospace for expressions, code, results, control labels and help examples.

Use an 8 dp corner radius for buttons, fields, cards, bars, popups and dialogs. Compact toolbar controls are 36 dp high with 20 dp icons. Keep 4 dp between adjacent controls and between the script header and code field. Use the shared toolbar dimension resources for these measurements. Spinner anchors have zero horizontal padding so the popup outer edges align with the anchor. Put label padding inside the adapter row. Popup background padding and horizontal offset must be zero. Keep format, precision and angle controls at the right edge.

Use green for section selection, orange for number format and yellow for precision and angle mode. Popup rows use the same typography and accent as their anchor. Highlight the current selection with the anchor color. Give rows at least the anchor height and 8 dp vertical text padding.

Search replaces section selection. Apply theme text and hint colors. Filter as text changes. Enter hides the keyboard. Back and the search button clear the query and restore section selection.

Cards show a name and a descriptive second line. Align icon actions at the right. Give every icon an accessible label.

Help uses the common dialog, padded scrollable content and a themed confirmation button. Show syntax, purpose and examples consistently. Every supported function and operator needs a discoverable entry or alias.

## Gruvbox palette reference

These HEX values define the default palettes. Dark mode uses bright accents. Light mode uses faded accents. The neutral accent family is available for secondary states. Source [Gruvbox color palettes and HEX codes](https://gruvbox.org/gruvbox-color-palettes-hex-codes/).

### Dark theme

- Background hard `#1D2021`, medium `#282828`, soft `#32302F`.
- Surface levels dark 1 `#3C3836`, dark 2 `#504945`, dark 3 `#665C54`, dark 4 `#7C6F64`.
- Foreground `#EBDBB2`. Light foreground scale light 0 `#FBF1C7`, light 1 `#EBDBB2`, light 2 `#D5C4A1`, light 3 `#BDAE93`, light 4 `#A89984`.
- Bright red `#FB4934`, green `#B8BB26`, yellow `#FABD2F`, blue `#83A598`, purple `#D3869B`, aqua `#8EC07C`, orange `#FE8019`.

### Light theme

- Background hard `#F9F5D7`, medium `#FBF1C7`, soft `#F2E5BC`.
- Surface levels light 1 `#EBDBB2`, light 2 `#D5C4A1`, light 3 `#BDAE93`, light 4 `#A89984`.
- Foreground `#3C3836`. Dark foreground scale dark 0 `#282828`, dark 1 `#3C3836`, dark 2 `#504945`, dark 3 `#665C54`, dark 4 `#7C6F64`.
- Faded red `#9D0006`, green `#79740E`, yellow `#B57614`, blue `#076678`, purple `#8F3F71`, aqua `#427B58`, orange `#AF3A03`.

### Neutral accents

- Red `#CC241D`, green `#98971A`, yellow `#D79921`, blue `#458588`, purple `#B16286`, aqua `#689D6A`, orange `#D65D0E`.

## Custom theme support

Every visible color must resolve through the shared theme system. Define default HEX values centrally in ThemePalette and shared theme resources. Views, adapters, dialogs, popups, icons, syntax highlighting, plots, search fields, selection states, error states and system bars must use theme roles. Add an overridable theme role whenever a component needs another color.

Custom themes must configure every color role used by the interface. Resolve missing keys through the corresponding light or dark default palette. Apply theme changes to open screens and recreated components. Use the same resolved roles for a popup and its anchor, including the selected row, foreground and icon tint.

Hardcoded component colors, literal HEX values in view logic and fixed framework accent colors are prohibited. Validate dark, light and custom themes, including popup selection, search, disabled controls, dialogs and contrast between text and background.
