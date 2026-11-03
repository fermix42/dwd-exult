// Private implementation of the one-shot Guild delivery; included in this quest's namespace.
class Guild_delivery;
Guild_delivery* active_delivery=nullptr;
unsigned delivery_generation=0;
bool delivery_requested=false;
int delivery_phase=-1;
Tile_coord placed_at(-1,-1,-1);
Game_object* guild_workbench() {
    Game_object_vector tables;
    Game_object::find_nearby(tables,Tile_coord(1592,370,0),633,1,0);
    for(auto* table:tables)if(table->get_tile()==Tile_coord(1592,370,0))return table;
    return nullptr;
}
Tile_coord free_bench_spot(Game_object* bench) {
    if(!bench)return Tile_coord(-1,-1,-1);
    const auto at=bench->get_tile();const auto& info=bench->get_info();
    // Use the clear front-edge patch, not an arbitrary fallback behind tools/walls.
    const Tile_coord candidate(at.tx,at.ty,at.tz+info.get_3d_height());
    if(Map_chunk::find_spot(candidate,0,Virtue_case::shape,0)==candidate)return candidate;
    return Tile_coord(-1,-1,-1);
}
class Guild_delivery : public Actor_action {
    std::unique_ptr<Actor_action> walk;
    Game_object_weak bench;
    Tile_coord stand,spot;
    unsigned generation;
    int phase=0;
    int route_retries=0;
public:
    Guild_delivery(Actor_action* path,Game_object* table,const Tile_coord& approach,const Tile_coord& surface)
        :walk(path),bench(weak_from_obj(table)),stand(approach),spot(surface),generation(delivery_generation) {
        active_delivery=this;delivery_phase=0;
    }
    ~Guild_delivery() override { if(active_delivery==this){active_delivery=nullptr;delivery_phase=-1;} }
    int get_speed() const override { return 200; }
    bool get_dest(Tile_coord& dest) const override { dest=stand;return phase==0; }
    int handle_event(Actor* npc) override {
        if(generation!=delivery_generation)return 0;
        auto table=bench.lock();if(!table)return 0;
        delivery_phase=phase;
        if(phase==0) {
            bool deleted=false;const int delay=walk?walk->handle_event_safely(npc,deleted):0;
            if(deleted)return 0;
            if(delay)return delay;
            walk.reset();
            if(npc->get_tile()!=stand && route_retries++<3) {
                auto path=std::make_unique<Path_walking_actor_action>(nullptr,6,3);
                if(auto* route=path->walk_to_tile(npc,npc->get_tile(),stand)) {
                    if(route==path.get())path.release();walk.reset(route);npc->set_frame_time(200);return 200;
                }
            }
            if(npc->get_tile()!=stand) {npc->say("I cannot get to the table, Avatar. Give me a little room.");return 0;}
            phase=1;return 200;
        }
        const int dir=npc->get_direction(spot);
        if(phase==1) {npc->change_frame(npc->get_dir_framenum(dir,Actor::standing));phase=2;return 300;}
        if(phase==2) {npc->change_frame(npc->get_dir_framenum(dir,Actor::reach1_frame));phase=3;return 350;}
        if(phase==3) {
            if(quest.stage!=READY_FOR_DELIVERY)return 0;
            spot=free_bench_spot(table.get());
            if(spot.tx<0) {npc->change_frame(npc->get_dir_framenum(dir,Actor::standing));npc->say("There is no room for the case here. Clear a space on the table, please.");return 0;}
            auto object=Game_window::get_instance()->get_map()->create_ireg_object(Virtue_case::shape,0);
            if(!Virtue_case::is_case(object.get()))return 0;
            object->set_flag(Obj_flags::okay_to_take);object->move(spot);
            // One engine-thread operation: no save can split placement and terminal state.
            quest.stage=DELIVERED;placed_at=spot;
            Game_window::get_instance()->set_all_dirty();phase=4;return 250;
        }
        if(phase==4) {
            npc->change_frame(npc->get_dir_framenum(dir,Actor::standing));
            for(const auto& delta:std::array<Tile_coord,4>{Tile_coord(-2,0,0),Tile_coord(2,0,0),Tile_coord(0,2,0),Tile_coord(0,-2,0)}) {
                const Tile_coord aside(stand.tx+delta.tx,stand.ty+delta.ty,stand.tz);
                auto path=std::make_unique<Path_walking_actor_action>(nullptr,6,3);
                Actor_action* route=path->walk_to_tile(npc,npc->get_tile(),aside);
                if(route) {if(route==path.get())path.release();walk.reset(route);break;}
            }
            phase=5;return 200;
        }
        if(phase==5 && walk) {
            bool deleted=false;const int delay=walk->handle_event_safely(npc,deleted);if(deleted)return 0;if(delay)return delay;walk.reset();
        }
        npc->say("There! A fine piece of work, if I may say so myself. I trust it shall serve thee well, Avatar.");
        return 0;
    }
};
bool start_guild_delivery() {
    if(active_delivery || quest.stage!=READY_FOR_DELIVERY)return false;
    auto* w=Game_window::get_instance();auto* npc=w->get_npc(86);auto* bench=guild_workbench();
    const auto surface=free_bench_spot(bench);
    if(!npc || !npc->can_act() || !bench || surface.tx<0)return false;
    const auto at=bench->get_tile();
    const std::array<Tile_coord,4> approaches={Tile_coord(at.tx-1,at.ty+1,at.tz),Tile_coord(at.tx+1,at.ty-1,at.tz),
        Tile_coord(at.tx-bench->get_info().get_3d_xtiles(bench->get_framenum()),at.ty-1,at.tz),
        Tile_coord(at.tx-1,at.ty-bench->get_info().get_3d_ytiles(bench->get_framenum()),at.tz)};
    for(const auto& approach:approaches) {
        auto path=std::make_unique<Path_walking_actor_action>(nullptr,6,3);
        Actor_action* route=path->walk_to_tile(npc,npc->get_tile(),approach);
        if(!route)continue;
        if(route==path.get())path.release();
        npc->set_action(new Guild_delivery(route,bench,approach,surface));npc->start(200,0);return true;
    }
    return false;
}
