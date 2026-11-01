#game "blackgate"
// Engine-bundled compatibility adapter for the original BG light lifecycle.
// Compiled into Exult; conventional mods may override it normally.
void beginCalendarLight 0x905 (var obj) {
    obj->set_light(true);
    UI_set_time_palette();
    var shape = obj->get_item_shape();
    var registered = false;
    if (shape == 701) { registered = (@0xe6)(obj, 50, 595, 255, 106, 0x600); }
    else if (shape == 338) { registered = (@0xe6)(obj, 50, 997, -1, 106, 0x600); }
    else if (shape == 435) { registered = (@0xe6)(obj, 50, 535, -1, 106, 0x600); }
    if (!registered) beginCalendarLight.original(obj);
}
