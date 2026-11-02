// GPL-2.0-or-later. Exult-owned physical Virtue Stone Case, provisional artwork.
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "native_virtue_case.h"
#include "Gump.h"
#include "Gump_manager.h"
#include "contain.h"
#include "font.h"
#include "font_map.h"
#include "game.h"
#include "gamewin.h"
#include "ibuf8.h"
#include "imagewin.h"
#include "objiter.h"
#include "shapevga.h"
#include "ready.h"
#include <array>
#include <memory>
#include <vector>

namespace {
bool installed = false;
constexpr int width = 200, height = 172;
// Original BG shape 330 frames: yellow/red/blue/orange/green/purple/black/white.
// Fixed columns read top to bottom: Honesty..Justice, Sacrifice..Humility.
constexpr int stone_frames[] = {2, 0, 1, 4, 3, 5, 7, 6};
constexpr const char* virtues[] = {"Honesty", "Compassion", "Valor", "Justice",
    "Sacrifice", "Honor", "Spirituality", "Humility"};
int slot_for(const Game_object* object) {
    if (!object || !object->as_virtstone() || object->get_shapenum() != 330) return -1;
    for (int i = 0; i < 8; ++i) if (object->get_framenum() == stone_frames[i]) return i;
    return -1;
}
int slot_x(int slot) { return 27 + (slot / 4) * 98; }
int slot_y(int slot) { return 50 + (slot % 4) * 32; }

// Pixel artwork belongs to this feature, following Orb_chart::install_shape.
// Palette indices are original BG wood, brass, burgundy and parchment ramps.
std::unique_ptr<Shape_frame> case_art(bool closed) {
    const int w = closed ? 23 : width, h = closed ? 16 : height;
    std::vector<unsigned char> pixels(w*h, 255);
    auto rect = [&](int x, int y, int rw, int rh, unsigned char c) {
        for (int py = y; py < y+rh; ++py) for (int px = x; px < x+rw; ++px) pixels[py*w+px] = c;
    };
    rect(1, 1, w-2, h-2, 147);
    rect(2, 2, w-4, h-4, 140);
    rect(3, 3, w-6, h-6, 144);
    for (int y = 5; y < h-4; y += 3) rect(4, y, w-8, 1, 145);
    if (closed) {
        rect(3, 10, 17, 1, 147);
        rect(10, 9, 3, 4, 8); rect(11, 10, 1, 1, 2);
        rect(3, 2, 3, 2, 9); rect(17, 2, 3, 2, 9);
    } else {
        rect(7, 7, w-14, 19, 29);
        rect(7, 30, w-14, h-38, 30);
        rect(10, 32, w-20, h-42, 29);
        rect(97, 32, 5, h-42, 145);
        for (int i = 0; i < 8; ++i) {
            const int x = slot_x(i), y = slot_y(i);
            rect(x-13, y-13, 27, 27, 30);
            rect(x-11, y-11, 23, 23, 147);
            rect(x+16, y-1, 49, 12, 9);
            rect(x+17, y, 47, 10, 135); // Blank handwritten destination note.
            rect(x+18, y+1, 45, 8, 134);
        }
        rect(25, 27, 13, 4, 9); rect(26, 27, 11, 1, 3);
        rect(162, 27, 13, 4, 9); rect(163, 27, 11, 1, 3);
        rect(94, h-6, 12, 4, 8); rect(96, h-5, 8, 1, 3);
    }
    return std::make_unique<Shape_frame>(pixels.data(), w, h, closed ? w/2 : 0, closed ? h-1 : 0, true);
}

class Case_gump : public Gump {
    std::unique_ptr<Shape_frame> background;
    std::array<std::unique_ptr<Shape_frame>, 8> seated;
public:
    explicit Case_gump(Container_game_object* cont) : Gump(cont, 0, 0, -1), background(case_art(false)) {
        set_object_area(TileRect(0, 0, width, height), 8, 162);
        for (int i = 0; i < 8; ++i) {
            ShapeID source(330, stone_frames[i]);
            auto* original = source.get_shape();
            if (!original) continue;
            const int sw = original->get_width(), sh = original->get_height();
            Image_buffer8 small(sw, sh); small.fill8(255);
            original->paint(&small, original->get_xleft(), original->get_yabove());
            const int scale = 3, w = sw*scale, h = sh*scale;
            std::vector<unsigned char> pixels(w*h, 255);
            for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
                pixels[y*w+x] = small.get_pixel8(x/scale, y/scale);
            seated[i] = std::make_unique<Shape_frame>(pixels.data(), w, h, w/2, h/2, true);
        }
        set_pos();
    }
    Gump* clone(Container_game_object* cont, int initx, int inity) override {
        auto* result = new Case_gump(cont); result->set_pos(initx, inity); return result;
    }
    TileRect get_rect() const override { return TileRect(x, y, width, height); }
    bool has_point(int sx, int sy) const override { return background->has_point(sx-x, sy-y); }
    const char* get_click_name() const override { return "Virtue Stone Case"; }
    bool add(Game_object* obj, int, int, int, int, bool dont_check, bool) override {
        return obj && container->add(obj, dont_check, false);
    }
    void paint() override {
        background->paint(gwin->get_win()->get_ib8(), x, y);
        paint_elems();
        auto font = fontManager.get_font("SMALL_BLACK_FONT");
        std::array<unsigned char, 256> ink;
        for (int i = 0; i < 256; ++i) ink[i] = i;
        ink[0] = 135;
        for (int i = 0; i < 8; ++i)
            font->paint_text(gwin->get_win()->get_ib8(), virtues[i], x+10+(i/4)*98, y+slot_y(i)-16, ink.data());
        Object_iterator next(container->get_objects());
        while (auto* object = next.get_next()) {
            const int slot = slot_for(object);
            if (slot >= 0 && seated[slot]) {
                Virtue_case::seat(container, object);
                seated[slot]->paint(gwin->get_win()->get_ib8(), x+slot_x(slot), y+slot_y(slot));
            }
        }
    }
    Game_object* find_object(int mx, int my) override {
        Object_iterator next(container->get_objects());
        while (auto* object = next.get_next()) {
            const int slot = slot_for(object);
            if (slot >= 0 && seated[slot] && seated[slot]->has_point(mx-x-slot_x(slot), my-y-slot_y(slot))) {
                // Same grab-anchor technique as Jawbone_gump::set_to_spot.
                // Dragging_info paints the original object, never this seated art.
                const auto* normal = object->get_shape();
                object->set_shape_pos(mx-x+normal->get_xleft()-normal->get_width()/2,
                    my-y+normal->get_yabove()-normal->get_height()/2);
                return object;
            }
        }
        return nullptr;
    }
};
}

void Virtue_case::install_shape(Shapes_vga_file& shapes) {
    installed = false;
    // Never claim a third-party shape. This mirrors the accepted Orb chart guard.
    if (!GAME_BG || (shape < shapes.get_num_shapes() && shapes.get_num_frames(shape))) return;
    auto* item = shapes.new_shape(shape);
    if (!item) return;
    item->add_frame(case_art(true), 0);
    Shape_info info;
    info.set_shape_class(Shape_info::container);
    info.set_weight_volume(10, 3);
    info.set_ready_type(lhand);
    info.set_alt_ready(rhand, backpack);
    info.set_3d(1, 1, 1);
    shapes.set_info(shape, info);
    installed = true;
}
bool Virtue_case::is_case(const Game_object* object) {
    return installed && GAME_BG && object && object->get_shapenum() == shape && object->get_framenum() == 0;
}
bool Virtue_case::accepts(const Container_game_object* cont, const Game_object* object) {
    if (!is_case(cont)) return true;
    const int slot = slot_for(object);
    if (slot < 0) return false;
    Object_iterator next(const_cast<Container_game_object*>(cont)->get_objects());
    while (auto* existing = next.get_next()) if (slot_for(existing) == slot) return false;
    return true;
}
void Virtue_case::seat(Container_game_object* cont, Game_object* object) {
    if (is_case(cont)) {
        const int slot = slot_for(object);
        if (slot >= 0) object->set_shape_pos(slot_x(slot), slot_y(slot));
    }
}
void Virtue_case::open(Container_game_object* cont) {
    auto* manager = Game_window::get_instance()->get_gump_man();
    if (!is_case(cont) || manager->find_gump(cont, -1)) return;
    manager->add_gump(new Case_gump(cont));
    Game_window::get_instance()->set_all_dirty();
}
