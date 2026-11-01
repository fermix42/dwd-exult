#game "blackgate"
// Built-in quiz slider correction. Mods retain normal override authority.
// Only the initial numeric selection differs from the original helper.
// Explicit utility IDs preserve the original zero-argument calling convention;
// the Fellowship helper still obtains its NPC through item (the caller itemref).
var askMapQuiz 0x886 () {
    var v0, v1, v2, v5, v6, v25, v26, v29, v30;

    v0 = [ 0, 0, 0 ];
    v1 = [ 1, 2, 3 ];
    v2 = false;
    if (!gflags[42] && (!gflags[43] && (!gflags[44] && (!gflags[45] && (!gflags[46] && (!gflags[47] && (!gflags[48] && !gflags[49]))))))) {
        for (v5 in v1) {
            do { v6 = UI_die_roll(1, 8); } while (v6 in v0);
            v0[v5] = v6;
        }
        for (v5 in v1) {
            if (v0[v5] == 1) {
                gflags[42] = true;
            }
        }
        for (v5 in v1) {
            if (v0[v5] == 2) {
                gflags[43] = true;
            }
        }
        for (v5 in v1) {
            if (v0[v5] == 3) {
                gflags[44] = true;
            }
        }
        for (v5 in v1) {
            if (v0[v5] == 4) {
                gflags[45] = true;
            }
        }
        for (v5 in v1) {
            if (v0[v5] == 5) {
                gflags[46] = true;
            }
        }
        for (v5 in v1) {
            if (v0[v5] == 6) {
                gflags[47] = true;
            }
        }
        for (v5 in v1) {
            if (v0[v5] == 7) {
                gflags[48] = true;
            }
        }
        for (v5 in v1) {
            if (v0[v5] == 8) {
                gflags[49] = true;
            }
        }
    } else {
        for (v5 in v1) {
            if (gflags[42] && !(1 in v0)) {
                v0[v5] = 1;
            }
            if (gflags[43] && !(2 in v0)) {
                v0[v5] = 2;
            }
            if (gflags[44] && !(3 in v0)) {
                v0[v5] = 3;
            }
            if (gflags[45] && !(4 in v0)) {
                v0[v5] = 4;
            }
            if (gflags[46] && !(5 in v0)) {
                v0[v5] = 5;
            }
            if (gflags[47] && !(6 in v0)) {
                v0[v5] = 6;
            }
            if (gflags[48] && !(7 in v0)) {
                v0[v5] = 7;
            }
            if (gflags[49] && !(8 in v0)) {
                v0[v5] = 8;
            }
        }
    }
    v25 = [ "What is the latitude of the northern-most point of the island Spektran?", "What longitude runs through the center of the island Buccaneer's Den?", "What longitude runs through the center of the island Terfin?", "What latitude runs through the center of Dagger Isle?", "What latitude runs through the center of Skara Brae?", "What latitude runs through the center of the Deep Forest?", "What latitude runs through the center of Buccaneer's Den?", "What longitude runs through the center of Skara Brae?" ];
    v26 = [ 120, 60, 120, 0, 30, 60, 60, 60 ];
    say("\"Before I give thee the password, I must admit I have had my doubts about thou truly being the Avatar. I must ask thee to indulge me in order to satisfy my somewhat suspicious nature. I shall ask thee a few questions regarding the geography of Britannia. Please answer with the number of the longitude or latitude from thy cloth map. Remember-- longitude refers to the lines that run north-south. They are determined by the numbers at the bottom of the map. Latitude refers to the lines that run east-west. They are determined by the numbers on the left side of thy map. If these questions are answered correctly, then I will cast aside all my doubts.\"");
    for (v5 in v1) {
        v29 = v25[v0[v5]];
        say("\"", v29, "\"");
        v30 = UI_input_numeric_value(0, 210, 5, v26[v0[v5]]);
        if (!(v30 == v26[v0[v5]])) {
            v2 = true;
        }
    }
    if (v2) {
        return false;
    } else {
        return true;
    }
}

void askFellowshipQuiz 0x852 () {
    var v0, v1, v2, v5, v6, v21, v22, v25, v26;

    v0 = [ 0, 0 ];
    v1 = [ 1, 2 ];
    v2 = false;
    if (!gflags[50] && (!gflags[51] && (!gflags[52] && (!gflags[53] && (!gflags[54] && !gflags[55]))))) {
        for (v5 in v1) {
            do { v6 = UI_die_roll(1, 6); } while (v6 in v0);
            v0[v5] = v6;
        }
        for (v5 in v1) {
            if (v0[v5] == 1) {
                gflags[50] = true;
            }
        }
        for (v5 in v1) {
            if (v0[v5] == 2) {
                gflags[51] = true;
            }
        }
        for (v5 in v1) {
            if (v0[v5] == 3) {
                gflags[52] = true;
            }
        }
        for (v5 in v1) {
            if (v0[v5] == 4) {
                gflags[53] = true;
            }
        }
        for (v5 in v1) {
            if (v0[v5] == 5) {
                gflags[54] = true;
            }
        }
        for (v5 in v1) {
            if (v0[v5] == 6) {
                gflags[55] = true;
            }
        }
    } else {
        for (v5 in v1) {
            if (gflags[50] && !(1 in v0)) {
                v0[v5] = 1;
            }
            if (gflags[51] && !(2 in v0)) {
                v0[v5] = 2;
            }
            if (gflags[52] && !(3 in v0)) {
                v0[v5] = 3;
            }
            if (gflags[53] && !(4 in v0)) {
                v0[v5] = 4;
            }
            if (gflags[54] && !(5 in v0)) {
                v0[v5] = 5;
            }
            if (gflags[55] && !(6 in v0)) {
                v0[v5] = 6;
            }
        }
    }
    v21 = [ "According to the Book of Archaic Knowledge, how many times must ginseng be reboiled in order for it to be properly used as a magical reagent?", "How many runes are in the archaic script of the outdated Britannian language?", "According to the Book of Archaic Knowledge, how many places may the Mandrake Root naturally be found?", "In the Book of Fellowship, how many bandits can be seen surrounding on the old man in the illustration on page three?", "According to the Traveller's Companion, how many parts of the body should one wish to protect with armour?", "According to the Book of Archaic Knowledge, fewer than how many pearls in 10,000 are black?" ];
    v22 = [ 40, 31, 2, 6, 6, 1 ];
    for (v5 in v1) {
        v25 = v21[v0[v5]];
        say("\"", v25, "\"");
        v26 = UI_input_numeric_value(0, 60, 1, v22[v0[v5]]);
        if (!(v26 == v22[v0[v5]])) {
            v2 = true;
        }
    }
    if (v2) {
        UI_set_item_flag(item, 25);
        gflags[56] = true;
    } else {
        UI_clear_item_flag(item, 25);
        gflags[56] = false;
    }
}
