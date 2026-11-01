// GPL-2.0-or-later.
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "native_orb_chart.h"
#include "actors.h"
#include "effects.h"
#include "font.h"
#include "font_map.h"
#include "fnames.h"
#include "game.h"
#include "gamewin.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "ibuf8.h"
#include "imagewin.h"
#include "Modal_gump.h"
#include "mouse.h"
#include "paths.h"
#include "ready.h"
#include "shapeid.h"
#include "shapevga.h"
#include "ucmachine.h"
#include <algorithm>
#include <array>
#include <memory>
#include <cmath>
#include <SDL3/SDL_rect.h>

namespace {
bool installed = false;
Game_object* party_item(int shape) {
    auto* w = Game_window::get_instance();
    Actor* party[9];
    const int count = w->get_party(party, 1);
    for (int i = 0; i < count; ++i)
        if (auto* item = party[i]->find_item(shape, c_any_qual, 0)) return item;
    return nullptr;
}
bool party_owns(Game_object* item) {
    if (!item) return false;
    auto* outer = item->get_outermost();
    auto* w = Game_window::get_instance();
    Actor* party[9];
    const int count = w->get_party(party, 1);
    return std::find(party, party + count, outer) != party + count;
}
bool readable(Game_object* item) {
    // Like an ordinary BG book/map, a reachable chart can also be read on the
    // ground. The hotkey only locates party inventory; it never searches here.
    return party_owns(item) || (item && !item->get_owner()
        && item->get_map_num()==Game_window::get_instance()->get_main_actor()->get_map_num()
        && Fast_pathfinder_client::is_grabable(Game_window::get_instance()->get_main_actor(),item));
}
constexpr int dx[] = {0,1,1,1,0,-1,-1,-1};
constexpr int dy[] = {-1,-1,0,1,1,1,0,-1};
const char* retail[] = {"Britain","Jhelom","Yew","Minoc","Trinsic","New\nMagincia","Skara\nBrae","Moonglow"};
const char* towns[] = {"Britain","Jhelom","Yew","Minoc","Trinsic","Skara\nBrae","New\nMagincia","Moonglow"};
const char* shrines[] = {"Compassion","Valor","Justice","Sacrifice","Honor","Spirituality","Humility","Honesty"};
const char* directions[] = {"north","northeast","east","southeast","south","southwest","west","northwest"};

class Chart_gump;
class Chart_mark : public Gump_button {
    int direction;
    bool outer_ring;
    std::string action;
    TileRect area, stamp_area;
public:
    Chart_mark(Chart_gump* par, int dir, bool outer, TileRect rect, TileRect stamp = {});
    const char* cli_action() const override { return action.c_str(); }
    TileRect get_rect() const override;
    Gump_button* on_button(int mx, int my) override;
    bool push(MouseButton b) override { return b == MouseButton::Left; }
    void unpush(MouseButton) override {}
    void paint() override {}
    bool activate(MouseButton b) override;
};

// Rune stones from original BG shapes.vga, shape 877. Frame associations are
// also documented by content/bgkeyring/src/items/shrines.uc (Honor is frame 6).
// Crop only the engraved strokes, not the stone outline. No invented symbols.
struct Rune_engraving { int x,y,w,h,background; };
constexpr Rune_engraving engravings[] = {
    {5,3,6,8,160}, {4,3,9,8,115}, {5,3,5,9,115}, {5,2,6,9,115},
    {4,3,6,8,145}, {4,4,7,6,115}, {4,3,7,8,160}, {4,3,8,9,144}
};
constexpr int retail_runes[] = {5,4,1,0,6,2,3,7};
constexpr int keyring_runes[] = {5,4,1,0,6,3,2,7};

class Chart_gump : public Modal_gump {
    int layout;
    std::unique_ptr<Shape_frame> paper;
    std::array<std::unique_ptr<Shape_frame>,8> small_runes, large_runes, retail_rune_art;
    std::array<unsigned char,256> ink_map;
    std::shared_ptr<Font> title_font;
    void text(const char* value,int cx,int top,bool heading=false,bool town=false) {
        auto f=heading?title_font:fontManager.get_font("SMALL_BLACK_FONT");
        auto* ib=gwin->get_win()->get_ib8();
        auto title_ink=ink_map; title_ink[0]=135;
        auto town_ink=ink_map; town_ink[0]=0;
        std::string label(value);
        auto split=label.find('\n');
        auto line=[&](const std::string& part,int py) {
            f->paint_text(ib,part.c_str(),x+cx-f->get_text_width(part.c_str())/2,y+py,heading?title_ink.data():town?town_ink.data():ink_map.data());
        };
        line(label.substr(0,split),top);
        if(split!=std::string::npos) line(label.substr(split+1),top+f->get_text_height());
    }
    void rune(int frame,int cx,int cy,bool large,bool monumental=false) {
        auto* glyph=(monumental?retail_rune_art:large?large_runes:small_runes)[frame].get();
        if(glyph) glyph->paint(gwin->get_win()->get_ib8(),x+cx,y+cy);
    }
    static std::string town_name(int d) {
        std::string name(towns[d]);
        std::replace(name.begin(),name.end(),'\n',' ');
        return name;
    }
    static SDL_Point town_position(int d) { return {160+73*dx[d],111+28*dy[d]}; }
    static SDL_Point town_stamp(int d) {
        constexpr int sx[]={160,233,207,233,160,87,133,87};
        constexpr int sy[]={94,94,111,128,130,128,111,94};
        return {sx[d],sy[d]};
    }
    static SDL_Point shrine_position(int d) {
        return {d==6?30:d==2?290:160+116*dx[d],dy[d]<0?54:dy[d]>0?155:111};
    }
    static int shrine_caption_x(int d) { return d==6?42:d==2?278:160+116*dx[d]; }
    TileRect town_label_area(int d) {
        const auto pos=town_position(d);
        auto f=fontManager.get_font("SMALL_BLACK_FONT");
        const int w=f->get_text_width(town_name(d).c_str());
        return TileRect(pos.x-w/2-1,pos.y-5,w+2,10);
    }
    static TileRect town_rune_area(int d) {
        const auto pos=town_stamp(d);
        const auto& crop=engravings[keyring_runes[d]];
        return TileRect(pos.x-crop.w/2-1,pos.y-crop.h/2-1,crop.w+2,crop.h+2);
    }
    void distance_ring(int rx,int ry,const std::vector<TileRect>& clear) {
        auto* ib=gwin->get_win()->get_ib8();
        auto dot=[&](int px,int py) {
            if((px+py)%4==0) return;
            for(const auto& area:clear) if(area.has_point(px,py)) return;
            ib->put_pixel8(138,x+px,y+py);
        };
        // A native one-pixel pen line. Gaps around inked names and rune marks
        // keep the two distance circuits secondary to the destinations.
        for(int px=-rx;px<=rx;++px) {
            const int py=int(std::lround(ry*std::sqrt(1.-double(px*px)/double(rx*rx))));
            dot(160+px,111+py);dot(160+px,111-py);
        }
        for(int py=-ry;py<=ry;++py) {
            const int px=int(std::lround(rx*std::sqrt(1.-double(py*py)/double(ry*ry))));
            dot(160+px,111+py);dot(160-px,111+py);
        }
    }
    void rose(int cx,int cy,int radius) {
        auto* ib=gwin->get_win()->get_ib8();
        auto dot=[&](int px,int py,unsigned char color=144) { ib->put_pixel8(color,x+px,y+py); };
        auto cross=[](int ax,int ay,int bx,int by,int px,int py) {
            return (bx-ax)*(py-ay)-(by-ay)*(px-ax);
        };
        // Eight solid, alternating ink points, rather than radiating limbs.
        // The diagonal points are shorter so the rose has cardinal bearings.
        const int base=std::max(2,radius/4);
        for(int d=0;d<8;++d) {
            const int reach=d%2?radius*7/10:radius;
            const int tx=dx[d]*reach,ty=dy[d]*reach;
            const int ax=dx[d]*base+dy[d]*base,ay=dy[d]*base-dx[d]*base;
            const int bx=dx[d]*base-dy[d]*base,by=dy[d]*base+dx[d]*base;
            for(int py=-radius;py<=radius;++py) for(int px=-radius;px<=radius;++px) {
                const int e1=cross(ax,ay,bx,by,px,py),e2=cross(bx,by,tx,ty,px,py),e3=cross(tx,ty,ax,ay,px,py);
                if((e1>=0 && e2>=0 && e3>=0)||(e1<=0 && e2<=0 && e3<=0))
                    dot(cx+px,cy+py,d%2?141:144);
            }
        }
        const int bezel=radius>=16?8:6;
        for(int py=-bezel;py<=bezel;++py) for(int px=-bezel;px<=bezel;++px) {
            const int r2=px*px+py*py;
            if(r2<=bezel*bezel) dot(cx+px,cy+py,r2>=(bezel-1)*(bezel-1)?144:135);
        }
        // An ankh inside the circular hub: a closed loop, crossbar and stem.
        constexpr int loop[][2]={{-1,-4},{0,-4},{1,-4},{-2,-3},{2,-3},{-2,-2},{2,-2},{-1,-1},{0,-1},{1,-1}};
        for(const auto& point:loop) dot(cx+point[0],cy+point[1]);
        for(int py=0;py<=4;++py) dot(cx,cy+py);
        for(int px=-3;px<=3;++px) dot(cx+px,cy+1);
    }
public:
    int direction = -1;
    bool outer_ring = false;
    Chart_gump(int profile) : Modal_gump(nullptr, game->get_shape("gumps/scroll")), layout(profile) {
        title_font=std::make_shared<Font>(File_spec(FONTS_VGA),0,-1);
        ink_map.fill(144); ink_map[255]=255; ink_map[0]=144;
        // Original BG scroll pixels, cropped at the endposts to give a usable
        // full-width chart at 320x200. Paper uses BG's warm parchment ramp.
        auto* source=get_shape();
        if(source && source->get_width()==224 && source->get_height()==173) {
            Image_buffer8 original(224,173);original.fill8(255);
            source->paint(&original,source->get_xleft(),source->get_yabove());
            std::array<unsigned char,320*200> pixels;
            for(int py=0;py<200;++py) for(int px=0;px<320;++px) {
                const int sx=px<14?34+px:px>=306?194+(px-306):48+(px-14)%144;
                const int sy=py<24?py:py>=176?149+(py-176):30+(py-24)%110;
                auto color=original.get_pixel8(sx,sy);
                if(color>=209 && color<=219) color-=75;
                pixels[py*320+px]=color;
            }
            paper=std::make_unique<Shape_frame>(pixels.data(),320,200,0,0,true);
        }
        for(int f=0;f<8;++f) {
            ShapeID stone(877,f,SF_SHAPES_VGA);
            auto* source_rune=stone.get_shape();
            if(!source_rune) continue;
            Image_buffer8 original(source_rune->get_width(),source_rune->get_height());
            original.fill8(255);
            source_rune->paint(&original,source_rune->get_xleft(),source_rune->get_yabove());
            const auto& crop=engravings[f];
            for(int scale=1;scale<=3;++scale) {
                const int w=crop.w*scale,h=crop.h*scale;
                std::vector<unsigned char> pixels(w*h,255);
                for(int py=0;py<crop.h;++py) for(int px=0;px<crop.w;++px) {
                    const auto c=original.get_pixel8(crop.x+px,crop.y+py);
                    if(c==crop.background || c==0 || c==255) continue;
                    const auto ink=c>crop.background?144:141;
                    for(int yy=0;yy<scale;++yy) for(int xx=0;xx<scale;++xx)
                        pixels[(py*scale+yy)*w+px*scale+xx]=ink;
                }
                auto glyph=std::make_unique<Shape_frame>(pixels.data(),w,h,w/2,h/2,true);
                (scale==1?small_runes:scale==2?large_runes:retail_rune_art)[f]=std::move(glyph);
            }
        }
        center_rect_on_screen(TileRect(0,0,320,200));
        add_elem(new Chart_mark(this,-1,false,layout==1?TileRect(142,84,37,37):TileRect(148,99,25,25)));
        for(int d=0;d<8;++d) {
            if(layout==1) {
                const int cx=160+105*dx[d],cy=103+45*dy[d];
                add_elem(new Chart_mark(this,d,false,TileRect(cx-43,cy-14,86,43)));
            } else {
                auto town_hit=town_label_area(d), stamp=town_rune_area(d);
                // Comfortable native hit halos; keep the approved ink/ring masks unchanged.
                town_hit=TileRect(town_hit.x-2,town_hit.y-2,town_hit.w+4,town_hit.h+4);
                stamp=TileRect(stamp.x-2,stamp.y-2,stamp.w+4,stamp.h+4);
                add_elem(new Chart_mark(this,d,false,town_hit,stamp));
                const auto pos=shrine_position(d);
                auto f=fontManager.get_font("SMALL_BLACK_FONT");
                const int w=f->get_text_width(shrines[d]);
                const auto& crop=engravings[keyring_runes[d]];
                TileRect glyph(pos.x-crop.w-1,pos.y-crop.h-1,crop.w*2+2,crop.h*2+2);
                TileRect label(shrine_caption_x(d)-w/2-1,pos.y+11,w+2,10);
                add_elem(new Chart_mark(this,d,true,glyph.add(label)));
            }
        }
    }
    TileRect get_rect() const override { return TileRect(x,y,320,200); }
    bool ready() const { return bool(paper); }
    bool has_point(int px,int py) const override { return get_rect().has_point(px,py); }
    bool is_draggable() const override { return false; }
    void choose(int dir,bool outer) { direction=dir;outer_ring=outer;done=true; }
    void close() override { direction=-1;done=true; }
    void paint() override {
        auto* ib=gwin->get_win()->get_ib8();
        if(paper) paper->paint(ib,x,y);
        text("Orb of the Moons",160,25,true);
        // Fine marginal penwork, not a matrix of cell borders.
        for(int py=37;py<159;++py) if(py%4!=0) {
            ib->put_pixel8(139,x+12,y+py);ib->put_pixel8(139,x+307,y+py);
        }
        for(int side:{0,1}) {
            const int cx=side?303:16;
            for(int r=0;r<4;++r) {
                ib->put_pixel8(141,x+cx+(side?-r:r),y+31+r);
                ib->put_pixel8(144,x+cx+(side?-r:r),y+35-r);
            }
        }
        if(layout==1) {
            for(int d=0;d<8;++d) {
                const int cx=160+105*dx[d],cy=102+45*dy[d];
                rune(retail_runes[d],cx,cy,true,true);
                std::string name(retail[d]);
                std::replace(name.begin(),name.end(),'\n',' ');
                text(name.c_str(),cx,cy+18);
            }
            for(int d:{1,2,3,5,6,7}) for(int r=22;r<=37;++r) if(r%3!=0)
                ib->put_pixel8(138,x+160+dx[d]*r,y+102+dy[d]*r);
            rose(160,102,18);
        } else {
            auto small=fontManager.get_font("SMALL_BLACK_FONT");
            text("Shrines of Virtue",160,39);
            std::vector<TileRect> clear;
            clear.emplace_back(100,23,120,26);
            clear.emplace_back(145,96,31,31);
            for(int d=0;d<8;++d) {
                clear.push_back(town_label_area(d));clear.push_back(town_rune_area(d));
                const auto pos=shrine_position(d);
                const auto& crop=engravings[keyring_runes[d]];
                clear.emplace_back(pos.x-crop.w-2,pos.y-crop.h-2,crop.w*2+4,crop.h*2+4);
                const int w=small->get_text_width(shrines[d]);
                clear.emplace_back(shrine_caption_x(d)-w/2-2,pos.y+10,w+4,12);
            }
            distance_ring(89,38,clear);
            distance_ring(130,62,clear);
            for(int d=0;d<8;++d) {
                const auto pos=town_position(d),stamp=town_stamp(d);
                rune(keyring_runes[d],stamp.x,stamp.y,false);
                text(town_name(d).c_str(),pos.x,pos.y-4,false,true);
                const auto outer=shrine_position(d);
                rune(keyring_runes[d],outer.x,outer.y,true);
                text(shrines[d],shrine_caption_x(d),outer.y+12);
            }
            rose(160,111,12);
        }
        gwin->set_painted();
    }
};
Chart_mark::Chart_mark(Chart_gump* par, int dir, bool outer, TileRect rect, TileRect stamp)
    : Gump_button(par,-1,rect.x,rect.y), direction(dir), outer_ring(outer),
      action(dir < 0 ? "cancel" : std::string(outer ? "far-" : "near-")+directions[dir]), area(rect), stamp_area(stamp) {}
TileRect Chart_mark::get_rect() const {
    const auto bounds=stamp_area.w?area.add(stamp_area):area;
    return TileRect(parent->get_x()+bounds.x,parent->get_y()+bounds.y,bounds.w,bounds.h);
}
Gump_button* Chart_mark::on_button(int mx, int my) {
    mx-=parent->get_x();my-=parent->get_y();
    if(area.has_point(mx,my)) return this;
    if(!stamp_area.w) return nullptr;
    if(stamp_area.has_point(mx,my)) return this;
    // Join a caption to its own rune with a narrow hit corridor, without
    // taking the neighbouring shrine's lettering into the town's hit area.
    const auto bounds=area.add(stamp_area);
    const int left=std::max(area.x,stamp_area.x),right=std::min(area.x+area.w,stamp_area.x+stamp_area.w);
    const int top=std::max(area.y,stamp_area.y),bottom=std::min(area.y+area.h,stamp_area.y+stamp_area.h);
    return ((mx>=left && mx<right && my>=bounds.y && my<bounds.y+bounds.h)
        || (my>=top && my<bottom && mx>=bounds.x && mx<bounds.x+bounds.w))?this:nullptr;
}
bool Chart_mark::activate(MouseButton b) {
    if (b != MouseButton::Left) return false;
    static_cast<Chart_gump*>(parent)->choose(direction,outer_ring);
    return true;
}
struct Target_scope {
    Usecode_machine* machine;
    Game_object* previous_item;
    Tile_coord* previous_tile;
    Target_scope(Usecode_machine* m, Tile_coord target) : machine(m) {
        machine->save_intercept(previous_item,previous_tile);
        machine->intercept_click_on_tile(new Tile_coord(target));
    }
    ~Target_scope() { machine->restore_intercept(previous_item,previous_tile); }
};
}

void Orb_chart::install_shape(Shapes_vga_file& shapes) {
    installed = false;
    if (!GAME_BG || (shape < shapes.get_num_shapes() && shapes.get_num_frames(shape))) return;
    // Hand-pixelled folded paper, using BG's original paper/edge palette ramps.
    constexpr const char* fold[] = {
        "  bbbbbbbbbb  ", " bpppppppppbd ", "bplllpllllbbd ", "bplllplllbbpd ",
        "bpppppppbbppd ", "bplllplbbpppd ", "bplllpbbppppd ", "bppppbbpppppd ",
        "bpllbblpppppd ", "bplbbplpppppd ", "bpbbppppppppd ", " bddddddddddd ",
        "  dddddddddd  "};
    unsigned char pixels[14*13];
    for (int y=0;y<13;++y) for(int x=0;x<14;++x) {
        const char c=fold[y][x];
        pixels[y*14+x]=c=='b'?215:c=='d'?217:c=='p'?210:c=='l'?212:255;
    }
    auto* item_shape=shapes.new_shape(shape);
    if(!item_shape) return;
    item_shape->add_frame(std::make_unique<Shape_frame>(pixels,14,13,7,12,true),0);
    Shape_info info;
    info.set_shape_class(Shape_info::quality);
    info.set_weight_volume(1,1);
    info.set_3d(1,1,0);
    shapes.set_info(shape,info);
    installed = true;
}
bool Orb_chart::is_chart(const Game_object* object) {
    return installed && GAME_BG && object && object->get_shapenum()==shape && object->get_framenum()==0;
}
bool Orb_chart::give() {
    if (!installed) return false;
    if (party_item(shape)) return true; // Only party possession, no world-wide duplicate search.
    auto* w=Game_window::get_instance();
    Actor* party[9];
    const int count=w->get_party(party,1);
    for(int i=0;i<count;++i) {
        auto* pack=party[i]->get_readied(backpack);
        if(pack && pack->add_quantity(1,shape,0,0)==0) return true;
        if(party[i]->add_quantity(1,shape,0,0)==0) return true;
    }
    return false;
}
void Orb_chart::open(Game_object* chart) {
    auto* w=Game_window::get_instance();
    auto* machine=w->get_usecode();
    if(!chart) chart=party_item(shape);
    if(!is_chart(chart) || !readable(chart) || machine->in_usecode() || w->get_gump_man()->modal_gump_mode()
        || machine->get_num_faces_on_screen() || w->main_actor_dont_move() || !w->main_actor_can_act()) return;
    const int profile=machine->orb_chart_layout();
    if(!profile) {
        w->get_effects()->center_text("Use the Orb by hand.");
        return;
    }
    auto generation=w->get_main_actor()->weak_from_this();
    auto owned_chart=chart->weak_from_this();
    Chart_gump dialog(profile);
    if(!dialog.ready()) return;
    if(!w->get_gump_man()->do_modal_gump(&dialog,Mouse::hand) || dialog.direction<0) return;
    auto player=generation.lock(), held=owned_chart.lock();
    if(!player || player.get()!=w->get_main_actor() || !held || !readable(held.get())
        || machine->orb_chart_layout()!=profile || machine->in_usecode()
        || w->main_actor_dont_move() || !w->main_actor_can_act()) return;
    auto* orb=party_item(785);
    if(!orb || orb->get_usecode()!=0x311) return;
    Tile_coord target=w->get_main_actor()->get_tile();
    const int distance=dialog.outer_ring?10:4;
    target.tx+=dx[dialog.direction]*distance;
    target.ty+=dy[dialog.direction]*distance;
    // No gate creation, quest checks, destination coordinates or travel here.
    // Even early Usecode returns restore any previous pending target.
    Target_scope scope(machine,target);
    orb->activate(Usecode_machine::double_click);
}
