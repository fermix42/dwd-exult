#game "blackgate"
// Explicit frame-lag comparison fixture: restores the old delayed-single-call
// mechanism through a conventional read-only game patch. Never a product resource.
extern void legacyUseLight object#(0x600) ();
void legacyStartLight 0x905 (var obj) {
    obj->halt_scheduled();
    obj->set_light(true);
    UI_set_time_palette();
    script obj after 50 ticks { nohalt; call legacyUseLight; }
}
