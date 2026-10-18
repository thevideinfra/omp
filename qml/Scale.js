// Imported with: import "Scale.js" as S
// Deliberately not a pragma library: it runs in the importer's context, so the
// `ui` and `theme` context properties resolve here and bindings that call
// these functions re-evaluate when density, font size or the shell font base
// change.

function sp(px) {
    return Math.max(1, Math.round(px * ui.densityScale))
}

function fs(px) {
    return Math.max(8, Math.round(px * theme.fontBase / 12 * ui.fontScale))
}
