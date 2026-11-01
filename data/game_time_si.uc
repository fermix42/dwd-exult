#game "serpentisle"
// SI has a distinct startLight entry and a different sconce transformation.
// The typed 435->481 descriptor preserves its original spent frame+2 completion.
void beginCalendarLight 0x950 (var obj) {
    var shape = obj->get_item_shape();
    if (shape != 701 && shape != 338 && shape != 435) { beginCalendarLight.original(obj); return; }
    obj->set_light(true);
    UI_set_time_palette();
    var registered;
    if (shape == 701) { registered = (@0xe6)(obj, 50, 595, 255, 65, 0x600); }
    else if (shape == 338) { registered = (@0xe6)(obj, 50, 997, -1, 65, 0x600); }
    else { registered = (@0xe6)(obj, 50, 481, -1, 65, 0x600); }
    if (!registered) beginCalendarLight.original(obj);
}
